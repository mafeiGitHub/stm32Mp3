// cLcd.cpp
//#define USE_RGB888
//{{{  includes
#include <stdlib.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>

#include "memory.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cpuUsage.h"

#include "stm32f7xx_hal.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery.h"
#else
  #include "stm32F769i_discovery.h"
#endif

#include "stm32f7xx_hal_dma2d.h"

#include "utils.h"

#include "cLcd.h"
#include "cLcdPrivate.h"

#include "widgets/cWidget.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "FreeSansBold.h"
//}}}
//{{{  defines
#define ABS(X) ((X) > 0 ? (X) : -(X))

#define LCD_DISP_PIN           GPIO_PIN_12
#define LCD_DISP_GPIO_PORT     GPIOI

#define LCD_BL_CTRL_PIN        GPIO_PIN_3
#define LCD_BL_CTRL_GPIO_PORT  GPIOK

#define kEnd             0
#define kStamp           1

#ifdef USE_RGB888
  const static int dstComponents = 3;
#else
  const static int dstComponents = 4;
#endif

// DSI lcd
//{{{  OTM8009A defines
#define OTM8009A_ORIENTATION_PORTRAIT    ((uint32_t)0x00) /* Portrait orientation choice of LCD screen  */
#define OTM8009A_ORIENTATION_LANDSCAPE   ((uint32_t)0x01) /* Landscape orientation choice of LCD screen */

#define OTM8009A_FORMAT_RGB888    ((uint32_t)0x00) /* Pixel format chosen is RGB888 : 24 bpp */
#define OTM8009A_FORMAT_RBG565    ((uint32_t)0x02) /* Pixel format chosen is RGB565 : 16 bpp */

/* Width and Height in Portrait mode */
#define  OTM8009A_480X800_WIDTH             ((uint16_t)480)     /* LCD PIXEL WIDTH   */
#define  OTM8009A_480X800_HEIGHT            ((uint16_t)800)     /* LCD PIXEL HEIGHT  */

/* Width and Height in Landscape mode */
#define  OTM8009A_800X480_WIDTH             ((uint16_t)800)     /* LCD PIXEL WIDTH   */
#define  OTM8009A_800X480_HEIGHT            ((uint16_t)480)     /* LCD PIXEL HEIGHT  */

// OTM8009A_480X800 Timing parameters for Portrait orientation mode
#define  OTM8009A_480X800_HSYNC             ((uint16_t)63)     /* Horizontal synchronization: This value is set to limit value mentionned
                                                                   in otm8009a spec to fit with USB functional clock configuration constraints */
#define  OTM8009A_480X800_HBP               ((uint16_t)120)     /* Horizontal back porch      */
#define  OTM8009A_480X800_HFP               ((uint16_t)120)     /* Horizontal front porch     */
#define  OTM8009A_480X800_VSYNC             ((uint16_t)12)      /* Vertical synchronization   */
#define  OTM8009A_480X800_VBP               ((uint16_t)12)      /* Vertical back porch        */
#define  OTM8009A_480X800_VFP               ((uint16_t)12)      /* Vertical front porch       */

// OTM8009A_800X480 Timing parameters for Landscape orientation mode
//         Same values as for Portrait mode in fact.
#define  OTM8009A_800X480_HSYNC             OTM8009A_480X800_HSYNC  /* Horizontal synchronization */
#define  OTM8009A_800X480_HBP               OTM8009A_480X800_HBP    /* Horizontal back porch      */
#define  OTM8009A_800X480_HFP               OTM8009A_480X800_HFP    /* Horizontal front porch     */
#define  OTM8009A_800X480_VSYNC             OTM8009A_480X800_VSYNC  /* Vertical synchronization   */
#define  OTM8009A_800X480_VBP               OTM8009A_480X800_VBP    /* Vertical back porch        */
#define  OTM8009A_800X480_VFP               OTM8009A_480X800_VFP    /* Vertical front porch       */

/* List of OTM8009A used commands                                  */
/* Detailed in OTM8009A Data Sheet 'DATA_SHEET_OTM8009A_V0 92.pdf' */
/* Version of 14 June 2012                                         */
#define  OTM8009A_CMD_NOP                   0x00  /* NOP command      */
#define  OTM8009A_CMD_SWRESET               0x01  /* Sw reset command */
#define  OTM8009A_CMD_RDDMADCTL             0x0B  /* Read Display MADCTR command : read memory display access ctrl */
#define  OTM8009A_CMD_RDDCOLMOD             0x0C  /* Read Display pixel format */
#define  OTM8009A_CMD_SLPIN                 0x10  /* Sleep In command */
#define  OTM8009A_CMD_SLPOUT                0x11  /* Sleep Out command */
#define  OTM8009A_CMD_PTLON                 0x12  /* Partial mode On command */

#define  OTM8009A_CMD_DISPOFF               0x28  /* Display Off command */
#define  OTM8009A_CMD_DISPON                0x29  /* Display On command */
#define  OTM8009A_CMD_CASET                 0x2A  /* Column address set command */
#define  OTM8009A_CMD_PASET                 0x2B  /* Page address set command */
#define  OTM8009A_CMD_RAMWR                 0x2C  /* Memory (GRAM) write command */
#define  OTM8009A_CMD_RAMRD                 0x2E  /* Memory (GRAM) read command  */

#define  OTM8009A_CMD_PLTAR                 0x30  /* Partial area command (4 parameters) */
#define  OTM8009A_CMD_TEOFF                 0x34  /* Tearing Effect Line Off command : command with no parameter */
#define  OTM8009A_CMD_TEEON                 0x35  /* Tearing Effect Line On command : command with 1 parameter 'TELOM' */

/* Parameter TELOM : Tearing Effect Line Output Mode : possible values */
#define OTM8009A_TEEON_TELOM_VBLANKING_INFO_ONLY            0x00
#define OTM8009A_TEEON_TELOM_VBLANKING_AND_HBLANKING_INFO   0x01

#define  OTM8009A_CMD_MADCTR                0x36  /* Memory Access write control command  */

/* Possible used values of MADCTR */
#define OTM8009A_MADCTR_MODE_PORTRAIT       0x00
#define OTM8009A_MADCTR_MODE_LANDSCAPE      0x60  /* MY = 0, MX = 1, MV = 1, ML = 0, RGB = 0 */

#define  OTM8009A_CMD_IDMOFF                0x38  /* Idle mode Off command */
#define  OTM8009A_CMD_IDMON                 0x39  /* Idle mode On command  */

#define  OTM8009A_CMD_COLMOD                0x3A  /* Interface Pixel format command */

/* Possible values of COLMOD parameter corresponding to used pixel formats */
#define  OTM8009A_COLMOD_RGB565             0x55
#define  OTM8009A_COLMOD_RGB888             0x77

#define  OTM8009A_CMD_RAMWRC                0x3C  /* Memory write continue command */
#define  OTM8009A_CMD_RAMRDC                0x3E  /* Memory read continue command  */

#define  OTM8009A_CMD_WRTESCN               0x44  /* Write Tearing Effect Scan line command */
#define  OTM8009A_CMD_RDSCNL                0x45  /* Read  Tearing Effect Scan line command */

