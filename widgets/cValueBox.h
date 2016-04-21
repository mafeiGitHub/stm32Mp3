// cValueBox.h
#pragma once
#include <string>
#include "cWidget.h"

class cValueBox : public cWidget {
public:
  cValueBox (float value, uint32_t colour, uint16_t width) : cWidget (colour, width), mValue(value) {}
  cValueBox (float value, uint32_t colour, uint16_t width, uint16_t height) : cWidget (colour, width, height), mValue(value) {}
  virtual ~cValueBox() {}

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
  virtual bool setValue (float value, float minValue, float maxValue) {

    if (value < 0.0f)
      value = 0.0f;
    else if (value > 1.0f)
      value = 1.0f;

    bool changed = value != mValue;
    if (changed)
      mValue = value;

    return changed;
    }
  //}}}

  //{{{
  virtual void pressed (int16_t x, int16_t y) {

    cWidget::pressed (x, y);
    if (mWidth > mHeight)
      setValue ((float)x / (float)mWidth);
    else
      setValue ((float)y / (float)mHeight);
    }
  //}}}
  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {
    cWidget::moved (x, y, z, xinc, yinc);
    if (mWidth > mHeight)
      setValue ((float)x / (float)mWidth);
    else
      setValue ((float)y / (float)mHeight);
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {
    if (mWidth > mHeight)
      lcd->rectClipped (mPressed ? LCD_LIGHTRED : mColour, mX, mY, int(mWidth * mValue), mHeight);
    else
      lcd->rectClipped (mPressed ? LCD_LIGHTRED : mColour, mX, mY, mWidth, int(mHeight * mValue));
    }
  //}}}

protected:
  float mValue = 0;
  };
