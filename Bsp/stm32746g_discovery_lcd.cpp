// stm32746g_discovery_lcd.cpp
//{{{  includes
#include "stm32746g_discovery_lcd.h"

#include "cmsis_os.h"

#include <ft2build.h>
#include FT_FREETYPE_H

extern int freeSansBold_len;
extern const uint8_t freeSansBold[64228];
//}}}
//{{{  defines
#define ABS(X) ((X) > 0 ? (X) : -(X))

#define LCD_DISP_PIN           GPIO_PIN_12
#define LCD_DISP_GPIO_PORT     GPIOI

#define LCD_BL_CTRL_PIN        GPIO_PIN_3
#define LCD_BL_CTRL_GPIO_PORT  GPIOK

// DMA2D CR reg bits
#define DMA2D_CR_MASK      ((uint32_t)0xFFFC3F07)  // unused bits mask
#define DMA2D_CR_START     ((uint32_t)0x00000001)  // Start transfer
#define DMA2D_CR_SUSP      ((uint32_t)0x00000002)  // Suspend transfer
#define DMA2D_CR_ABORT     ((uint32_t)0x00000004)  // Abort transfer
#define DMA2D_CR_TEIE      ((uint32_t)0x00000100)  // Transfer Error Interrupt Enable
#define DMA2D_CR_TCIE      ((uint32_t)0x00000200)  // Transfer Complete Interrupt Enable
#define DMA2D_CR_TWIE      ((uint32_t)0x00000400)  // Transfer Watermark Interrupt Enable
#define DMA2D_CR_CAEIE     ((uint32_t)0x00000800)  // CLUT Access Error Interrupt Enable
#define DMA2D_CR_CTCIE     ((uint32_t)0x00001000)  // CLUT Transfer Complete Interrupt Enable
#define DMA2D_CR_CEIE      ((uint32_t)0x00002000)  // Configuration Error Interrupt Enable
#define DMA2D_CR_MODE      ((uint32_t)0x00030000)  // DMA2D Mode mask
#define DMA2D_CR_M2M       ((uint32_t)0x00000000)  // - DMA2D memory to memory transfer mode
#define DMA2D_CR_M2M_PFC   ((uint32_t)0x00010000)  // - DMA2D memory to memory with pixel format conversion transfer mode
#define DMA2D_CR_M2M_BLEND ((uint32_t)0x00020000)  // - DMA2D memory to memory with blending transfer mode
#define DMA2D_CR_R2M       ((uint32_t)0x00030000)  // - DMA2D register to memory transfer mode

#define DMA2D_ARGB8888     ((uint32_t)0x00000000)  // ARGB8888 DMA2D color mode
#define DMA2D_RGB888       ((uint32_t)0x00000001)  // RGB888 DMA2D color mode
#define DMA2D_RGB565       ((uint32_t)0x00000002)  // RGB565 DMA2D color mode
#define DMA2D_ARGB1555     ((uint32_t)0x00000003)  // ARGB1555 DMA2D color mode
#define DMA2D_ARGB4444     ((uint32_t)0x00000004)  // ARGB4444 DMA2D color mode

#define CM_ARGB8888        ((uint32_t)0x00000000)  // ARGB8888 color mode
#define CM_RGB888          ((uint32_t)0x00000001)  // RGB888 color mode
#define CM_RGB565          ((uint32_t)0x00000002)  // RGB565 color mode
#define CM_ARGB1555        ((uint32_t)0x00000003)  // ARGB1555 color mode
#define CM_ARGB4444        ((uint32_t)0x00000004)  // ARGB4444 color mode
#define CM_L8              ((uint32_t)0x00000005)  // L8 color mode
#define CM_AL44            ((uint32_t)0x00000006)  // AL44 color mode
#define CM_AL88            ((uint32_t)0x00000007)  // AL88 color mode
#define CM_L4              ((uint32_t)0x00000008)  // L4 color mode
#define CM_A8              ((uint32_t)0x00000009)  // A8 color mode
#define CM_A4              ((uint32_t)0x0000000A)  // A4 color mode
//}}}
//{{{  struct tLTDC
typedef struct {
  osSemaphoreId sem;
  uint32_t timeouts;
  uint32_t lineIrq;
  uint32_t fifoUnderunIrq;
  uint32_t transferErrorIrq;
  uint32_t lastTicks;
  uint32_t lineTicks;
  uint32_t frameWait;
  } tLTDC;