/* CABC Management : ie : Content Adaptive Back light Control in IC OTM8009a */
#define  OTM8009A_CMD_WRDISBV               0x51  /* Write Display Brightness command          */
#define  OTM8009A_CMD_WRCTRLD               0x53  /* Write CTRL Display command                */
#define  OTM8009A_CMD_WRCABC                0x55  /* Write Content Adaptive Brightness command */
#define  OTM8009A_CMD_WRCABCMB              0x5E  /* Write CABC Minimum Brightness command     */

#define OTM8009A_480X800_FREQUENCY_DIVIDER  2   /* LCD Frequency divider      */
//}}}
//{{{  shortRegData const
const uint8_t ShortRegData1[]  = {OTM8009A_CMD_NOP, 0x00};
const uint8_t ShortRegData2[]  = {OTM8009A_CMD_NOP, 0x80};
const uint8_t ShortRegData3[]  = {0xC4, 0x30};
const uint8_t ShortRegData4[]  = {OTM8009A_CMD_NOP, 0x8A};
const uint8_t ShortRegData5[]  = {0xC4, 0x40};
const uint8_t ShortRegData6[]  = {OTM8009A_CMD_NOP, 0xB1};
const uint8_t ShortRegData7[]  = {0xC5, 0xA9};
const uint8_t ShortRegData8[]  = {OTM8009A_CMD_NOP, 0x91};
const uint8_t ShortRegData9[]  = {0xC5, 0x34};
const uint8_t ShortRegData10[] = {OTM8009A_CMD_NOP, 0xB4};
const uint8_t ShortRegData11[] = {0xC0, 0x50};
const uint8_t ShortRegData12[] = {0xD9, 0x4E};
const uint8_t ShortRegData13[] = {OTM8009A_CMD_NOP, 0x81};
const uint8_t ShortRegData14[] = {0xC1, 0x66};
const uint8_t ShortRegData15[] = {OTM8009A_CMD_NOP, 0xA1};
const uint8_t ShortRegData16[] = {0xC1, 0x08};
const uint8_t ShortRegData17[] = {OTM8009A_CMD_NOP, 0x92};
const uint8_t ShortRegData18[] = {0xC5, 0x01};
const uint8_t ShortRegData19[] = {OTM8009A_CMD_NOP, 0x95};
const uint8_t ShortRegData20[] = {OTM8009A_CMD_NOP, 0x94};
const uint8_t ShortRegData21[] = {0xC5, 0x33};
const uint8_t ShortRegData22[] = {OTM8009A_CMD_NOP, 0xA3};
const uint8_t ShortRegData23[] = {0xC0, 0x1B};
const uint8_t ShortRegData24[] = {OTM8009A_CMD_NOP, 0x82};
const uint8_t ShortRegData25[] = {0xC5, 0x83};
const uint8_t ShortRegData26[] = {0xC4, 0x83};
const uint8_t ShortRegData27[] = {0xC1, 0x0E};
const uint8_t ShortRegData28[] = {OTM8009A_CMD_NOP, 0xA6};
const uint8_t ShortRegData29[] = {OTM8009A_CMD_NOP, 0xA0};
const uint8_t ShortRegData30[] = {OTM8009A_CMD_NOP, 0xB0};
const uint8_t ShortRegData31[] = {OTM8009A_CMD_NOP, 0xC0};
const uint8_t ShortRegData32[] = {OTM8009A_CMD_NOP, 0xD0};
const uint8_t ShortRegData33[] = {OTM8009A_CMD_NOP, 0x90};
const uint8_t ShortRegData34[] = {OTM8009A_CMD_NOP, 0xE0};
const uint8_t ShortRegData35[] = {OTM8009A_CMD_NOP, 0xF0};
const uint8_t ShortRegData36[] = {OTM8009A_CMD_SLPOUT, 0x00};
const uint8_t ShortRegData37[] = {OTM8009A_CMD_COLMOD, OTM8009A_COLMOD_RGB565};
const uint8_t ShortRegData38[] = {OTM8009A_CMD_COLMOD, OTM8009A_COLMOD_RGB888};
const uint8_t ShortRegData39[] = {OTM8009A_CMD_MADCTR, OTM8009A_MADCTR_MODE_LANDSCAPE};
const uint8_t ShortRegData40[] = {OTM8009A_CMD_WRDISBV, 0x7F};
const uint8_t ShortRegData41[] = {OTM8009A_CMD_WRCTRLD, 0x2C};
const uint8_t ShortRegData42[] = {OTM8009A_CMD_WRCABC, 0x02};
const uint8_t ShortRegData43[] = {OTM8009A_CMD_WRCABCMB, 0xFF};
const uint8_t ShortRegData44[] = {OTM8009A_CMD_DISPON, 0x00};
const uint8_t ShortRegData45[] = {OTM8009A_CMD_RAMWR, 0x00};
const uint8_t ShortRegData46[] = {0xCF, 0x00};
const uint8_t ShortRegData47[] = {0xC5, 0x66};
const uint8_t ShortRegData48[] = {OTM8009A_CMD_NOP, 0xB6};
const uint8_t ShortRegData49[] = {0xF5, 0x06};
//}}}
//{{{  lcdRegData const
const uint8_t lcdRegData1[]  = {0x80,0x09,0x01,0xFF};
const uint8_t lcdRegData2[]  = {0x80,0x09,0xFF};
const uint8_t lcdRegData3[]  = {0x00,0x09,0x0F,0x0E,0x07,0x10,0x0B,0x0A,0x04,0x07,0x0B,0x08,0x0F,0x10,0x0A,0x01,0xE1};
const uint8_t lcdRegData4[]  = {0x00,0x09,0x0F,0x0E,0x07,0x10,0x0B,0x0A,0x04,0x07,0x0B,0x08,0x0F,0x10,0x0A,0x01,0xE2};
const uint8_t lcdRegData5[]  = {0x79,0x79,0xD8};
const uint8_t lcdRegData6[]  = {0x00,0x01,0xB3};
const uint8_t lcdRegData7[]  = {0x85,0x01,0x00,0x84,0x01,0x00,0xCE};
const uint8_t lcdRegData8[]  = {0x18,0x04,0x03,0x39,0x00,0x00,0x00,0x18,0x03,0x03,0x3A,0x00,0x00,0x00,0xCE};
const uint8_t lcdRegData9[]  = {0x18,0x02,0x03,0x3B,0x00,0x00,0x00,0x18,0x01,0x03,0x3C,0x00,0x00,0x00,0xCE};
const uint8_t lcdRegData10[] = {0x01,0x01,0x20,0x20,0x00,0x00,0x01,0x02,0x00,0x00,0xCF};
const uint8_t lcdRegData11[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCB};
const uint8_t lcdRegData12[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCB};
const uint8_t lcdRegData13[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCB};
const uint8_t lcdRegData14[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCB};
const uint8_t lcdRegData15[] = {0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCB};
const uint8_t lcdRegData16[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0xCB};
const uint8_t lcdRegData17[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCB};
const uint8_t lcdRegData18[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xCB};
const uint8_t lcdRegData19[] = {0x00,0x26,0x09,0x0B,0x01,0x25,0x00,0x00,0x00,0x00,0xCC};
const uint8_t lcdRegData20[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0x0A,0x0C,0x02,0xCC};
const uint8_t lcdRegData21[] = {0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCC};
const uint8_t lcdRegData22[] = {0x00,0x25,0x0C,0x0A,0x02,0x26,0x00,0x00,0x00,0x00,0xCC};
const uint8_t lcdRegData23[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x0B,0x09,0x01,0xCC};
const uint8_t lcdRegData24[] = {0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCC};
const uint8_t lcdRegData25[] = {0xFF,0xFF,0xFF,0xFF};
// CASET value (Column Address Set) : X direction LCD GRAM boundaries
// depending on LCD orientation mode and PASET value (Page Address Set) : Y direction
// LCD GRAM boundaries depending on LCD orientation mode
// XS[15:0] = 0x000 = 0, XE[15:0] = 0x31F = 799 for landscape mode : apply to CASET
// YS[15:0] = 0x000 = 0, YE[15:0] = 0x31F = 799 for portrait mode : : apply to PASET
const uint8_t lcdRegData27[] = {0x00, 0x00, 0x03, 0x1F, OTM8009A_CMD_CASET};
// XS[15:0] = 0x000 = 0, XE[15:0] = 0x1DF = 479 for portrait mode : apply to CASET
// YS[15:0] = 0x000 = 0, YE[15:0] = 0x1DF = 479 for landscape mode : apply to PASET
const uint8_t lcdRegData28[] = {0x00, 0x00, 0x01, 0xDF, OTM8009A_CMD_PASET};
//}}}
//}}}
//{{{  static vars
#ifdef STM32F769xx
  static DSI_HandleTypeDef hdsi_discovery;
  static DSI_VidCfgTypeDef hdsivideo_handle;
