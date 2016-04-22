#pragma once
#include <string>
//{{{  LCD colour defines
#define LCD_BLACK         0xFF000000
#define LCD_BLUE          0xFF0000FF
#define LCD_GREEN         0xFF00FF00
#define LCD_RED           0xFFFF0000
#define LCD_CYAN          0xFF00FFFF
#define LCD_MAGENTA       0xFFFF00FF
#define LCD_YELLOW        0xFFFFFF00
#define LCD_WHITE         0xFFFFFFFF

#define LCD_DARKGREY      0xFF404040
#define LCD_GREY          0xFF808080
#define LCD_LIGHTGREY     0xFFD3D3D3

#define LCD_LIGHTBLUE     0xFF8080FF
#define LCD_LIGHTGREEN    0xFF80FF80
#define LCD_LIGHTRED      0xFFFF8080
#define LCD_LIGHTCYAN     0xFF80FFFF
#define LCD_LIGHTMAGENTA  0xFFFF80FF
#define LCD_LIGHTYELLOW   0xFFFFFF80

#define LCD_DARKBLUE      0xFF000080
#define LCD_DARKGREEN     0xFF008000
#define LCD_DARKRED       0xFF800000
#define LCD_DARKCYAN      0xFF008080
#define LCD_DARKMAGENTA   0xFF800080
#define LCD_DARKYELLOW    0xFF808000

#define LCD_BROWN         0xFFA52A2A
#define LCD_ORANGE        0xFFFFA500
//}}}
class cLcd {
public:
  // static members
  static cLcd* create (std::string title, bool buffered = true);
  static cLcd* get() { return mLcd; }

  // static gets
  static uint16_t getWidth() { return 480; }
  static uint16_t getHeight() { return 272; }
  static uint8_t getFontHeight() { return 15; }
  static uint8_t getLineHeight() { return 18; }

  // static string utils
  static std::string hexStr (int value, uint8_t width = 0);
  static std::string intStr (int value, uint8_t width = 0, char fill = ' ');
  static void debug (uint32_t colour, std::string str, bool newLine = true) { get()->info (colour, str, newLine); }
  static void debug (std::string str) { get()->info (str); }

  // sets
  void setTitle (std::string title);
  void setShowDebug (bool title, bool info, bool lcdStats, bool footer);

  // string
  void info (uint32_t colour, std::string str, bool newLine = true);
  void info (std::string str, bool newLine = true);
  int string (uint32_t col, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height);

  // touch
  void press (int pressCount, int x, int y, int z, int xinc, int yinc);

  // draws
  void pixel (uint32_t colour, int16_t x, int16_t y);
  void pixelClipped (uint32_t colour, int16_t x, int16_t y);
  void rect (uint32_t col, int16_t x, int16_t y, uint16_t width, uint16_t height);
  void rectClipped (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height);
  void rectOutline (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height);
  void clear (uint32_t colour);
  void ellipse (uint32_t colour, int16_t x, int16_t Ypos, uint16_t xradius, uint16_t yradius);
  void ellipseOutline (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius);
  void line (uint32_t colour, int16_t x1, int16_t y1, int16_t x2, int16_t y2);

  void startDraw();
  void drawCursor (uint32_t colour, int16_t x, int16_t y, int16_t z);
  void endDraw();
  void draw();

  void displayOn();
  void displayOff();

private:
  cLcd (uint32_t buffer0, uint32_t buffer1);
  ~cLcd() {}

  void init (std::string title, bool buffered);
  void ltdcInit (uint32_t frameBufferAddress);
  void layerInit (uint8_t layer, uint32_t frameBufferAddress);

  void setFont (const uint8_t* font, int length);
  void setLayer (uint8_t layer, uint32_t frameBufferAddress);
  void showLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha);

  void stampClipped (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height);

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
      mColour = LCD_WHITE;
      mString = "";
      }
    //}}}

    int mTime = 0;
    int mColour = LCD_WHITE;
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
