// iDraw.h
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
class iDraw {
public:
  virtual ~iDraw() {}

  virtual int text (uint32_t col, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t width, uint16_t height) = 0;
  virtual int measure (int fontHeight, std::string str) = 0;

  virtual void pixel (uint32_t colour, int16_t x, int16_t y) = 0;
  virtual void stamp (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) = 0;
  virtual void rect (uint32_t col, int16_t x, int16_t y, uint16_t width, uint16_t height) = 0;
  virtual void pixelClipped (uint32_t colour, int16_t x, int16_t y) = 0;
  virtual void stampClipped (uint32_t colour, uint8_t* src, int16_t x, int16_t y, uint16_t width, uint16_t height) = 0;
  virtual void rectClipped (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) = 0;
  virtual void rectOutline (uint32_t colour, int16_t x, int16_t y, uint16_t width, uint16_t height) = 0;
  virtual void clear (uint32_t colour) = 0;
  virtual void ellipse (uint32_t colour, int16_t x, int16_t Ypos, uint16_t xradius, uint16_t yradius) = 0;
  virtual void ellipseOutline (uint32_t colour, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius) = 0;
  virtual void line (uint32_t colour, int16_t x1, int16_t y1, int16_t x2, int16_t y2) = 0;
  };