#endif

//{{{  struct tLTDC
typedef struct {
  SemaphoreHandle_t sem;
  uint32_t timeouts;
  uint32_t lineIrq;
  uint32_t fifoUnderunIrq;
  uint32_t transferErrorIrq;
  uint32_t lastTicks;
  uint32_t lineTicks;
  uint32_t frameWait;
  } tLTDC;
//}}}
static tLTDC ltdc;

static uint8_t showAlpha[2];
uint32_t showFrameBufferAddress[2];

static uint32_t* mDma2dBuf = nullptr;
static uint32_t* mDma2dIsrBuf = nullptr;
static SemaphoreHandle_t mDma2dSem;

//{{{
class cFontChar {
public:
  void* operator new (std::size_t size) { return pvPortMalloc (size); }
  void operator delete (void *ptr) { vPortFree (ptr); }

  uint8_t* bitmap;
  int16_t left;
  int16_t top;
  int16_t pitch;
  int16_t rows;
  int16_t advance;
  };
//}}}
typedef std::map<uint16_t, cFontChar*> cFontCharMap;
cFontCharMap mFontCharMap;

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

    ltdc.lineTicks = xTaskGetTickCount() - ltdc.lastTicks;
    ltdc.lastTicks = xTaskGetTickCount();
    ltdc.lineIrq++;

    if (ltdc.frameWait) {
      portBASE_TYPE taskWoken = pdFALSE;
      if (xSemaphoreGiveFromISR (ltdc.sem, &taskWoken) == pdTRUE)
        portEND_SWITCHING_ISR (taskWoken);
      }
    ltdc.frameWait = 0;
    }

  LTDC->ICR = LTDC_FLAG_LI | LTDC_FLAG_FU | LTDC_FLAG_TE;
  }
//}}}
//{{{
void LCD_DMA2D_IRQHandler() {

  // clear interrupts
  DMA2D->IFCR = DMA2D_ISR_TCIF | DMA2D_ISR_TEIF | DMA2D_ISR_CEIF;

  while (true) {
    uint32_t opcode = *mDma2dIsrBuf++;
    switch (opcode) {
      case kEnd: {
        DMA2D->CR = 0;
        mDma2dIsrBuf = mDma2dBuf;
        portBASE_TYPE taskWoken = pdFALSE;
        if (xSemaphoreGiveFromISR (mDma2dSem, &taskWoken) == pdTRUE)
          portEND_SWITCHING_ISR (taskWoken);
        return;
        }
      case kStamp:
        DMA2D->OMAR    = *mDma2dIsrBuf;   // output start address
        DMA2D->BGMAR   = *mDma2dIsrBuf++; // - repeated to bgnd start addres
        DMA2D->OOR     = *mDma2dIsrBuf;   // output stride
        DMA2D->BGOR    = *mDma2dIsrBuf++; // - repeated to bgnd stride
        DMA2D->NLR     = *mDma2dIsrBuf++; // width:height
        DMA2D->FGMAR   = *mDma2dIsrBuf++; // fgnd start address
        DMA2D->FGPFCCR = DMA2D_INPUT_A8;  // fgnd PFC
        DMA2D->FGOR    = 0;               // fgnd stride
        DMA2D->CR = DMA2D_M2M_BLEND | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
        return;

      default: // opcode==address : value
        *(uint32_t*)opcode = *mDma2dIsrBuf++;
        if (opcode == AHB1PERIPH_BASE + 0xB000U) // CR
          return;
      }
    }
  }
//}}}

// cLcd
//{{{
cLcd::cLcd (uint32_t buffer0, uint32_t buffer1)  {

  mLcd = this;
  mBuffer[false] = buffer0;
  mBuffer[true] = buffer1;
  updateNumDrawLines();
  }
//}}}
cLcd::~cLcd() {}

// static
cLcd* cLcd::mLcd = nullptr;

