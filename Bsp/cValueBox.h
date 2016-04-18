// cValueBox.h
#pragma once
//{{{  includes
#include <string>
#include "cWidget.h"
//}}}

class cValueBox : public cWidget {
public:
  cValueBox (float value, uint32_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) :
    cWidget (colour, xorg, yorg, xlen, ylen), mValue(value) {}
  ~cValueBox() {}

  //{{{
  float getValue() {
    return mValue;
    }
  //}}}
  //{{{
  virtual void setValue (float value) {
    mValue = value;
    }
  //}}}

  //{{{
  virtual void pressed (int16_t x, int16_t y) {

    cWidget::pressed (x, y);
    if (mXlen > mYlen)
      setValue ((float)x / (float)mXlen);
    else
      setValue ((float)y / (float)mYlen);
    }
  //}}}
  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t xinc, int16_t yinc) {
    cWidget::moved (x, y, xinc, yinc);
    if (mXlen > mYlen)
      setValue ((float)x / (float)mXlen);
    else
      setValue ((float)y / (float)mYlen);
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {
    if (mXlen > mYlen)
      lcd->rect (mPressed ? LCD_LIGHTRED : mColour, mXorg, mYorg, int(mXlen * mValue), mYlen);
    else
      lcd->rect (mPressed ? LCD_LIGHTRED : mColour, mXorg, mYorg, mXlen, int(mYlen * mValue));
    }
  //}}}

protected:
  float mValue = 0;
  };
