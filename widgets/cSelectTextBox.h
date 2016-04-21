// cSelectTextBox.h - select text box, selectedText holds current select text
#pragma once
#include "cTextBox.h"

class cSelectTextBox : public cTextBox {
public:
  cSelectTextBox (std::string text, std::string& selectedText, bool& changedFlag, uint16_t width) :
    cTextBox (text, width), mSelectedTextRef(selectedText), mChangedFlag(changedFlag) { mChangedFlag = false; }
  virtual ~cSelectTextBox() {}

  virtual void released() {
    mSelectedTextRef = mText;
    mChangedFlag = true;
    cTextBox::released();
    }

  virtual void draw (cLcd* lcd) {
    setTextColour (mText == mSelectedTextRef ? LCD_WHITE : LCD_DARKGREY);
    cTextBox::draw (lcd);
    }

private:
  std::string& mSelectedTextRef;
  bool& mChangedFlag;
  };