//{{{
void cLcd::init (std::string title) {

  mDrawBuffer = !mDrawBuffer;
  ltdcInit (mBuffer[mDrawBuffer]);

  //  dma2d init
  // unchanging dma2d regs
  #ifdef USE_RGB888
    DMA2D->OPFCCR  = DMA2D_INPUT_RGB888; // dst  PFC
    DMA2D->BGPFCCR = DMA2D_INPUT_RGB888; // bgnd PFC
  #else
    DMA2D->OPFCCR  = DMA2D_INPUT_ARGB8888;
    DMA2D->BGPFCCR = DMA2D_INPUT_ARGB8888;
  #endif
  //DMA2D->AMTCR = 0x1001;

  // zero out first opcode, point past it
  mDma2dBuf = (uint32_t*)DMA2D_BUFFER;
  mDma2dIsrBuf = mDma2dBuf;
  mDma2dCurBuf = mDma2dBuf;
  *mDma2dCurBuf = kEnd;

  // why is this counting, init -1 ??
  //vSemaphoreCreateBinary (mDma2dSem);
  mDma2dSem = xSemaphoreCreateCounting (-1, 0);

  // dma2d IRQ init
  HAL_NVIC_SetPriority (DMA2D_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ (DMA2D_IRQn);

  // font init
  FT_Init_FreeType (&FTlibrary);
  FT_New_Memory_Face (FTlibrary, (FT_Byte*)freeSansBold, sizeof (freeSansBold), 0, &FTface);
  FTglyphSlot = FTface->glyph;

  // preload fontChars
  for (char ch = 0x20; ch <= 0x7F; ch++)
    loadChar (cWidget::getFontHeight(), ch);
  for (char ch = 0x21; ch <= 0x3F; ch++)
    loadChar (cWidget::getBigFontHeight(), ch);
  for (char ch = 0x21; ch <= 0x3F; ch++)
    loadChar (cWidget::getSmallFontHeight(), ch);

  FT_Done_Face (FTface);
  //FT_Done_FreeType (FTlibrary);

  mTitle = title;
  updateNumDrawLines();
  }
//}}}

//{{{
void cLcd::setShowDebug (bool title, bool info, bool lcdStats, bool footer) {

  mShowTitle = title;
  mShowInfo = info;
  mShowLcdStats = lcdStats;
  mShowFooter = footer;

  updateNumDrawLines();
  }
//}}}

//{{{
void cLcd::info (std::string str) {
  info (COL_WHITE, str);
  }
//}}}
//{{{
void cLcd::info (uint32_t colour, std::string str) {

  bool tailing = mLastLine == (int)mFirstLine + mNumDrawLines - 1;

  auto line = (mLastLine < mMaxLine-1) ? mLastLine+1 : mLastLine;
  mLines[line].mTime = xTaskGetTickCount();
  mLines[line].mColour = colour;
  mLines[line].mString = str;
  mLastLine = line;

  if (tailing)
    mFirstLine = mLastLine - mNumDrawLines + 1;
  }
//}}}

//{{{
void cLcd::press (int pressCount, int16_t x, int16_t y, uint16_t z, int16_t xinc, int16_t yinc) {

  if ((pressCount > 30) && (x <= mStringPos) && (y <= cWidget::getBoxHeight()))
    reset();
  else if (pressCount == 0) {
    if (x <= mStringPos) {
      // set displayFirstLine
      if (y < 2 * cWidget::getBoxHeight())
        displayTop();
      else if (y > getLcdHeightPix() - 2 * cWidget::getBoxHeight())
        displayTail();
      }
    }
  else {
    // inc firstLine
    float value = mFirstLine - ((2.0f * yinc) / cWidget::getBoxHeight());

    if (value < 0)
      mFirstLine = 0;
    else if (mLastLine <= (int)mNumDrawLines-1)
      mFirstLine = 0;
    else if (value > mLastLine - mNumDrawLines + 1)
      mFirstLine = mLastLine - mNumDrawLines + 1;
    else
      mFirstLine = value;
    }
  }
//}}}

//{{{
void cLcd::startRender() {

  mDrawBuffer = !mDrawBuffer;
  setLayer (0, mBuffer[mDrawBuffer]);

  // frameSync;
  ltdc.frameWait = 1;
  if (xSemaphoreTake (ltdc.sem, 100) == pdFALSE)
    ltdc.timeouts++;

  mDrawStartTime = xTaskGetTickCount();
  }
//}}}
//{{{
void cLcd::renderCursor (uint32_t colour, int16_t x, int16_t y, int16_t z) {
  ellipse (colour, x, y, z, z);
  }
//}}}
//{{{
void cLcd::endRender (bool forceInfo) {

  auto y = 0;
  if ((mShowTitle || forceInfo) && !mTitle.empty()) {
    //{{{  draw title
    text (COL_YELLOW, cWidget::getFontHeight(), mTitle, 0, y, getLcdWidthPix(), cWidget::getBoxHeight());
    y += cWidget::getBoxHeight();
    }
    //}}}
  if (mShowInfo || forceInfo) {
    //{{{  draw info lines
    if (mLastLine >= 0) {
      // draw scroll bar
      auto yorg = cWidget::getBoxHeight() + ((int)mFirstLine * mNumDrawLines * cWidget::getBoxHeight() / (mLastLine + 1));
      auto height = mNumDrawLines * mNumDrawLines * cWidget::getBoxHeight() / (mLastLine + 1);
      rectClipped (COL_YELLOW, 0, yorg, 8, height);
      }

    auto lastLine = (int)mFirstLine + mNumDrawLines - 1;
    if (lastLine > mLastLine)
      lastLine = mLastLine;

    for (auto lineIndex = (int)mFirstLine; lineIndex <= lastLine; lineIndex++) {
      auto x = 0;
      auto xinc = text (COL_GREEN, cWidget::getFontHeight(),
                        dec ((mLines[lineIndex].mTime-mStartTime) / 1000) + "." +
                        dec ((mLines[lineIndex].mTime-mStartTime) % 1000, 3, '0'),
                        x, y, getLcdWidthPix(), cWidget::getBoxHeight());
      x += xinc + 3;

      text (mLines[lineIndex].mColour, cWidget::getFontHeight(), mLines[lineIndex].mString,
            x, y, getLcdWidthPix()-x, getLcdHeightPix());

      y += cWidget::getBoxHeight();
      }
    }
    //}}}
  if (mShowLcdStats) {
    //{{{  draw lcdStats
    std::string str = dec (ltdc.lineIrq) + ":f " +
                      dec (ltdc.lineTicks) + "ms " +
                      dec (mDma2dTimeouts) + " " +
                      dec (ltdc.transferErrorIrq) + " " +
                      dec (ltdc.fifoUnderunIrq);
    text (COL_WHITE, cWidget::getFontHeight(), str, 0, getLcdHeightPix() - 2 * cWidget::getBoxHeight(), getLcdWidthPix(), 24);
    }
    //}}}
  if (mShowFooter || forceInfo)
    //{{{  draw footer
    text (COL_YELLOW, cWidget::getFontHeight(),
          dec (xPortGetFreeHeapSize()) + " " +
          dec (xPortGetMinimumEverFreeHeapSize()) + " " +
          dec (osGetCPUUsage()) + "% " + dec (mDrawTime) + "ms " + dec (mDma2dCurBuf - mDma2dBuf),
          0, -cWidget::getFontHeight() + getLcdHeightPix(), getLcdWidthPix(), cWidget::getFontHeight());
    //}}}

  // terminate opCode buffer
  *mDma2dCurBuf = kEnd;

   // send opCode buffer
  LCD_DMA2D_IRQHandler();

  // wait
  if (xSemaphoreTake (mDma2dSem, 500) == pdFALSE)
    mDma2dTimeouts++;
  showLayer (0, mBuffer[mDrawBuffer], 255);

  // reset opcode buffer
  mDma2dCurBuf = mDma2dBuf;
  *mDma2dCurBuf = kEnd;

  mDrawTime = xTaskGetTickCount() - mDrawStartTime;
  }
//}}}

//{{{
void cLcd::displayOn() {

  #ifdef STM32F746xx
    // enable LTDC
    LTDC->GCR |= LTDC_GCR_LTDCEN;

    // turn on DISP pin
    HAL_GPIO_WritePin (LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_SET);        /* Assert LCD_DISP pin */

    // turn on backlight
    HAL_GPIO_WritePin (LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);  /* Assert LCD_BL_CTRL pin */
  #else
    /* Send Display on DCS command to display */
    HAL_DSI_ShortWrite (&(hdsi_discovery), hdsivideo_handle.VirtualChannelID, DSI_DCS_SHORT_PKT_WRITE_P1,
                        OTM8009A_CMD_DISPON, 0x00);
  #endif
  }
//}}}
//{{{
void cLcd::displayOff() {

   #ifdef STM32F746xx
    // disable LTDC
    LTDC->GCR &= ~LTDC_GCR_LTDCEN;

    // turn off DISP pin
    HAL_GPIO_WritePin (LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_RESET);      /* De-assert LCD_DISP pin */

    // turn off backlight
    HAL_GPIO_WritePin (LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_RESET);/* De-assert LCD_BL_CTRL pin */
  #else
    /* Send Display off DCS Command to display */
    HAL_DSI_ShortWrite (&(hdsi_discovery), hdsivideo_handle.VirtualChannelID, DSI_DCS_SHORT_PKT_WRITE_P1,
                        OTM8009A_CMD_DISPOFF, 0x00);
  #endif
  }
//}}}

// iDraw
//{{{
void cLcd::pixel (uint32_t colour, int16_t x, int16_t y) {

  rect (colour, x, y, 1, 1);
  }
//}}}
//{{{
void cLcd::rect (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  // often same colour
  if (colour != mCurDstColour) {
    *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x38; // OCOLR - output colour
    *mDma2dCurBuf++ = colour;
    mCurDstColour = colour;
    }

  // quite often same stride
  if (uint32_t(getLcdWidthPix() - width) != mDstStride) {
    *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x40; // OOR - output stride
    mDstStride = getLcdWidthPix() - width;
    *mDma2dCurBuf++ = mDstStride;
    }

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x3C; // OMAR - output start address
  *mDma2dCurBuf++ = mCurFrameBufferAddress + ((y * getLcdWidthPix()) + x) * dstComponents;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x44; // NLR
  *mDma2dCurBuf++ = (width << 16) | height;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U;        // CR
  *mDma2dCurBuf++ = DMA2D_R2M | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
  }
//}}}
//{{{
void cLcd::stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  // often same colour
  if (colour != mCurSrcColour) {
    *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x20; // FGCOLR - fgnd colour
    *mDma2dCurBuf++ = colour;
    mCurSrcColour = colour;
    }

  *mDma2dCurBuf++ = kStamp;
  *mDma2dCurBuf++ = mCurFrameBufferAddress + ((y * getLcdWidthPix()) + x) * dstComponents; // output start address
  mDstStride = getLcdWidthPix() - width;
  *mDma2dCurBuf++ = mDstStride;                                          // stride
  *mDma2dCurBuf++ = (width << 16) | height;                              // width:height
  *mDma2dCurBuf++ = (uint32_t)src;                                       // fgnd start address
  }