//}}}
//{{{  struct tDma2d
typedef struct {
  osSemaphoreId sem;
  uint32_t* isrBuf;
  uint32_t* curBuf;
  uint32_t* highWater;
  uint32_t buf[5000];  // should overflow into counts first
  uint32_t irqTeCount;
  uint32_t irqCeCount;
  uint32_t irqTcCount;
  uint32_t timeouts;
  } tDma2d;
//}}}
//{{{  struct tFontChar
typedef struct {
  uint8_t* bitmap;
  int16_t left;
  int16_t top;
  int16_t pitch;
  int16_t rows;
  int16_t advance;
  } tFontChar;
//}}}
//{{{  static vars
static uint32_t curFrameBufferAddress;
static uint32_t setFrameBufferAddress[2];
static uint32_t showFrameBufferAddress[2];
static uint8_t showAlpha[2];

static tLTDC ltdc;
static tDma2d dma;

#define maxChars 0x60
static tFontChar* chars[maxChars];

static FT_Library FTlibrary;
static FT_Face FTface;
static FT_GlyphSlot FTglyphSlot;
//}}}
LTDC_HandleTypeDef hLtdc;

//{{{
void LCD_LTDC_IRQHandler() {

  if (LTDC->ISR & LTDC_IT_FU) ltdc.fifoUnderunIrq++;
  if (LTDC->ISR & LTDC_IT_TE) ltdc.transferErrorIrq++;

  // line interrupt
  if (LTDC->ISR & LTDC_IT_LI) {
    // switch showFrameBuffer
    LTDC_Layer_TypeDef* ltdcLayer = (LTDC_Layer_TypeDef*)((uint32_t)LTDC + 0x84); // + (0x80*layer));
    ltdcLayer->CFBAR = showFrameBufferAddress[0];
    if (showAlpha[0]) {
      ltdcLayer->CR |= LTDC_LxCR_LEN;
      ltdcLayer->CACR &= ~LTDC_LxCACR_CONSTA;
      ltdcLayer->CACR = showAlpha[0];
      }
    else
      ltdcLayer->CR &= ~LTDC_LxCR_LEN;
    LTDC->SRCR |= LTDC_SRCR_IMR;

    ltdc.lineTicks = osKernelSysTick() - ltdc.lastTicks;
    ltdc.lastTicks = osKernelSysTick();
    ltdc.lineIrq++;

    if (ltdc.frameWait)
      osSemaphoreRelease (ltdc.sem);
    ltdc.frameWait = 0;
    }

  LTDC->ICR = LTDC_FLAG_LI | LTDC_FLAG_FU | LTDC_FLAG_TE;
  }
//}}}
//{{{
void LCD_DMA2D_IRQHandler() {

  // debug count interrupts
  if (DMA2D->ISR & DMA2D_ISR_TEIF) dma.irqTeCount++;
  if (DMA2D->ISR & DMA2D_ISR_CEIF) dma.irqCeCount++;
  if (DMA2D->ISR & DMA2D_ISR_TCIF) dma.irqTcCount++;

  // clear interrupt flags
  DMA2D->IFCR = DMA2D_ISR_TCIF | DMA2D_ISR_TEIF | DMA2D_ISR_CEIF;

  switch (*dma.isrBuf) {
    case 1: // fill
      dma.isrBuf++;
      DMA2D->OCOLR = *dma.isrBuf++;  // colour
      DMA2D->OMAR  = *dma.isrBuf++;  // fb start address
      DMA2D->OOR   = *dma.isrBuf++;  // stride
      DMA2D->NLR   = *dma.isrBuf++;  // xlen:ylen
      DMA2D->CR = DMA2D_CR_R2M | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
      break;

    case 2: // stamp
      dma.isrBuf++;
      DMA2D->FGCOLR = *dma.isrBuf++; // src color
      DMA2D->OMAR   = *dma.isrBuf;   // bgnd fb start address
      DMA2D->BGMAR  = *dma.isrBuf++;
      DMA2D->OOR    = *dma.isrBuf;   // bgnd stride
      DMA2D->BGOR   = *dma.isrBuf++;
      DMA2D->NLR    = *dma.isrBuf++; // xlen:ylen
      DMA2D->FGMAR  = *dma.isrBuf++; // src start address
      DMA2D->CR = DMA2D_CR_M2M_BLEND | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
      break;

    default: // normally 0
      // no more opCodes, disable interrupts and release semaphore to signal done
      DMA2D->CR = 0;
      dma.isrBuf = dma.buf;
      osSemaphoreRelease (dma.sem);
      break;
    }
  }
