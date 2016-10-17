// cLcd.cpp
//{{{  includes
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "memory.h"
#include "stm32746g_disco.h"
#include "stm32f7xx_hal_dma2d.h"

#include "cLcd.h"
#include "cLcdPrivate.h"

#include "cmsis_os.h"
#include "cpuUsage.h"

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

#define kWipe  1
#define kStamp 2
//}}}
//{{{  static vars
static uint32_t curFrameBufferAddress;
static uint32_t setFrameBufferAddress[2];
static uint32_t showFrameBufferAddress[2];
static uint8_t showAlpha[2];

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
static tLTDC ltdc;

static uint32_t* mDma2dBuf = nullptr;
static uint32_t* mDma2dIsrBuf = nullptr;
static osSemaphoreId mDma2dSem;

#define maxChars 0x60
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

  // clear interrupt flags
  DMA2D->IFCR = DMA2D_ISR_TCIF | DMA2D_ISR_TEIF | DMA2D_ISR_CEIF;

  while (true) {
    switch (*mDma2dIsrBuf++) {
      case kWipe:
        DMA2D->OCOLR = *mDma2dIsrBuf++;  // colour
        DMA2D->OMAR  = *mDma2dIsrBuf++;  // fb start address
        DMA2D->OOR   = *mDma2dIsrBuf++;  // stride
        DMA2D->NLR   = *mDma2dIsrBuf++;  // width:height
        DMA2D->CR = DMA2D_R2M | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
        return;

      case kStamp:
        DMA2D->FGCOLR = *mDma2dIsrBuf++; // src color
        DMA2D->OMAR   = *mDma2dIsrBuf;   // bgnd fb start address
        DMA2D->BGMAR  = *mDma2dIsrBuf++;
        DMA2D->OOR    = *mDma2dIsrBuf;   // bgnd stride
        DMA2D->BGOR   = *mDma2dIsrBuf++;
        DMA2D->NLR    = *mDma2dIsrBuf++; // width:height
        DMA2D->FGMAR  = *mDma2dIsrBuf++; // src start address
        DMA2D->CR = DMA2D_M2M_BLEND | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
        return;

      default: // normally 0
        // no more opCodes, disable interrupts and release semaphore to signal done
        DMA2D->CR = 0;
        mDma2dIsrBuf = mDma2dBuf;
        osSemaphoreRelease (mDma2dSem);
        return;
      }
    }
  }
//}}}

cLcd* cLcd::mLcd = nullptr;

// cLcd
//{{{
cLcd::cLcd (uint32_t buffer0, uint32_t buffer1)  {

  mStartTime = osKernelSysTick();

  mBuffer[false] = buffer0;
  mBuffer[true] = buffer1;

  updateNumDrawLines();
  }
//}}}
cLcd::~cLcd() {}

// static members
//{{{
cLcd* cLcd::create (std::string title, bool buffered) {

  if (!mLcd) {
    mLcd = new cLcd (SDRAM_FRAME0, SDRAM_FRAME1);
    mLcd->init (title, buffered);
    }

  return mLcd;
  }
//}}}
//{{{
std::string cLcd::hex (int value, uint8_t width) {

  std::ostringstream os;
  if (width)
    os << std::hex << std::setfill ('0') << std::setw (width) << value;
  else
    os << std::hex << value;
  return os.str();
  }
//}}}
//{{{
std::string cLcd::dec (int value, uint8_t width, char fill) {

  std::ostringstream os;
  if (width)
    os << std::setfill (fill) << std::setw (width) << value;
  else
    os << value;
  return os.str();
  }
//}}}