//}}}
//{{{
int cLcd::text (uint32_t colour, uint16_t fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  auto xend = x + width;
  for (uint16_t i = 0; i < str.size(); i++) {
    if ((str[i] >= 0x20) && (str[i] <= 0x7F)) {
      auto fontCharIt = mFontCharMap.find (fontHeight<<8 | str[i]);
      if (fontCharIt != mFontCharMap.end()) {
        auto fontChar = fontCharIt->second;
        if (x + fontChar->left + fontChar->pitch >= xend)
          break;
        else if (fontChar->bitmap)
          stampClipped (colour, fontChar->bitmap, x + fontChar->left, y + fontHeight - fontChar->top, fontChar->pitch, fontChar->rows);

        x += fontChar->advance;
        }
      }
    }

  return x;
  }
//}}}
//{{{
void cLcd::copy (uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {
// copy RGB888 to ARGB8888

  // output
  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x3C; // OMAR - output start address
  *mDma2dCurBuf++ = mCurFrameBufferAddress + ((y * getLcdWidthPix()) + x) * dstComponents;

  mDstStride = getLcdWidthPix() - width;
  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x40; // OOR - output stride
  *mDma2dCurBuf++ = mDstStride;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x44; // NLR - width:height
  *mDma2dCurBuf++ = (width << 16) | height;


  // src - fgnd
  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x1C; // FGPFCCR - fgnd PFC
  *mDma2dCurBuf++ = DMA2D_INPUT_RGB888;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x0C; // FGMAR - fgnd address
  *mDma2dCurBuf++ = (uint32_t)src;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x10; // FGOR - fgnd stride
  *mDma2dCurBuf++ = 0;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U;        // CR
  *mDma2dCurBuf++ = DMA2D_M2M_PFC | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
  }
//}}}
//{{{
void cLcd::copy (uint8_t* src, int16_t srcx, int16_t srcy, uint16_t srcWidth, uint16_t srcHeight,
                 int16_t dstx, int16_t dsty, uint16_t dstWidth, uint16_t dstHeight) {
// copy src to dst
// - some corner cases missing, not enough src for dst needs padding

  // output
  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x3C; // OMAR - output start address
  *mDma2dCurBuf++ = mCurFrameBufferAddress + ((dsty * getLcdWidthPix()) + dstx) * dstComponents;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x40; // OOR - output stride
  mDstStride = getLcdWidthPix() - dstWidth;
  *mDma2dCurBuf++ = mDstStride;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x44; // NLR - width:height
  *mDma2dCurBuf++ = (dstWidth << 16) | dstHeight;

  // src - fgnd
  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x1C; // FGPFCCR - fgnd PFC
  *mDma2dCurBuf++ = DMA2D_INPUT_RGB888;

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x0C; // FGMAR - fgnd address
  *mDma2dCurBuf++ = (uint32_t)(src + ((srcy * srcWidth) + srcx) * dstComponents);

  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U + 0x10; // FGOR - fgnd stride
  *mDma2dCurBuf++ = srcWidth - dstWidth;

  // CR
  *mDma2dCurBuf++ = AHB1PERIPH_BASE + 0xB000U;        // CR
  *mDma2dCurBuf++ = DMA2D_M2M_PFC | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
  }
//}}}

// private
//{{{
void cLcd::ltdcInit (uint32_t frameBufferAddress) {

  hLtdc.Instance = LTDC;
  //{{{  common clock enables
  __HAL_RCC_LTDC_CLK_ENABLE();
  __HAL_RCC_DMA2D_CLK_ENABLE();
  //}}}

  #ifdef STM32F746G_DISCO
    //{{{  gpio clock enables
    // enable GPIO clock, includes backlight, display enable
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    //}}}
    //{{{  pll config
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
    //{{{  LTDC info
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

    // LTDC Polarity
    hLtdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
    hLtdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
    hLtdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
    hLtdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

    // LTDC width height
    hLtdc.LayerCfg->ImageWidth = RK043FN48H_WIDTH;
    hLtdc.LayerCfg->ImageHeight = RK043FN48H_HEIGHT;

    // LTDC Background value
    hLtdc.Init.Backcolor.Blue = 0;
    hLtdc.Init.Backcolor.Green = 0;
    hLtdc.Init.Backcolor.Red = 0;
    //}}}
    HAL_LTDC_Init (&hLtdc);
  #else
    hdsi_discovery.Instance = DSI;
    //{{{  dsi, XRES gpio clock enables
    __HAL_RCC_DSI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    //}}}
    //{{{  pll config
    // Note: The following values should not be changed as the PLLSAI is also used to clock the USB FS
    // PLLSAI_VCO Input = HSE_VALUE/PLL_M = 1 Mhz
    // PLLSAI_VCO Output = PLLSAI_VCO Input * PLLSAIN = 384 Mhz
    // PLLLCDCLK = PLLSAI_VCO Output/PLLSAIR = 384 MHz / 7 = 54.85 MHz
    // LTDC clock frequency = PLLLCDCLK / LTDC_PLLSAI_DIVR_2 = 54.85 MHz / 2 = 27.429 MHz

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
    PeriphClkInitStruct.PLLSAI.PLLSAIR = 7;
    PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
    HAL_RCCEx_PeriphCLKConfig (&PeriphClkInitStruct);
    //}}}
    //{{{  pulse DSI LCD XRES - active low
    // Configure the GPIO on PJ15
    GPIO_InitTypeDef gpio_init_structure;
    gpio_init_structure.Pin   = GPIO_PIN_15;
    gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull  = GPIO_PULLUP;
    gpio_init_structure.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init (GPIOJ, &gpio_init_structure);

    // pulse XRES active low
    HAL_GPIO_WritePin (GPIOJ, GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin (GPIOJ, GPIO_PIN_15, GPIO_PIN_SET);
    HAL_Delay(10);
    //}}}
    //{{{  init DSI
    DSI_PLLInitTypeDef dsiPllInit;
    dsiPllInit.PLLNDIV = 100;
    dsiPllInit.PLLIDF = DSI_PLL_IN_DIV5;
    dsiPllInit.PLLODF = DSI_PLL_OUT_DIV1;

    uint32_t laneByteClk_kHz = 62500; /* 500 MHz / 8 = 62.5 MHz = 62500 kHz */
    hdsi_discovery.Init.NumberOfLanes = DSI_TWO_DATA_LANES;
    hdsi_discovery.Init.TXEscapeCkdiv = laneByteClk_kHz / 15620; // TXEscapeCkdiv = f(LaneByteClk)/15.62 = 4
    HAL_DSI_Init (&hdsi_discovery, &dsiPllInit);

    // config DSI
    uint32_t HACT = getLcdWidthPix(); /*!< Horizontal Active time in units of lcdClk = imageSize X in pixels to display */
    uint32_t VACT = getLcdHeightPix(); /*!< Vertical Active time in units of lines = imageSize Y in pixels to display */

    // The following values are same for portrait and landscape orientations
    uint32_t VSA = OTM8009A_480X800_VSYNC; // 12  - Vertical start active time in units of lines
    uint32_t VBP = OTM8009A_480X800_VBP;   // 12  - Vertical Back Porch time in units of lines
    uint32_t VFP = OTM8009A_480X800_VFP;   // 12  - Vertical Front Porch time in units of lines
    uint32_t HSA = OTM8009A_480X800_HSYNC; // 63  - Horizontal start active time in units of lcdClk
    uint32_t HBP = OTM8009A_480X800_HBP;   // 120 - Horizontal Back Porch time in units of lcdClk
    uint32_t HFP = OTM8009A_480X800_HFP;   // 120 - Horizontal Front Porch time in units of lcdClk

    uint32_t LcdClock  = 27429; /*!< LcdClk = 27429 kHz */

    hdsivideo_handle.ColorCoding          = DSI_RGB888;
    hdsivideo_handle.VSPolarity           = DSI_VSYNC_ACTIVE_HIGH;
    hdsivideo_handle.HSPolarity           = DSI_HSYNC_ACTIVE_HIGH;
    hdsivideo_handle.DEPolarity           = DSI_DATA_ENABLE_ACTIVE_HIGH;
    hdsivideo_handle.Mode                 = DSI_VID_MODE_BURST; /* Mode Video burst ie : one LgP per line */
    hdsivideo_handle.NullPacketSize       = 0xFFF;
    hdsivideo_handle.NumberOfChunks       = 0;
    hdsivideo_handle.PacketSize           = HACT; /* Value depending on display orientation choice portrait/landscape */
    hdsivideo_handle.HorizontalSyncActive = (HSA * laneByteClk_kHz) / LcdClock;
    hdsivideo_handle.HorizontalBackPorch  = (HBP * laneByteClk_kHz) / LcdClock;
    hdsivideo_handle.HorizontalLine       = ((HACT + HSA + HBP + HFP) * laneByteClk_kHz)/LcdClock; /* Value depending on display orientation choice portrait/landscape */
    hdsivideo_handle.VerticalSyncActive   = VSA;
    hdsivideo_handle.VerticalBackPorch    = VBP;
    hdsivideo_handle.VerticalFrontPorch   = VFP;
    hdsivideo_handle.VerticalActive       = VACT; /* Value depending on display orientation choice portrait/landscape */
    hdsivideo_handle.LPCommandEnable      = DSI_LP_COMMAND_ENABLE; // Enable sending LP command while streaming active in video mode */
    hdsivideo_handle.LPLargestPacketSize  = 16; // Largest packet size possible to transmit in LP mode in VSA, VBP, VFP regions
    hdsivideo_handle.LPVACTLargestPacketSize = 0;  // Largest packet size possible to transmit in LP mode in HFP region during VACT period
    hdsivideo_handle.LPHorizontalFrontPorchEnable = DSI_LP_HFP_ENABLE;    /* Allow sending LP commands during HFP period */
    hdsivideo_handle.LPHorizontalBackPorchEnable  = DSI_LP_HBP_ENABLE;    /* Allow sending LP commands during HBP period */
    hdsivideo_handle.LPVerticalActiveEnable       = DSI_LP_VACT_ENABLE;   /* Allow sending LP commands during VACT period */
    hdsivideo_handle.LPVerticalFrontPorchEnable   = DSI_LP_VFP_ENABLE;    /* Allow sending LP commands during VFP period */
    hdsivideo_handle.LPVerticalBackPorchEnable    = DSI_LP_VBP_ENABLE;    /* Allow sending LP commands during VBP period */
    hdsivideo_handle.LPVerticalSyncActiveEnable    = DSI_LP_VSYNC_ENABLE; /* Allow sending LP commands during VSync = VSA period */
    HAL_DSI_ConfigVideoMode (&hdsi_discovery, &hdsivideo_handle);
    //}}}
    //{{{  LTDC info
    hLtdc.Init.HorizontalSync = HSA - 1;
    hLtdc.Init.AccumulatedHBP = HSA + HBP - 1;
    hLtdc.Init.AccumulatedActiveW = getLcdWidthPix() + HSA + HBP - 1;
    hLtdc.Init.TotalWidth = getLcdWidthPix() + HSA + HBP + HFP - 1;

    hLtdc.LayerCfg->ImageWidth  = getLcdWidthPix();
    hLtdc.LayerCfg->ImageHeight = getLcdHeightPix();

    // LTDC background value
    hLtdc.Init.Backcolor.Blue = 0;
    hLtdc.Init.Backcolor.Green = 0;
    hLtdc.Init.Backcolor.Red = 0;
    hLtdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
    //}}}
    HAL_LTDC_StructInitFromVideoConfig (&(hLtdc), &(hdsivideo_handle));
    HAL_LTDC_Init (&hLtdc);
    HAL_DSI_Start (&hdsi_discovery);
    otm8009aInit (true);
  #endif

  layerInit (0, frameBufferAddress);
  mSetFrameBufferAddress[1] = frameBufferAddress;
  showFrameBufferAddress[1] = frameBufferAddress;
  showAlpha[1] = 0;

  ltdc.timeouts = 0;
  ltdc.lineIrq = 0;
  ltdc.fifoUnderunIrq = 0;
  ltdc.transferErrorIrq = 0;
  ltdc.lastTicks = 0;
  ltdc.lineTicks = 0;
  ltdc.frameWait = 0;

  vSemaphoreCreateBinary (ltdc.sem);

  HAL_NVIC_SetPriority (LTDC_IRQn, 0xE, 0);
  HAL_NVIC_EnableIRQ (LTDC_IRQn);

  // set line interupt line number
  LTDC->LIPCR = 0;

  // enable line interrupt
  LTDC->IER |= LTDC_IT_LI;
  }
//}}}
//{{{
void cLcd::layerInit (uint8_t layer, uint32_t frameBufferAddress) {

  LTDC_LayerCfgTypeDef* curLayerCfg = &hLtdc.LayerCfg[layer];

  curLayerCfg->WindowX0 = 0;
  curLayerCfg->WindowX1 = getLcdWidthPix();
  curLayerCfg->WindowY0 = 0;
  curLayerCfg->WindowY1 = getLcdHeightPix();

  #ifdef USE_RGB888
    curLayerCfg->PixelFormat = LTDC_PIXEL_FORMAT_RGB888;
  #else
    curLayerCfg->PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
  #endif

  curLayerCfg->FBStartAdress = (uint32_t)frameBufferAddress;

  curLayerCfg->Alpha = 255;
  curLayerCfg->Alpha0 = 0;

  curLayerCfg->Backcolor.Blue = 0;
  curLayerCfg->Backcolor.Green = 0;
  curLayerCfg->Backcolor.Red = 0;

  curLayerCfg->BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  curLayerCfg->BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;

  curLayerCfg->ImageWidth = getLcdWidthPix();
  curLayerCfg->ImageHeight = getLcdHeightPix();

  HAL_LTDC_ConfigLayer (&hLtdc, curLayerCfg, layer);

  // local state
  mCurFrameBufferAddress = frameBufferAddress;
  mSetFrameBufferAddress[layer] = frameBufferAddress;
  showFrameBufferAddress[layer] = frameBufferAddress;
  showAlpha[layer] = 255;
  }
//}}}

#ifdef STM32F769I_DISCO
  //{{{
  void cLcd::dsiWriteCmd (uint32_t NbrParams, uint8_t* pParams) {

    if (NbrParams <= 1)
     HAL_DSI_ShortWrite (&hdsi_discovery, 0, DSI_DCS_SHORT_PKT_WRITE_P1, pParams[0], pParams[1]);
    else
     HAL_DSI_LongWrite (&hdsi_discovery,  0, DSI_DCS_LONG_PKT_WRITE, NbrParams, pParams[NbrParams], pParams);
    }
  //}}}
  //{{{
  void cLcd::otm8009aInit (bool landscape) {

    /* Enable CMD2 to access vendor specific commands                               */
    /* Enter in command 2 mode and set EXTC to enable address shift function (0x00) */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);
    dsiWriteCmd (3, (uint8_t*)lcdRegData1);

    /* Enter ORISE Command 2 */
    dsiWriteCmd (0, (uint8_t*)ShortRegData2); /* Shift address to 0x80 */
    dsiWriteCmd (2, (uint8_t*)lcdRegData2);

    /* SD_PCH_CTRL - 0xC480h - 129th parameter - Default 0x00          */
    /* Set SD_PT                                                       */
    /* -> Source output level during porch and non-display area to GND */
    dsiWriteCmd (0, (uint8_t*)ShortRegData2);
    dsiWriteCmd (0, (uint8_t*)ShortRegData3);
    HAL_Delay(10);

    /* Not documented */
    dsiWriteCmd (0, (uint8_t*)ShortRegData4);
    dsiWriteCmd (0, (uint8_t*)ShortRegData5);
    HAL_Delay(10);

    /* PWR_CTRL4 - 0xC4B0h - 178th parameter - Default 0xA8 */
    /* Set gvdd_en_test                                     */
    /* -> enable GVDD test mode !!!                         */
    dsiWriteCmd (0, (uint8_t*)ShortRegData6);
    dsiWriteCmd (0, (uint8_t*)ShortRegData7);

    /* PWR_CTRL2 - 0xC590h - 146th parameter - Default 0x79      */
    /* Set pump 4 vgh voltage                                    */
    /* -> from 15.0v down to 13.0v                               */
    /* Set pump 5 vgh voltage                                    */
    /* -> from -12.0v downto -9.0v                               */
    dsiWriteCmd (0, (uint8_t*)ShortRegData8);
    dsiWriteCmd (0, (uint8_t*)ShortRegData9);

    /* P_DRV_M - 0xC0B4h - 181th parameter - Default 0x00 */
    /* -> Column inversion                                */
    dsiWriteCmd (0, (uint8_t*)ShortRegData10);
    dsiWriteCmd (0, (uint8_t*)ShortRegData11);

    /* VCOMDC - 0xD900h - 1st parameter - Default 0x39h */
    /* VCOM Voltage settings                            */
    /* -> from -1.0000v downto -1.2625v                 */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);
    dsiWriteCmd (0, (uint8_t*)ShortRegData12);

    /* Oscillator adjustment for Idle/Normal mode (LPDT only) set to 65Hz (default is 60Hz) */
    dsiWriteCmd (0, (uint8_t*)ShortRegData13);
    dsiWriteCmd (0, (uint8_t*)ShortRegData14);

    /* Video mode internal */
    dsiWriteCmd (0, (uint8_t*)ShortRegData15);
    dsiWriteCmd (0, (uint8_t*)ShortRegData16);

    /* PWR_CTRL2 - 0xC590h - 147h parameter - Default 0x00 */
    /* Set pump 4&5 x6                                     */
    /* -> ONLY VALID when PUMP4_EN_ASDM_HV = "0"           */
    dsiWriteCmd (0, (uint8_t*)ShortRegData17);
    dsiWriteCmd (0, (uint8_t*)ShortRegData18);

    /* PWR_CTRL2 - 0xC590h - 150th parameter - Default 0x33h */
    /* Change pump4 clock ratio                              */
    /* -> from 1 line to 1/2 line                            */
    dsiWriteCmd (0, (uint8_t*)ShortRegData19);
    dsiWriteCmd (0, (uint8_t*)ShortRegData9);

    /* GVDD/NGVDD settings */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);
    dsiWriteCmd (2, (uint8_t*)lcdRegData5);

    /* PWR_CTRL2 - 0xC590h - 149th parameter - Default 0x33h */
    /* Rewrite the default value !                           */
    dsiWriteCmd (0, (uint8_t*)ShortRegData20);
    dsiWriteCmd (0, (uint8_t*)ShortRegData21);

    /* Panel display timing Setting 3 */
    dsiWriteCmd(0, (uint8_t*)ShortRegData22);
    dsiWriteCmd (0, (uint8_t*)ShortRegData23);

    /* Power control 1 */
    dsiWriteCmd (0, (uint8_t*)ShortRegData24);
    dsiWriteCmd (0, (uint8_t*)ShortRegData25);

    /* Source driver precharge */
    dsiWriteCmd (0, (uint8_t *)ShortRegData13);
    dsiWriteCmd (0, (uint8_t*)ShortRegData26);

    dsiWriteCmd (0, (uint8_t*)ShortRegData15);
    dsiWriteCmd (0, (uint8_t*)ShortRegData27);

    dsiWriteCmd (0, (uint8_t*)ShortRegData28);
    dsiWriteCmd (2, (uint8_t *)lcdRegData6);

    /* GOAVSt*/
    dsiWriteCmd (0, (uint8_t*)ShortRegData2);
    dsiWriteCmd (6, (uint8_t*)lcdRegData7);

    dsiWriteCmd (0, (uint8_t*)ShortRegData29);
    dsiWriteCmd (14, (uint8_t*)lcdRegData8);

    dsiWriteCmd (0, (uint8_t*)ShortRegData30);
    dsiWriteCmd (14, (uint8_t*)lcdRegData9);

    dsiWriteCmd (0, (uint8_t*)ShortRegData31);
    dsiWriteCmd (10, (uint8_t*)lcdRegData10);

    dsiWriteCmd (0, (uint8_t*)ShortRegData32);
    dsiWriteCmd (0, (uint8_t*)ShortRegData46);

    dsiWriteCmd (0, (uint8_t*)ShortRegData2);
    dsiWriteCmd (10, (uint8_t*)lcdRegData11);

    dsiWriteCmd (0, (uint8_t*)ShortRegData33);
    dsiWriteCmd (15, (uint8_t*)lcdRegData12);

    dsiWriteCmd (0, (uint8_t*)ShortRegData29);
    dsiWriteCmd (15, (uint8_t*)lcdRegData13);

    dsiWriteCmd (0, (uint8_t*)ShortRegData30);
    dsiWriteCmd (10, (uint8_t*)lcdRegData14);

    dsiWriteCmd (0, (uint8_t*)ShortRegData31);
    dsiWriteCmd (15, (uint8_t*)lcdRegData15);

    dsiWriteCmd (0, (uint8_t*)ShortRegData32);
    dsiWriteCmd (15, (uint8_t*)lcdRegData16);

    dsiWriteCmd (0, (uint8_t*)ShortRegData34);
    dsiWriteCmd (10, (uint8_t*)lcdRegData17);

    dsiWriteCmd (0, (uint8_t*)ShortRegData35);
    dsiWriteCmd (10, (uint8_t*)lcdRegData18);

    dsiWriteCmd (0, (uint8_t*)ShortRegData2);
    dsiWriteCmd (10, (uint8_t*)lcdRegData19);

    dsiWriteCmd (0, (uint8_t*)ShortRegData33);
    dsiWriteCmd (15, (uint8_t*)lcdRegData20);

    dsiWriteCmd (0, (uint8_t*)ShortRegData29);
    dsiWriteCmd (15, (uint8_t*)lcdRegData21);

    dsiWriteCmd (0, (uint8_t*)ShortRegData30);
    dsiWriteCmd (10, (uint8_t*)lcdRegData22);

    dsiWriteCmd (0, (uint8_t*)ShortRegData31);
    dsiWriteCmd (15, (uint8_t*)lcdRegData23);

    dsiWriteCmd (0, (uint8_t*)ShortRegData32);
    dsiWriteCmd (15, (uint8_t*)lcdRegData24);

    /* PWR_CTRL1 - 0xc580h - 130th parameter - default 0x00 */
    /* Pump 1 min and max DM                                */
    dsiWriteCmd (0, (uint8_t*)ShortRegData13);
    dsiWriteCmd (0, (uint8_t*)ShortRegData47);
    dsiWriteCmd (0, (uint8_t*)ShortRegData48);
    dsiWriteCmd (0, (uint8_t*)ShortRegData49);

    /* Exit CMD2 mode */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);
    dsiWriteCmd (3, (uint8_t*)lcdRegData25);

    // Standard DCS Initialization TO KEEP CAN BE DONE IN HSDT                   */

    /* NOP - goes back to DCS std command ? */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);

    /* Gamma correction 2.2+ table (HSDT possible) */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);
    dsiWriteCmd (16, (uint8_t*)lcdRegData3);

    /* Gamma correction 2.2- table (HSDT possible) */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);
    dsiWriteCmd (16, (uint8_t*)lcdRegData4);

    /* Send Sleep Out command to display : no parameter */
    dsiWriteCmd (0, (uint8_t*)ShortRegData36);

    /* Wait for sleep out exit*/
    HAL_Delay (120);

    //dsiWriteCmd (0, (uint8_t*)ShortRegData37); // Set Pixel color format to RGB565
    dsiWriteCmd (0, (uint8_t*)ShortRegData38); // Set Pixel color format to RGB888

    // portrait by default
    if (landscape) {
      dsiWriteCmd (0, (uint8_t*)ShortRegData39);
      dsiWriteCmd (4, (uint8_t*)lcdRegData27);
      dsiWriteCmd (4, (uint8_t*)lcdRegData28);
      }

    // CABC : Content Adaptive Backlight Control section start
    dsiWriteCmd (0, (uint8_t*)ShortRegData40); // defaut is 0 (lowest Brightness), 0xFF is highest Brightness, try 0x7F : intermediate value
    dsiWriteCmd (0, (uint8_t*)ShortRegData41); // defaut is 0, try 0x2C - Brightness Control Block, Display Dimming & BackLight on
    dsiWriteCmd (0, (uint8_t*)ShortRegData42); // defaut is 0, try 0x02 - image Content based Adaptive Brightness [Still Picture]
    dsiWriteCmd (0, (uint8_t*)ShortRegData43); // defaut is 0 (lowest Brightness), 0xFF is highest Brightness

    /* Send Command Display On */
    dsiWriteCmd (0, (uint8_t*)ShortRegData44);

    /* NOP command */
    dsiWriteCmd (0, (uint8_t*)ShortRegData1);

    /* Send Command GRAM memory write (no parameters) : this initiates frame write via other DSI commands sent by */
    /* DSI host from LTDC incoming pixels in video mode */
    dsiWriteCmd (0, (uint8_t*)ShortRegData45);
    }
  //}}}