//}}}

//{{{
void lcdInit (uint32_t frameBufferAddress) {

  //{{{  LTDC, dma2d clock enable
  // enable the LTDC and DMA2D clocks
  __HAL_RCC_LTDC_CLK_ENABLE();
  __HAL_RCC_DMA2D_CLK_ENABLE();

  // enable GPIO clock, includes backlight, display enable
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  //}}}
  //{{{  LTDC pin config
  // GPIOE config
  GPIO_InitTypeDef gpio_init_structure;
  gpio_init_structure.Pin       = GPIO_PIN_4;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  gpio_init_structure.Speed     = GPIO_SPEED_FAST;
  gpio_init_structure.Alternate = GPIO_AF14_LTDC;
  HAL_GPIO_Init (GPIOE, &gpio_init_structure);

  // GPIOG config
  gpio_init_structure.Pin       = GPIO_PIN_12;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Alternate = GPIO_AF9_LTDC;
  HAL_GPIO_Init (GPIOG, &gpio_init_structure);

  // GPIOI LTDC alternate config
  gpio_init_structure.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Alternate = GPIO_AF14_LTDC;
  HAL_GPIO_Init (GPIOI, &gpio_init_structure);

  // GPIOJ config
  gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                                  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                                  GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Alternate = GPIO_AF14_LTDC;
  HAL_GPIO_Init (GPIOJ, &gpio_init_structure);

  // GPIOK config
  gpio_init_structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Alternate = GPIO_AF14_LTDC;
  HAL_GPIO_Init (GPIOK, &gpio_init_structure);

  // LCD_DISP GPIO config
  gpio_init_structure.Pin       = LCD_DISP_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init (LCD_DISP_GPIO_PORT, &gpio_init_structure);

  // LCD_BL_CTRL GPIO config
  gpio_init_structure.Pin       = LCD_BL_CTRL_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init (LCD_BL_CTRL_GPIO_PORT, &gpio_init_structure);
  //}}}

  //{{{  LTDC timing config
  #define RK043FN48H_WIDTH   480 // LCD PIXEL WIDTH
  #define RK043FN48H_HSYNC   41  // Horizontal synchronization
  #define RK043FN48H_HBP     13  // Horizontal back porch
  #define RK043FN48H_HFP     32  // Horizontal front porch

  #define RK043FN48H_HEIGHT  272 // LCD PIXEL HEIGHT
  #define RK043FN48H_VSYNC   10  // Vertical synchronization
  #define RK043FN48H_VBP     2   // Vertical back porch
  #define RK043FN48H_VFP     2   // Vertical front porch

  hLtdc.Init.HorizontalSync =     RK043FN48H_HSYNC - 1;
  hLtdc.Init.AccumulatedHBP =     RK043FN48H_HSYNC + RK043FN48H_HBP - 1;
  hLtdc.Init.AccumulatedActiveW = RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP - 1;
  hLtdc.Init.TotalWidth =         RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP + RK043FN48H_HFP - 1;

  hLtdc.Init.VerticalSync =       RK043FN48H_VSYNC - 1;
  hLtdc.Init.AccumulatedVBP =     RK043FN48H_VSYNC + RK043FN48H_VBP - 1;
  hLtdc.Init.AccumulatedActiveH = RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP - 1;
  hLtdc.Init.TotalHeigh =         RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP + RK043FN48H_VFP - 1;

  // - PLLSAI_VCO In  = HSE_VALUE / PLLM                        = 1 Mhz
  // - PLLSAI_VCO Out = PLLSAI_VCO Input * PLLSAIN              = 192 Mhz
  // - PLLLCDCLK      = PLLSAI_VCO Output / PLLSAIR    = 192/5  = 38.4 Mhz
  // - LTDC clock     = PLLLCDCLK / LTDC_PLLSAI_DIVR_4 = 38.4/4 = 9.6Mhz
  RCC_PeriphCLKInitTypeDef periph_clk_init_struct;
  periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  periph_clk_init_struct.PLLSAI.PLLSAIN = 192;
  periph_clk_init_struct.PLLSAI.PLLSAIR = 5; // could be 7
  periph_clk_init_struct.PLLSAIDivR = RCC_PLLSAIDIVR_4;
  HAL_RCCEx_PeriphCLKConfig (&periph_clk_init_struct);
  //}}}
  //{{{  LTDC Polarity
  hLtdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hLtdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hLtdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hLtdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  //}}}
  //{{{  LTDC width height
  hLtdc.LayerCfg->ImageWidth = RK043FN48H_WIDTH;
  hLtdc.LayerCfg->ImageHeight = RK043FN48H_HEIGHT;
  //}}}
  //{{{  LTDC Background value
  hLtdc.Init.Backcolor.Blue = 0;
  hLtdc.Init.Backcolor.Green = 0;
  hLtdc.Init.Backcolor.Red = 0;
  //}}}
  hLtdc.Instance = LTDC;
  HAL_LTDC_Init (&hLtdc);

  lcdLayerInit (0, frameBufferAddress);
  setFrameBufferAddress[1] = frameBufferAddress;
  showFrameBufferAddress[1] = frameBufferAddress;
  showAlpha[1] = 0;

  //{{{  LTDC IRQ init
  osSemaphoreDef (ltdcSem);
  ltdc.sem = osSemaphoreCreate (osSemaphore (ltdcSem), -1);
  ltdc.timeouts = 0;
  ltdc.lineIrq = 0;
  ltdc.fifoUnderunIrq = 0;
  ltdc.transferErrorIrq = 0;
  ltdc.lastTicks = 0;
  ltdc.lineTicks = 0;
  ltdc.frameWait = 0;

  HAL_NVIC_SetPriority (LTDC_IRQn, 0xE, 0);
  HAL_NVIC_EnableIRQ (LTDC_IRQn);

  // set line interupt line number
  LTDC->LIPCR = 0;

  // enable line interrupt
  LTDC->IER |= LTDC_IT_LI;
  //}}}
  //{{{  dma2d IRQ init
  // unchanging dma2d regs
  DMA2D->OPFCCR  = DMA2D_ARGB8888; // bgnd fb ARGB
  DMA2D->BGPFCCR = DMA2D_ARGB8888;
  DMA2D->FGPFCCR = CM_A8;          // src alpha
  DMA2D->FGOR    = 0;              // src stride
  //DMA2D->AMTCR = 0x1001;

  osSemaphoreDef (dma2dSem);
  dma.sem = osSemaphoreCreate (osSemaphore (dma2dSem), -1);

  // zero out first opcode, point past it
  *dma.buf = 0;
  dma.curBuf = dma.buf+1;
  dma.highWater = dma.curBuf;

  dma.isrBuf = dma.buf;

  dma.irqTeCount = 0;
  dma.irqCeCount = 0;
  dma.irqTcCount = 0;
  dma.timeouts = 0;

  HAL_NVIC_SetPriority (DMA2D_IRQn, 0x05, 0);
  HAL_NVIC_EnableIRQ (DMA2D_IRQn);
  //}}}

  // font init
  for (auto i = 0; i < maxChars; i++)
    chars[i] = nullptr;
  lcdSetFont (freeSansBold, freeSansBold_len);
  }
