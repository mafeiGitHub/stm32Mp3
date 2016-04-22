// cSelectTextBox.h - select text box, selectedText holds current select text
#pragma once
#include "cTextBox.h"

class cSelectTextBox : public cTextBox {
public:
  cSelectTextBox (std::string& text, std::string& selectedText, bool& changedFlag, uint16_t width) :
    cTextBox (text, width), mSelectedTextRef(selectedText), mChangedFlagRef(changedFlag) { mChangedFlagRef = false; }
  virtual ~cSelectTextBox() {}

  virtual void pressed (int16_t x, int16_t y) {
    cTextBox::pressed (x, y);
    if (mParent)
      mParent->pressed (x, y);

    }

  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {
    if (mParent)
      mParent->moved (x, y,z, xinc, yinc);
    }

  virtual void released() {
    mSelectedTextRef = mTextRef;
    mChangedFlagRef = true;
    cTextBox::released();
    }

  virtual void draw (cLcd* lcd) {
    setTextColour (mTextRef == mSelectedTextRef ? LCD_WHITE : LCD_DARKGREY);
    cTextBox::draw (lcd);
    }

private:
  std::string& mSelectedTextRef;
  bool& mChangedFlagRef;
  };
