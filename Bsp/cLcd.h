#pragma once
#include <stdint.h>
#include <string>
#include "../../shared/widgets/iDraw.h"

class cFontChar;
class cLcd : public iDraw {
public:
  cLcd (uint32_t buffer0, uint32_t buffer1);
  virtual ~cLcd();

  // static members
  static cLcd* get() { return mLcd; }

  void init (std::string title);

  // sets
  void setShowDebug (bool title, bool info, bool lcdStats, bool footer);

  // string
  void info (std::string str);
  void info (uint32_t colour, std::string str);

  // touch
  void press (int pressCount, int16_t x, int16_t y, uint16_t z, int16_t xinc, int16_t yinc);

  void startRender();
  void renderCursor (uint32_t colour, int16_t x, int16_t y, int16_t z);
  void endRender (bool forceInfo);

  void displayOn();
  void displayOff();

  // iDraw
  #ifdef STM32F746G_DISCO
    uint16_t getLcdWidthPix() { return 480; }
    uint16_t getLcdHeightPix() { return 272; }
  #else
    uint16_t getLcdWidthPix() { return 800; }
    uint16_t getLcdHeightPix() { return 480; }
  #endif

  void pixel (uint32_t colour, int16_t x, int16_t y);
  void rect (uint32_t col, int16_t x, int16_t y, uint16_t width, uint16_t height);
  void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height);
  int text (uint32_t col, uint16_t fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height);

  void copy (uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height);
  void copy (uint8_t* src, int16_t srcx, int16_t srcy, uint16_t srcWidth, uint16_t srcHeight,
             int16_t dstx, int16_t dsty, uint16_t dstWidth, uint16_t dstHeight);

private:
  void ltdcInit (uint32_t frameBufferAddress);
  void layerInit (uint8_t layer, uint32_t frameBufferAddress);

  #ifdef STM32F769I_DISCO
    void dsiWriteCmd (uint32_t NbrParams, uint8_t* pParams);
    void otm8009aInit (bool landscape);
  #endif

  void setLayer (uint8_t layer, uint32_t frameBufferAddress);
  void showLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha);

  void reset();
  void displayTop();
  void displayTail();
  void updateNumDrawLines();
  cFontChar* loadChar (uint16_t fontHeight, char ch);

  // static vars
  static cLcd* mLcd;

  //{{{  vars
  int mStartTime = 0;

  float mFirstLine = 0;
  int mNumDrawLines = 0;
  int mStringPos = 0;

  bool mShowTitle = true;
  bool mShowInfo = true;
  bool mShowLcdStats = false;
  bool mShowFooter = true;

  std::string mTitle;

  int mLastLine = -1;
  int mMaxLine = 256;

  //{{{
  class cLine {
  public:
    cLine() {}
    ~cLine() {}

    //{{{
    void clear() {
      mTime = 0;
      mColour = COL_WHITE;
      mString = "";
      }
    //}}}

    int mTime = 0;
    int mColour = COL_WHITE;
    std::string mString;
    };
  //}}}
  cLine mLines[256];

  int mDrawStartTime = 0;
  int mDrawTime = 0;
  bool mDrawBuffer = false;
  uint32_t mBuffer[2] = {0,0};

  uint32_t* mDma2dCurBuf = nullptr;
  uint32_t mDma2dTimeouts = 0;

  uint32_t mCurFrameBufferAddress = 0;
  uint32_t mSetFrameBufferAddress[2];
  uint32_t mCurDstColour = 0;
  uint32_t mCurSrcColour = 0;
  uint32_t mDstStride = 0;
  //}}}
  };