//}}}
//{{{
void lcdLayerInit (uint8_t layer, uint32_t frameBufferAddress) {

  LTDC_LayerCfgTypeDef* curLayerCfg = &hLtdc.LayerCfg[layer];

  curLayerCfg->WindowX0 = 0;
  curLayerCfg->WindowX1 = lcdGetXSize();
  curLayerCfg->WindowY0 = 0;
  curLayerCfg->WindowY1 = lcdGetYSize();

  curLayerCfg->PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
  curLayerCfg->FBStartAdress = (uint32_t)frameBufferAddress;

  curLayerCfg->Alpha = 255;
  curLayerCfg->Alpha0 = 0;

  curLayerCfg->Backcolor.Blue = 0;
  curLayerCfg->Backcolor.Green = 0;
  curLayerCfg->Backcolor.Red = 0;

  curLayerCfg->BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  curLayerCfg->BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;

  curLayerCfg->ImageWidth = lcdGetXSize();
  curLayerCfg->ImageHeight = lcdGetYSize();

  HAL_LTDC_ConfigLayer (&hLtdc, curLayerCfg, layer);

  // local state
  curFrameBufferAddress = frameBufferAddress;
  setFrameBufferAddress[layer] = frameBufferAddress;
  showFrameBufferAddress[layer] = frameBufferAddress;
  showAlpha[layer] = 255;
  }
