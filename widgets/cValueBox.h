// cTextBox.h
#pragma once
#include "cWidget.h"

class cValueBox : public cWidget {
public:
  cValueBox (float& value, bool& changedFlag, uint32_t colour, uint16_t width, uint16_t height) :
    cWidget (colour, width, height), mValueRef(value), mChangedFlagRef(changedFlag) { mChangedFlagRef = false; }
  virtual ~cValueBox() {}

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
      lcd->rectClipped (mPressed ? LCD_LIGHTRED : mColour, mX, mY, int(mWidth * limitValue (mValueRef)), mHeight);
    else
      lcd->rectClipped (mPressed ? LCD_LIGHTRED : mColour, mX, mY, mWidth, int(mHeight * limitValue (mValueRef)));
    }
  //}}}

private :
  //{{{
  void setValue (float value) {

    value = limitValue (value);
    if (value != mValueRef) {
      mValueRef = value;
      mChangedFlagRef = true;
      }
    }
  //}}}
  float limitValue (float value) { return value > mMaxValue ? mMaxValue : (value < mMinValue ? mMinValue : value); }

  float& mValueRef;
  bool& mChangedFlagRef;

  float mMinValue = 0.0f;
  float mMaxValue = 1.0f;
  };