// public members
//{{{
void cLcd::setTitle (std::string title) {
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
void cLcd::info (uint32_t colour, std::string str, bool newLine) {

  bool tailing = mLastLine == (int)mFirstLine + mNumDrawLines - 1;

  auto line = (newLine && (mLastLine < mMaxLine-1)) ? mLastLine+1 : mLastLine;
  mLines[line].mTime = osKernelSysTick();
  mLines[line].mColour = colour;
  mLines[line].mString = str;
  if (newLine)
    mLastLine = line;

  if (tailing)
    mFirstLine = mLastLine - mNumDrawLines + 1;
  }
//}}}
//{{{
void cLcd::info (std::string str, bool newLine) {
  info (COL_WHITE, str, newLine);
  }
//}}}

//{{{
void cLcd::press (int pressCount, int x, int y, int z, int xinc, int yinc) {

  if ((pressCount > 30) && (x <= mStringPos) && (y <= getLineHeight()))
    reset();
  else if (pressCount == 0) {
    if (x <= mStringPos) {
      // set displayFirstLine
      if (y < 2 * getLineHeight())
        displayTop();
      else if (y > getHeight() - 2 * getLineHeight())
        displayTail();
      }
    }
  else {
    // inc firstLine
    float value = mFirstLine - ((2.0f * yinc) / getLineHeight());

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

  if (mBuffered) {
    mDrawBuffer = !mDrawBuffer;
    setLayer (0, mBuffer[mDrawBuffer]);

    // frameSync;
    ltdc.frameWait = 1;
    if (osSemaphoreWait (ltdc.sem, 100) != osOK)
      ltdc.timeouts++;
    }

  mDrawStartTime = osKernelSysTick();
  }
//}}}
//{{{
void cLcd::renderCursor (uint32_t colour, int16_t x, int16_t y, int16_t z) {
  ellipse (colour, x, y, z, z);
  }
//}}}
//{{{
void cLcd::endRender() {

  auto y = 0;
  if (mShowTitle && !mTitle.empty()) {
    //{{{  draw title
    text (COL_WHITE, getFontHeight(), mTitle, 0, y, getWidth(), getLineHeight());
    y += getLineHeight();
    }
    //}}}
  if (mShowInfo) {
    //{{{  draw info lines
    if (mLastLine >= 0) {
      // draw scroll bar
      auto yorg = getLineHeight() + ((int)mFirstLine * mNumDrawLines * getLineHeight() / (mLastLine + 1));
      auto height = mNumDrawLines * mNumDrawLines * getLineHeight() / (mLastLine + 1);
      rectClipped (COL_YELLOW, 0, yorg, 8, height);
      }

    auto lastLine = (int)mFirstLine + mNumDrawLines - 1;
    if (lastLine > mLastLine)
      lastLine = mLastLine;
    for (auto lineIndex = (int)mFirstLine; lineIndex <= lastLine; lineIndex++) {
      auto x = 0;
      auto xinc = text (COL_GREEN, getFontHeight(),
                        dec ((mLines[lineIndex].mTime-mStartTime) / 1000) + "." +
                        dec ((mLines[lineIndex].mTime-mStartTime) % 1000, 3, '0'), x, y, getWidth(), getLineHeight());
      x += xinc + 3;
      text (mLines[lineIndex].mColour, getFontHeight(), mLines[lineIndex].mString, x, y, getWidth(), getLineHeight());
      y += getLineHeight();
      }
    }
    //}}}
  if (mShowLcdStats) {
    //{{{  draw lcdStats
    std::string str = dec (ltdc.lineIrq) + ":f " +
                  dec (ltdc.lineTicks) + "ms " +
                      dec (mDma2dHighWater-mDma2dBuf) + ":hi " +
                      dec (mDma2dTimeouts) + " " +
                      dec (ltdc.transferErrorIrq) + " " +
                      dec (ltdc.fifoUnderunIrq);
    text (COL_WHITE, getFontHeight(), str, 0, getHeight() - 2 * getLineHeight(), getWidth(), 24);
    }
    //}}}
  if (mShowFooter)
    //{{{  draw footer
    text (COL_YELLOW, getFontHeight(),
          dec (xPortGetFreeHeapSize()) + " " + dec (osGetCPUUsage()) + "% " + dec (mDrawTime) + "ms",
          0, getHeight()-getLineHeight(), getWidth(), getLineHeight());
    //}}}

  sendWait();
  showLayer (0, mBuffer[mDrawBuffer], 255);

  mDrawTime = osKernelSysTick() - mDrawStartTime;
  }
//}}}

//{{{
void cLcd::displayOn() {

  // enable LTDC
  LTDC->GCR |= LTDC_GCR_LTDCEN;

  // turn on DISP pin
  HAL_GPIO_WritePin (LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_SET);        /* Assert LCD_DISP pin */

  // turn on backlight
  HAL_GPIO_WritePin (LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);  /* Assert LCD_BL_CTRL pin */
  }
//}}}
//{{{
void cLcd::displayOff() {

  // disable LTDC
  LTDC->GCR &= ~LTDC_GCR_LTDCEN;

  // turn off DISP pin
  HAL_GPIO_WritePin (LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_RESET);      /* De-assert LCD_DISP pin */

  // turn off backlight
  HAL_GPIO_WritePin (LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_RESET);/* De-assert LCD_BL_CTRL pin */
  }
//}}}

// iDraw
//{{{
void cLcd::pixel (uint32_t colour, int16_t x, int16_t y) {

  if (mBuffered)
    rect (colour, x, y, 1, 1);
  else
    *(uint32_t*)(curFrameBufferAddress + (y*getWidth() + x)*4) = colour;
  }
//}}}
//{{{
void cLcd::rect (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  if (mBuffered) {
    *mDma2dCurBuf++ = colour;                                             // colour
    *mDma2dCurBuf++ = curFrameBufferAddress + ((y * getWidth()) + x) * 4; // fb start address
    *mDma2dCurBuf++ = getWidth() - width;                                 // stride
    *mDma2dCurBuf++ = (width << 16) | height;                             // width:height
    *mDma2dCurBuf++ = 0;                                                  // terminate
    *(mDma2dCurBuf-6) = kWipe;                                            // fill opCode

    if (mDma2dCurBuf > mDma2dHighWater)
      mDma2dHighWater = mDma2dCurBuf;
    }

  else {
    DMA2D->OCOLR = colour;
    DMA2D->OMAR  = curFrameBufferAddress + ((y * getWidth()) + x) * 4;
    DMA2D->OOR   = getWidth() - width;
    DMA2D->NLR   = (width << 16) | height;
    DMA2D->CR = DMA2D_R2M | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
    while (!(DMA2D->ISR & DMA2D_ISR_TCIF)) {}
    DMA2D->IFCR |= DMA2D_IFSR_CTEIF | DMA2D_IFSR_CTCIF | DMA2D_IFSR_CTWIF|
                   DMA2D_IFSR_CCAEIF | DMA2D_IFSR_CCTCIF | DMA2D_IFSR_CCEIF;
    }

  }
//}}}
//{{{
void cLcd::stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  if (mBuffered) {
    *mDma2dCurBuf++ = colour;                                             // colour
    *mDma2dCurBuf++ = curFrameBufferAddress + ((y * getWidth()) + x) * 4; // bgnd fb start address
    *mDma2dCurBuf++ = getWidth() - width;                                 // stride
    *mDma2dCurBuf++ = (width << 16) | height;                             // width:height
    *mDma2dCurBuf++ = (uint32_t)src;                                      // src start address
    *mDma2dCurBuf++ = 0;                                                  // terminate
    *(mDma2dCurBuf-7) = kStamp;                                           // stamp opCode

    if (mDma2dCurBuf > mDma2dHighWater)
      mDma2dHighWater = mDma2dCurBuf;
    }

  else {
    DMA2D->FGCOLR = colour;
    DMA2D->OMAR   = curFrameBufferAddress + ((y * getWidth()) + x) * 4;
    DMA2D->BGMAR  = curFrameBufferAddress + ((y * getWidth()) + x) * 4;
    DMA2D->OOR    = getWidth() - width;
    DMA2D->BGOR   = getWidth() - width;
    DMA2D->NLR    = (width << 16) | height;
    DMA2D->FGMAR  = (uint32_t)src;
    DMA2D->CR = DMA2D_M2M_BLEND | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE | DMA2D_CR_START;
    while (!(DMA2D->ISR & DMA2D_ISR_TCIF)) {}
    DMA2D->IFCR |= DMA2D_IFSR_CTEIF | DMA2D_IFSR_CTCIF | DMA2D_IFSR_CTWIF|
                   DMA2D_IFSR_CCAEIF | DMA2D_IFSR_CCTCIF | DMA2D_IFSR_CCEIF;
    }
  }
//}}}
//{{{
int cLcd::text (uint32_t colour, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  for (unsigned int i = 0; i < str.size(); i++) {
    if ((str[i] >= 0x20) && (str[i] <= 0x7F)) {
      auto fontChar = chars[str[i] - 0x20];
      if (!fontChar) {
        FT_Set_Pixel_Sizes (FTface, 0, fontHeight);
        FT_Load_Char (FTface, str[i], FT_LOAD_RENDER);

        // cache char info
        fontChar = (tFontChar*)pvPortMalloc (sizeof(tFontChar));
        chars[str[i] - 0x20] = fontChar;
        fontChar->left = FTglyphSlot->bitmap_left;
        fontChar->top = FTglyphSlot->bitmap_top;
        fontChar->pitch = FTglyphSlot->bitmap.pitch;
        fontChar->rows = FTglyphSlot->bitmap.rows;
        fontChar->advance = FTglyphSlot->advance.x / 64;
        fontChar->bitmap = nullptr;

        if (FTglyphSlot->bitmap.buffer) {
          // cache char bitmap
          fontChar->bitmap = (uint8_t*)pvPortMalloc (FTglyphSlot->bitmap.pitch * FTglyphSlot->bitmap.rows);
          memcpy (fontChar->bitmap, FTglyphSlot->bitmap.buffer, FTglyphSlot->bitmap.pitch * FTglyphSlot->bitmap.rows);
          }
        }

      if (x + fontChar->left + fontChar->pitch >= width)
        break;
      else if (fontChar->bitmap)
        stampClipped (colour, fontChar->bitmap, x + fontChar->left, y + fontHeight - fontChar->top, fontChar->pitch, fontChar->rows);

      x += fontChar->advance;
      }
    }

  return x;
  }
//}}}
//{{{
void cLcd::copy (uint32_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {
  }
//}}}
//{{{
void cLcd::copy (ID2D1Bitmap* bitMap, int16_t x, int16_t y, uint16_t width, uint16_t height) {
  }
//}}}

//{{{
void cLcd::pixelClipped (uint32_t colour, int16_t x, int16_t y) {

    rectClipped (colour, x, y, 1, 1);
  }
//}}}
//{{{
void cLcd::stampClipped (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  if (!width || !height || x < 0)
    return;

  if (y < 0) {
    // top clip
    if (y + height <= 0)
      return;
    height += y;
    src += -y * width;
    y = 0;
    }

  if (y + height > getHeight()) {
    // bottom yclip
    if (y >= getHeight())
      return;
    height = getHeight() - y;
    }

  stamp (colour, src, x, y, width, height);
  }
//}}}
//{{{
void cLcd::rectClipped (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) {

  if (x >= getWidth())
    return;
  if (y >= getHeight())
    return;

  int xend = x + width;
  if (xend <= 0)
    return;

  int yend = y + height;
  if (yend <= 0)
    return;

  if (x < 0)
    x = 0;
  if (xend > getWidth())
    xend = getWidth();

  if (y < 0)
    y = 0;
  if (yend > getHeight())
    yend = getHeight();

  if (!width)
    return;
  if (!height)
    return;

  rect (colour, x, y, xend - x, yend - y);
  }
//}}}
//{{{
void cLcd::rectOutline (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t thickness) {

  rectClipped (colour, x, y, width, thickness);
  rectClipped (colour, x + width - thickness, y, thickness, height);
  rectClipped (colour, x, y + height - thickness, width, thickness);
  rectClipped (colour, x, y, thickness, height);
  }
//}}}
//{{{
void cLcd::clear (uint32_t colour) {

  rect (colour, 0, 0, getWidth(), getHeight());
  }
//}}}

//{{{
void cLcd::ellipse (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

  if (!xradius)
    return;
  if (!yradius)
    return;

  int x1 = 0;
  int y1 = -yradius;
  int err = 2 - 2*xradius;
  float k = (float)yradius / xradius;

  do {
    rectClipped (colour, (x-(uint16_t)(x1 / k)), y + y1, (2*(uint16_t)(x1 / k) + 1), 1);
    rectClipped (colour, (x-(uint16_t)(x1 / k)), y - y1, (2*(uint16_t)(x1 / k) + 1), 1);

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
void cLcd::ellipseOutline (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) {

  if (xradius && yradius) {
    int x1 = 0;
    int y1 = -yradius;
    int err = 2 - 2*xradius;
    float k = (float)yradius / xradius;

    do {
      rectClipped (colour, x - (uint16_t)(x1 / k), y + y1, 1, 1);
      rectClipped (colour, x + (uint16_t)(x1 / k), y + y1, 1, 1);
      rectClipped (colour, x + (uint16_t)(x1 / k), y - y1, 1, 1);
      rectClipped (colour, x - (uint16_t)(x1 / k), y - y1, 1, 1);

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
void cLcd::line (uint32_t colour, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {

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
    rectClipped (colour, x, y, 1, 1);   /* Draw the current pixel */
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

// private
//{{{
void cLcd::init (std::string title, bool buffered) {

  mBuffered = buffered;
  mDrawBuffer = !mDrawBuffer;
  ltdcInit (mBuffer[mDrawBuffer]);

  //  dma2d init
  // unchanging dma2d regs
  DMA2D->OPFCCR  = DMA2D_INPUT_ARGB8888; // bgnd fb ARGB
  DMA2D->BGPFCCR = DMA2D_INPUT_ARGB8888;
  DMA2D->FGPFCCR = DMA2D_INPUT_A8;       // src alpha
  DMA2D->FGOR    = 0;              // src stride
  //DMA2D->AMTCR = 0x1001;

  // zero out first opcode, point past it
  mDma2dBuf = (uint32_t*)DMA2D_BUFFER; // pvPortMalloc (8192 * 4);
  mDma2dCurBuf = mDma2dBuf;
  *mDma2dCurBuf++ = 0;
  mDma2dHighWater = mDma2dCurBuf;
  mDma2dIsrBuf = mDma2dBuf;
  mDma2dTimeouts = 0;

  if (mBuffered) {
    //{{{  dma2d IRQ init
    osSemaphoreDef (dma2dSem);
    mDma2dSem = osSemaphoreCreate (osSemaphore (dma2dSem), -1);

    HAL_NVIC_SetPriority (DMA2D_IRQn, 0x0F, 0);
    HAL_NVIC_EnableIRQ (DMA2D_IRQn);
    }
    //}}}

  clear (COL_BLACK);
  sendWait();
  displayOn();

  // font init
  setFont (freeSansBold, freeSansBold_len);
  for (auto i = 0; i < maxChars; i++)
    chars[i] = nullptr;

  setTitle (title);
  }
//}}}
//{{{
void cLcd::ltdcInit (uint32_t frameBufferAddress) {

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

  layerInit (0, frameBufferAddress);
  setFrameBufferAddress[1] = frameBufferAddress;
  showFrameBufferAddress[1] = frameBufferAddress;
  showAlpha[1] = 0;

  if (mBuffered) {
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
    }
    //}}}
  }
//}}}
//{{{
void cLcd::layerInit (uint8_t layer, uint32_t frameBufferAddress) {

  LTDC_LayerCfgTypeDef* curLayerCfg = &hLtdc.LayerCfg[layer];

  curLayerCfg->WindowX0 = 0;
  curLayerCfg->WindowX1 = getWidth();
  curLayerCfg->WindowY0 = 0;
  curLayerCfg->WindowY1 = getHeight();

  curLayerCfg->PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
  curLayerCfg->FBStartAdress = (uint32_t)frameBufferAddress;

  curLayerCfg->Alpha = 255;
  curLayerCfg->Alpha0 = 0;

  curLayerCfg->Backcolor.Blue = 0;
  curLayerCfg->Backcolor.Green = 0;
  curLayerCfg->Backcolor.Red = 0;

  curLayerCfg->BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  curLayerCfg->BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;

  curLayerCfg->ImageWidth = getWidth();
  curLayerCfg->ImageHeight = getHeight();

  HAL_LTDC_ConfigLayer (&hLtdc, curLayerCfg, layer);

  // local state
  curFrameBufferAddress = frameBufferAddress;
  setFrameBufferAddress[layer] = frameBufferAddress;
  showFrameBufferAddress[layer] = frameBufferAddress;
  showAlpha[layer] = 255;
  }
//}}}
//{{{
void cLcd::setFont (const uint8_t* font, int length) {

  FT_Init_FreeType (&FTlibrary);
  FT_New_Memory_Face (FTlibrary, (FT_Byte*)font, length, 0, &FTface);
  FTglyphSlot = FTface->glyph;
  //FT_Done_Face(face);
  //FT_Done_FreeType (library);
  }
//}}}

//{{{
void cLcd::setLayer (uint8_t layer, uint32_t frameBufferAddress) {

  curFrameBufferAddress = frameBufferAddress;
  setFrameBufferAddress[layer] = frameBufferAddress;
  }
//}}}
//{{{
void cLcd::showLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha) {

  showFrameBufferAddress[layer] = frameBufferAddress;
  showAlpha[layer] = alpha;
  }
//}}}

//{{{
void cLcd::send() {

  // send first opCode using IRQhandler
  if (mBuffered)
    LCD_DMA2D_IRQHandler();
  }
//}}}
//{{{
void cLcd::wait() {

  if (mBuffered) {
    if (osSemaphoreWait (mDma2dSem, 500) != osOK)
      mDma2dTimeouts++;

    // zero out first opcode, point past it
    mDma2dCurBuf = mDma2dBuf;
    *mDma2dCurBuf++ = 0;
    }
  }
//}}}
//{{{
void cLcd::sendWait() {

  send();
  wait();
  }
//}}}

//{{{
void cLcd::reset() {

  for (auto i = 0; i < mMaxLine; i++)
    mLines[i].clear();

  mStartTime = osKernelSysTick();
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

  mStringPos = getLineHeight()*3;

  auto numDrawLines = getHeight() / getLineHeight();
  if (mShowTitle && !mTitle.empty())
    numDrawLines--;
  if (mShowLcdStats)
    numDrawLines--;
  if (mShowFooter)
    numDrawLines--;

  mNumDrawLines = numDrawLines;
  }
//}}}