//}}}
//{{{
void lcdDisplayOn() {

  // enable LTDC
  LTDC->GCR |= LTDC_GCR_LTDCEN;

  // turn on DISP pin
  HAL_GPIO_WritePin (LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_SET);        /* Assert LCD_DISP pin */

  // turn on backlight
  HAL_GPIO_WritePin (LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);  /* Assert LCD_BL_CTRL pin */
  }
//}}}
//{{{
void lcdDisplayOff() {

  // disable LTDC
  LTDC->GCR &= ~LTDC_GCR_LTDCEN;

  // turn off DISP pin
  HAL_GPIO_WritePin (LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_RESET);      /* De-assert LCD_DISP pin */

  // turn off backlight
  HAL_GPIO_WritePin (LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_RESET);/* De-assert LCD_BL_CTRL pin */
  }
//}}}

//{{{
void lcdDebug (int16_t y) {

  char debugStr [80];
  sprintf (debugStr, "%d %d %d %d %d %d %d %d %d",
           (int)ltdc.lineIrq, (int)ltdc.lineTicks,
           (int)(dma.highWater-dma.buf), (int)dma.irqTeCount, (int)dma.irqCeCount, (int)dma.irqTcCount, (int)dma.timeouts,
           (int)ltdc.transferErrorIrq, (int)ltdc.fifoUnderunIrq);
  lcdString (LCD_WHITE, 20, debugStr, 0, y, lcdGetXSize(), 24);
  }
//}}}

//{{{
void lcdSetLayer (uint8_t layer, uint32_t frameBufferAddress) {

  curFrameBufferAddress = frameBufferAddress;
  setFrameBufferAddress[layer] = frameBufferAddress;
  }
//}}}
//{{{
void lcdShowLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha) {

  showFrameBufferAddress[layer] = frameBufferAddress;
  showAlpha[layer] = alpha;
  }
//}}}
//{{{
void lcdFrameSync() {

  ltdc.frameWait = 1;
  if (osSemaphoreWait (ltdc.sem, 100) != osOK)
    ltdc.timeouts++;
  }
//}}}

//{{{
void lcdSetFont (const uint8_t* font, int length) {

  FT_Init_FreeType (&FTlibrary);
  FT_New_Memory_Face (FTlibrary, (FT_Byte*)font, length, 0, &FTface);
  FTglyphSlot = FTface->glyph;
  //FT_Done_Face(face);
  //FT_Done_FreeType (library);
  }
//}}}

//{{{
void lcdStamp (uint32_t col, uint8_t* src, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen) {

  if (xlen && ylen) {
    *dma.curBuf++ = col;                                                         // colour
    *dma.curBuf++ = curFrameBufferAddress + ((y * lcdGetXSize()) + x) * 4;  // bgnd fb start address
    *dma.curBuf++ = lcdGetXSize() - xlen;                                        // stride
    *dma.curBuf++ = (xlen << 16) | ylen;                                         // xlen:ylen
    *dma.curBuf++ = (uint32_t)src;                                               // src start address
    *dma.curBuf++ = 0;                                                           // terminate
    *(dma.curBuf-7) = 2;                                                         // stamp opCode

    if (dma.curBuf > dma.highWater)
      dma.highWater = dma.curBuf;
    }
  }
