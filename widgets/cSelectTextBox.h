// cTextBox.h
#pragma once
#include "cTextBox.h"

class cSelectTextBox : public cTextBox {
public:
  cSelectTextBox (std::string text, std::string& selectedText, bool& changedFlag) :
    cTextBox (text, cLcd::getWidth() - 20 - 1),
    mSelectedText(selectedText), mChangedFlag(changedFlag) { mChangedFlag = false; }
  virtual ~cSelectTextBox() {}

  virtual void released (int16_t x, int16_t y) {
    mSelectedText = mText;
    mChangedFlag = true;
    cTextBox::released (x, y);
    }

private:
  std::string& mSelectedText;
  bool& mChangedFlag;
  };
