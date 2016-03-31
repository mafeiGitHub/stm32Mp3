#pragma once
#include <string>

//{{{  LCD colour defines
#define LCD_BLUE          ((uint32_t)0xFF0000FF)
#define LCD_GREEN         ((uint32_t)0xFF00FF00)
#define LCD_RED           ((uint32_t)0xFFFF0000)
#define LCD_CYAN          ((uint32_t)0xFF00FFFF)
#define LCD_MAGENTA       ((uint32_t)0xFFFF00FF)
#define LCD_YELLOW        ((uint32_t)0xFFFFFF00)

#define LCD_LIGHTBLUE     ((uint32_t)0xFF8080FF)
#define LCD_LIGHTGREEN    ((uint32_t)0xFF80FF80)
#define LCD_LIGHTRED      ((uint32_t)0xFFFF8080)
#define LCD_LIGHTCYAN     ((uint32_t)0xFF80FFFF)
#define LCD_LIGHTMAGENTA  ((uint32_t)0xFFFF80FF)
#define LCD_LIGHTYELLOW   ((uint32_t)0xFFFFFF80)

#define LCD_DARKBLUE      ((uint32_t)0xFF000080)
#define LCD_DARKGREEN     ((uint32_t)0xFF008000)
#define LCD_DARKRED       ((uint32_t)0xFF800000)
#define LCD_DARKCYAN      ((uint32_t)0xFF008080)
#define LCD_DARKMAGENTA   ((uint32_t)0xFF800080)
#define LCD_DARKYELLOW    ((uint32_t)0xFF808000)

#define LCD_WHITE         ((uint32_t)0xFFFFFFFF)
#define LCD_LIGHTGREY     ((uint32_t)0xFFD3D3D3)
#define LCD_GREY          ((uint32_t)0xFF808080)
#define LCD_DARKGREY      ((uint32_t)0xFF404040)
#define LCD_BLACK         ((uint32_t)0xFF000000)

#define LCD_BROWN         ((uint32_t)0xFFA52A2A)
#define LCD_ORANGE        ((uint32_t)0xFFFFA500)

#define LCD_TRANSPARENT   ((uint32_t)0xFF000000)
//}}}
class cLcd {
public:
  cLcd();
  ~cLcd() {}

  void init (uint32_t buffer0, uint32_t buffer1);

  int getWidth() { return 480; }
  int getHeight() { return 272; }

  void setTitle (std::string title);
  void setShowTime (bool enable);
  void setShowDebug (bool enable);
  void setShowFooter (bool enable);

  std::string toString (int value);
  void text (uint32_t colour, std::string str);
  void text (std::string str);

  void pixel (uint32_t col, int16_t x, int16_t y);
  void pixelClipped (uint32_t col, int16_t x, int16_t y);
  void rect (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);
  void rectClipped (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);
  void rectOutline (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);
  void clear (uint32_t col);
  void ellipse (uint32_t col, int16_t x, int16_t Ypos, uint16_t xradius, uint16_t yradius);
  void ellipseOutline (uint32_t col, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius);
  void line (uint32_t col, int16_t x1, int16_t y1, int16_t x2, int16_t y2);

  void pressed (int pressCount, int x, int y, int xinc, int yinc);
  void startDraw();
  void endDraw();
  void displayOn();
  void displayOff();

private:
  void ltdcInit (uint32_t frameBufferAddress);
  void layerInit (uint8_t layer, uint32_t frameBufferAddress);

  void setFont (const uint8_t* font, int length);
  void setLayer (uint8_t layer, uint32_t frameBufferAddress);
  void showLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha);

  void stamp (uint32_t col, uint8_t* src, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);
  int string (uint32_t col, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);

  void send();
  void wait();
  void sendWait();

  void reset();
  void displayTop();
  void displayTail();
  void updateNumDrawLines();

  //{{{  vars
  int mStartTime = 0;

  float mFirstLine = 0;
  int mNumDrawLines = 0;
  int mFontHeight = 16;
  int mLineInc = 18;
  int mStringPos = 0;

  bool mShowTime = true;
  bool mShowDebug = false;
  bool mShowFooter = true;

  std::string mTitle;

  int mLastLine = -1;
  int mMaxLine = 500;

  //{{{
  class cLine {
  public:
    cLine() {}
    ~cLine() {}

    //{{{
    void clear() {
      mTime = 0;
      mColour = LCD_WHITE;
      mString = "";
      }
    //}}}

    int mTime = 0;
    int mColour = LCD_WHITE;
    std::string mString;
    };
  //}}}
  cLine mLines[500];

  int mDrawStartTime = 0;
  int mDrawTime = 0;
  bool mDrawBuffer = false;
  uint32_t mBuffer[2] = {0,0};
  //}}}
  };
