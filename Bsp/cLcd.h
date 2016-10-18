#pragma once
#include <string>
#include "../../shared/widgets/iDraw.h"

class cLcd : public iDraw {
public:
  cLcd (uint32_t buffer0, uint32_t buffer1);
  virtual ~cLcd();

  // static members
  static cLcd* create (std::string title, bool buffered = true);
  static cLcd* get() { return mLcd; }

  // static gets
  static uint16_t getWidth() { return 480; }
  static uint16_t getHeight() { return 272; }
  static uint8_t getFontHeight() { return 16; }
  static uint8_t getLineHeight() { return 19; }

  // static string utils
  static std::string hex (int value, uint8_t width = 0);
  static std::string dec (int value, uint8_t width = 0, char fill = ' ');
  static void debug (uint32_t colour, std::string str, bool newLine = true) { get()->info (colour, str, newLine); }
  static void debug (std::string str) { get()->info (str); }

  // sets
  void setTitle (std::string title);
  void setShowDebug (bool title, bool info, bool lcdStats, bool footer);

  // string
  void info (uint32_t colour, std::string str, bool newLine = true);
  void info (std::string str, bool newLine = true);

  // touch
  void press (int pressCount, int x, int y, int z, int xinc, int yinc);

  void startRender();
  void renderCursor (uint32_t colour, int16_t x, int16_t y, int16_t z);
  void endRender();

  void displayOn();
  void displayOff();

  // iDraw
  virtual void pixel (uint32_t colour, int16_t x, int16_t y);
  virtual void rect (uint32_t col, int16_t x, int16_t y, uint16_t width, uint16_t height);
  virtual void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height);
  virtual int text (uint32_t col, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height);
  virtual void copy (uint32_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height);
  virtual void copy (ID2D1Bitmap* bitMap, int16_t x, int16_t y, uint16_t width, uint16_t height);
  virtual void pixelClipped (uint32_t colour, int16_t x, int16_t y);
  virtual void stampClipped (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height);
  virtual void rectClipped (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height);
  virtual void rectOutline (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t thickness);
  virtual void clear (uint32_t colour);
  virtual void ellipse (uint32_t colour, int16_t x, int16_t Ypos, uint16_t xradius, uint16_t yradius);
  virtual void ellipseOutline (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius);
  virtual void line (uint32_t colour, int16_t x1, int16_t y1, int16_t x2, int16_t y2);

private:
  void init (std::string title, bool buffered);
  void ltdcInit (uint32_t frameBufferAddress);
  void layerInit (uint8_t layer, uint32_t frameBufferAddress);

  void setFont (const uint8_t* font, int length);
  void setLayer (uint8_t layer, uint32_t frameBufferAddress);
  void showLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha);

  void send();
  void wait();
  void sendWait();

  void reset();
  void displayTop();
  void displayTail();
  void updateNumDrawLines();

  // static vars
  static cLcd* mLcd;

  //{{{  vars
  bool mBuffered = true;
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
  uint32_t* mDma2dHighWater = nullptr;
  uint32_t mDma2dTimeouts= 0;
  //}}}
  };
