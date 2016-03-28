#pragma once
#include <string>
#include "stm32746g_discovery.h"
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}

//{{{  LCD_colours defines
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

extern LTDC_HandleTypeDef hLtdc;
void LCD_LTDC_IRQHandler();
void LCD_DMA2D_IRQHandler();

void lcdInit (uint32_t frameBufferAddress);
void lcdLayerInit (uint8_t layer, uint32_t frameBufferAddress);
void lcdDisplayOn();
void lcdDisplayOff();

inline uint16_t lcdGetXSize() { return 480; }
inline uint16_t lcdGetYSize() { return 272; }

void lcdDebug (int16_t y);

void lcdSetLayer (uint8_t layer, uint32_t frameBufferAddress);
void lcdShowLayer (uint8_t layer, uint32_t frameBufferAddress, uint8_t alpha);
void lcdFrameSync();

void lcdSetFont (const uint8_t* font, int length);

void lcdStamp (uint32_t col, uint8_t* src, int16_t x, int16_t y, uint16_t xpitch, uint16_t ylen);
void lcdString (uint32_t col, int fontHeight, std::string str, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);

void lcdRect (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);
void lcdClipRect (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);
void lcdEllipse (uint32_t col, int16_t x, int16_t Ypos, uint16_t xradius, uint16_t yradius);
void lcdClear (uint32_t col);

void lcdPixel (uint32_t col, int16_t x, int16_t y);
void lcdClippedPixel (uint32_t col, int16_t x, int16_t y);
void lcdLine (uint32_t col, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
void lcdOutRect (uint32_t col, int16_t x, int16_t y, uint16_t xlen, uint16_t ylen);
void lcdOutEllipse (uint32_t col, int16_t x, int16_t y, uint16_t xradius, uint16_t yradius);

void lcdSend();
void lcdWait();
void lcdSendWait();

//{{{
#ifdef __cplusplus
  }
#endif
//}}}
