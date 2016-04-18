// cWidget.h
#pragma once
#include "cLcd.h"

class cWidget {
public:
  cWidget (uint32_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) :
    mColour(colour), mXorg(xorg), mYorg(yorg), mXlen(xlen), mYlen(ylen) {}
  ~cWidget() {}

  int16_t getXorg() { return mXorg; }
  int16_t getYorg() { return mYorg; }
  int16_t getXlen() { return mXlen; }
  int16_t getYlen() { return mYlen; }
  int16_t getPressed() { return mPressed; }

  virtual void setColour (uint32_t colour) { mColour = colour; }
  virtual bool picked (int16_t x, int16_t y) { return (x >= mXorg) && (x < mXorg + mXlen) && (y >= mYorg) && (y < mYorg + mYlen); }
  virtual void pressed (int16_t x, int16_t y) { mPressed = true; }
  virtual void moved (int16_t x, int16_t y, int16_t xinc, int16_t yinc) {}
  virtual void released (int16_t x, int16_t y) { mPressed = false; }
  virtual void draw (cLcd* lcd) { lcd->rect (mPressed ? LCD_LIGHTRED : mColour, mXorg, mYorg, mXlen, mYlen); }

protected:
  uint32_t mColour = LCD_LIGHTGREY;

  int16_t mXorg = 0;
  int16_t mYorg = 0;
  int16_t mXlen = 0;
  int16_t mYlen = 0;

  bool mPressed = false;
  };
