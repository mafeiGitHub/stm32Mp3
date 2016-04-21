// cTextBox.h
#pragma once
#include "cTextBox.h"

class cSelectTextBox : public cTextBox {
public:
  cSelectTextBox (std::string text, std::string& selectedText, bool& changedFlag) :
    cTextBox (text, cLcd::getWidth() - 20 - 1),
    mSelectedText(selectedText), mChangedFlag(changedFlag) { mChangedFlag = false; }
  virtual ~cSelectTextBox() {}

  virtual void released() {
    mSelectedText = mText;
    mChangedFlag = true;
    cTextBox::released();
    }

private:
  std::string& mSelectedText;
  bool& mChangedFlag;
  };