#endif

//{{{
void cLcd::setLayer (uint8_t layer, uint32_t frameBufferAddress) {

  mCurFrameBufferAddress = frameBufferAddress;
  mSetFrameBufferAddress[layer] = frameBufferAddress;
  }
//}}}
//{{{
void cLcd::showLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha) {

  showFrameBufferAddress[layer] = frameBufferAddress;
  showAlpha[layer] = alpha;
  }
//}}}

//{{{
cFontChar* cLcd::loadChar (uint16_t fontHeight, char ch) {

  FT_Set_Pixel_Sizes (FTface, 0, fontHeight);
  FT_Load_Char (FTface, ch, FT_LOAD_RENDER);

  auto fontChar = new cFontChar();
  fontChar->left = FTglyphSlot->bitmap_left;
  fontChar->top = FTglyphSlot->bitmap_top;
  fontChar->pitch = FTglyphSlot->bitmap.pitch;
  fontChar->rows = FTglyphSlot->bitmap.rows;
  fontChar->advance = FTglyphSlot->advance.x / 64;
  fontChar->bitmap = nullptr;

  if (FTglyphSlot->bitmap.buffer) {
    fontChar->bitmap = (uint8_t*)pvPortMalloc (FTglyphSlot->bitmap.pitch * FTglyphSlot->bitmap.rows);
    memcpy (fontChar->bitmap, FTglyphSlot->bitmap.buffer, FTglyphSlot->bitmap.pitch * FTglyphSlot->bitmap.rows);
    }

  auto insertPair = mFontCharMap.insert (cFontCharMap::value_type (fontHeight<<8 | ch, fontChar));
  auto fontCharIt = insertPair.first;

  return fontCharIt->second;
  }
//}}}

//{{{
void cLcd::reset() {

  for (auto i = 0; i < mMaxLine; i++)
    mLines[i].clear();

  mStartTime = xTaskGetTickCount();
  mLastLine = -1;
  mFirstLine = 0;
  }
//}}}
//{{{
void cLcd::displayTop() {
  mFirstLine = 0;
  }
//}}}
//{{{
void cLcd::displayTail() {

  if (mLastLine > (int)mNumDrawLines-1)
    mFirstLine = mLastLine - mNumDrawLines + 1;
  else
    mFirstLine = 0;
  }
//}}}
//{{{
void cLcd::updateNumDrawLines() {

  mStringPos = cWidget::getBoxHeight()*3;

  auto numDrawLines = getLcdHeightPix() / cWidget::getBoxHeight();
  if (mShowTitle && !mTitle.empty())
    numDrawLines--;
  if (mShowLcdStats)
    numDrawLines--;
  if (mShowFooter)
    numDrawLines--;

  mNumDrawLines = numDrawLines;
  }
//}}}
