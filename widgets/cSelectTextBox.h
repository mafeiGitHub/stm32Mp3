// cSelectTextBox.h
#pragma once
#include "cTextBox.h"

class cSelectTextBox : public cTextBox {
public:
  cSelectTextBox (std::string text, std::string& selectedText, bool& changedFlag, uint16_t width) :
    cTextBox (text, width), mSelectedText(selectedText), mChangedFlag(changedFlag) { mChangedFlag = false; }
  virtual ~cSelectTextBox() {}

  virtual void released() {
    mSelectedText = mText;
    mChangedFlag = true;
    cTextBox::released();
    }

  virtual void draw (cLcd* lcd) {
    setTextColour (mText == mSelectedText ? LCD_WHITE : LCD_DARKGREY);
    cTextBox::draw (lcd);
    }

private:
  std::string& mSelectedText;
  bool& mChangedFlag;
  };
