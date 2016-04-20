// cWidget.h
#pragma once
#include "cLcd.h"

class cWidget {
public:
  cWidget (uint16_t xlen) : mXlen(xlen) {}
  cWidget (uint32_t colour, uint16_t xlen) : mColour(colour), mXlen(xlen) {}
  cWidget (uint32_t colour, uint16_t xlen, uint16_t ylen) : mColour(colour), mXlen(xlen), mYlen(ylen) {}
  virtual ~cWidget() {}

  int16_t getXorg() { return mXorg; }
  int16_t getYorg() { return mYorg; }
  int16_t getWidth() { return mXlen; }
  int16_t getHeight() { return mYlen; }
  int getPressed() { return mPressed; }

  void setOrg (int16_t x, int16_t y) { mXorg = x; mYorg = y; }

  virtual void setColour (uint32_t colour) { mColour = colour; }

  //{{{
  virtual bool picked (int16_t x, int16_t y, int16_t z) {
    return (x >= mXorg) && (x < mXorg + mXlen) && (y >= mYorg) && (y < mYorg + mYlen);
    }
  //}}}
  virtual void pressed (int16_t x, int16_t y) { mPressed++; }
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {}
  virtual void released (int16_t x, int16_t y) { mPressed = 0; }

  //{{{
  virtual void draw (cLcd* lcd) {
    lcd->rectClipped (mPressed ? LCD_LIGHTRED : mColour, mXorg+1, mYorg+1, mXlen-1, mYlen-1);
    }
  //}}}

protected:
  uint32_t mColour = LCD_LIGHTGREY;

  int16_t mXorg = 0;
  int16_t mYorg = 0;
  int16_t mXlen = 100;
  int16_t mYlen = 19;

  int mPressed = 0;
  };
