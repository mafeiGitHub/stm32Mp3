// cTextBox.h
#pragma once
#include "cValueBox.h"

class cValueRefBox : public cValueBox {
public:
  //{{{
  cValueRefBox (float& value, bool& changedFlag, uint32_t colour, uint16_t width, uint16_t height) :
    cValueBox (value, colour, width, height), mValueRef(value), mChangedFlag(changedFlag) { mChangedFlag = false; }
  //}}}
  virtual ~cValueRefBox() {}

  virtual void setValue (float value) {
    auto oldValue = getValue();
    if (cValueBox::setValue (value, 0.0f, 1.0f)) {
      mValueRef = getValue();
      if (getValue() != oldValue)
        mChangedFlag = true;
      }
    }

  virtual void draw (cLcd* lcd) {
    mValue = mValueRef;
    cValueBox::draw (lcd);
    }

private :
  float& mValueRef;
  bool& mChangedFlag;
  };