//}}}
//{{{
int lcdString (uint32_t col, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen) {

  for (unsigned int i = 0; i < str.size(); i++) {
    if ((str[i] >= 0x20) && (str[i] <= 0x7F)) {
      auto fontChar = chars[str[i] - 0x20];
      if (fontChar == nullptr) {
        FT_Set_Pixel_Sizes (FTface, 0, fontHeight);
        FT_Load_Char (FTface, str[i], FT_LOAD_RENDER);

        // cache char info
        fontChar = (tFontChar*)malloc (sizeof(tFontChar));
        chars[str[i] - 0x20] = fontChar;
        fontChar->left = FTglyphSlot->bitmap_left;
        fontChar->top = FTglyphSlot->bitmap_top;
        fontChar->pitch = FTglyphSlot->bitmap.pitch;
        fontChar->rows = FTglyphSlot->bitmap.rows;
        fontChar->advance = FTglyphSlot->advance.x / 64;
        fontChar->bitmap = nullptr;

        if (FTglyphSlot->bitmap.buffer) {
          // cache char bitmap
          fontChar->bitmap = (uint8_t*)malloc (FTglyphSlot->bitmap.pitch * FTglyphSlot->bitmap.rows);
          memcpy (fontChar->bitmap, FTglyphSlot->bitmap.buffer, FTglyphSlot->bitmap.pitch * FTglyphSlot->bitmap.rows);
          }
        }

      if (x + fontChar->left + fontChar->pitch > lcdGetXSize())
        break;
      else if (fontChar->bitmap)
        lcdStamp (col, fontChar->bitmap, x + fontChar->left, y + fontHeight - fontChar->top, fontChar->pitch, fontChar->rows);
      x += fontChar->advance;
      }
    }

  return x;
  }
//}}}

//{{{
void lcdRect (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen) {

  if (!xlen)
    return;
  if (!ylen)
    return;

  *dma.curBuf++ = col;                                                         // colour
  *dma.curBuf++ = curFrameBufferAddress + ((y * lcdGetXSize()) + x) * 4;  // fb start address
  *dma.curBuf++ = lcdGetXSize() - xlen;                                        // stride
  *dma.curBuf++ = (xlen << 16) | ylen;                                         // xlen:ylen
  *dma.curBuf++ = 0;                                                           // terminate
  *(dma.curBuf-6) = 1;                                                         // fill opCode

  if (dma.curBuf > dma.highWater)
    dma.highWater = dma.curBuf;
  }
//}}}
//{{{
void lcdClipRect (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen) {

  if (x >= lcdGetXSize())
    return;
  if (y >= lcdGetYSize())
    return;

  int xend = x + xlen;
  if (xend <= 0)
    return;

  int yend = y + ylen;
  if (yend <= 0)
    return;

  if (x < 0)
    x = 0;
  if (xend > lcdGetXSize())
    xend = lcdGetXSize();

  if (y < 0)
    y = 0;
  if (yend > lcdGetYSize())
    yend = lcdGetYSize();

  lcdRect (col, x, y, xend - x, yend - y);
  }
//}}}
//{{{
void lcdEllipse (uint32_t col, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

  if (!xradius)
    return;
  if (!yradius)
    return;

  int x1 = 0;
  int y1 = -yradius;
  int err = 2 - 2*xradius;
  float k = (float)yradius / xradius;

  do {
    lcdClipRect (col, (x-(uint16_t)(x1 / k)), y + y1, (2*(uint16_t)(x1 / k) + 1), 1);
    lcdClipRect (col, (x-(uint16_t)(x1 / k)), y - y1, (2*(uint16_t)(x1 / k) + 1), 1);

    int e2 = err;
    if (e2 <= x1) {
      err += ++x1 * 2 + 1;
      if (-y1 == x && e2 <= y1)
        e2 = 0;
      }
    if (e2 > y1)
      err += ++y1*2 + 1;
    } while (y1 <= 0);
  }
//}}}
//{{{
void lcdClear (uint32_t col) {

  lcdRect (col, 0, 0, lcdGetXSize(), lcdGetYSize());
  }
//}}}

//{{{
void lcdPixel (uint32_t col, int16_t x, int16_t y) {

  *(uint32_t*)(curFrameBufferAddress + (y*lcdGetXSize() + x)*4) = col;
  }
//}}}
//{{{
void lcdClipPixel (uint32_t col, int16_t x, int16_t y) {

  if ((x >= 0) && (y > 0) && (x < lcdGetXSize()) && (y < lcdGetYSize()))
    *(uint32_t*)(curFrameBufferAddress + (y*lcdGetXSize() + x)*4) = col;
  }
//}}}
//{{{
void lcdLine (uint32_t col, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {

  int16_t deltax = ABS(x2 - x1);        /* The difference between the x's */
  int16_t deltay = ABS(y2 - y1);        /* The difference between the y's */
  int16_t x = x1;                       /* Start x off at the first pixel */
  int16_t y = y1;                       /* Start y off at the first pixel */

  int16_t xinc1;
  int16_t xinc2;
  if (x2 >= x1) {               /* The x-values are increasing */
    xinc1 = 1;
    xinc2 = 1;
    }
  else {                         /* The x-values are decreasing */
    xinc1 = -1;
    xinc2 = -1;
    }

  int yinc1;
  int yinc2;
  if (y2 >= y1) {                 /* The y-values are increasing */
    yinc1 = 1;
    yinc2 = 1;
    }
  else {                         /* The y-values are decreasing */
    yinc1 = -1;
    yinc2 = -1;
    }

  int den = 0;
  int num = 0;
  int num_add = 0;
  int num_pixels = 0;
  if (deltax >= deltay) {        /* There is at least one x-value for every y-value */
    xinc1 = 0;                  /* Don't change the x when numerator >= denominator */
    yinc2 = 0;                  /* Don't change the y for every iteration */
    den = deltax;
    num = deltax / 2;
    num_add = deltay;
    num_pixels = deltax;         /* There are more x-values than y-values */
    }
  else {                         /* There is at least one y-value for every x-value */
    xinc2 = 0;                  /* Don't change the x for every iteration */
    yinc1 = 0;                  /* Don't change the y when numerator >= denominator */
    den = deltay;
    num = deltay / 2;
    num_add = deltax;
    num_pixels = deltay;         /* There are more y-values than x-values */
  }

  for (int curpixel = 0; curpixel <= num_pixels; curpixel++) {
    lcdClipPixel (col, x, y);   /* Draw the current pixel */
    num += num_add;                            /* Increase the numerator by the top of the fraction */
    if (num >= den) {                          /* Check if numerator >= denominator */
      num -= den;                             /* Calculate the new numerator value */
      x += xinc1;                             /* Change the x as appropriate */
      y += yinc1;                             /* Change the y as appropriate */
      }
    x += xinc2;                               /* Change the x as appropriate */
    y += yinc2;                               /* Change the y as appropriate */
    }
  }
//}}}
//{{{
void lcdOutRect (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen) {

  lcdClipRect (col, x, y, xlen, 1);
  lcdClipRect (col, x + xlen, y, 1, ylen);
  lcdClipRect (col, x, y + ylen, xlen, 1);
  lcdClipRect (col, x, y, 1, ylen);
  }
//}}}
//{{{
void lcdOutEllipse (uint32_t col, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

  if (xradius && yradius) {
    int x1 = 0;
    int y1 = -yradius;
    int err = 2 - 2*xradius;
    float k = (float)yradius / xradius;

    do {
      lcdClipPixel (col, x - (uint16_t)(x1 / k), y + y1);
      lcdClipPixel (col, x + (uint16_t)(x1 / k), y + y1);
      lcdClipPixel (col, x + (uint16_t)(x1 / k), y - y1);
      lcdClipPixel (col, x - (uint16_t)(x1 / k), y - y1);

      int e2 = err;
      if (e2 <= x1) {
        err += ++x1*2 + 1;
        if (-y1 == x1 && e2 <= y1)
          e2 = 0;
        }
      if (e2 > y1)
        err += ++y1*2 + 1;
      } while (y1 <= 0);
    }
  }
//}}}

//{{{
void lcdSend() {

  // send first opCode using IRQhandler
  LCD_DMA2D_IRQHandler();
  }
//}}}
//{{{
void lcdWait() {

  if (osSemaphoreWait (dma.sem, 500) != osOK)
    dma.timeouts++;

  // zero out first opcode, point past it
  *dma.buf = 0;
  dma.curBuf = dma.buf+1;
  }
//}}}
//{{{
void lcdSendWait() {

  lcdSend();
  lcdWait();
  }
//}}}
