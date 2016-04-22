// cSelectTextBox.h - select text box
#pragma once
#include "cTextBox.h"

class cSelectTextBox : public cTextBox {
public:
  //{{{
  cSelectTextBox (std::string& text, int selectId, int& selectedId, bool& changedFlag, uint16_t width) :
    cTextBox (text, width), mSelectId(selectId), mSelectedIdRef(selectedId), mChangedFlagRef(changedFlag) {
      mChangedFlagRef = false;
      }
  //}}}
  virtual ~cSelectTextBox() {}

  //{{{
  void setText (std::string& text, int selectId) {
    cTextBox::setText (text);
    mSelectId = selectId;
    }
  //}}}

  //{{{
  virtual void pressed (int16_t x, int16_t y) {
    cTextBox::pressed (x, y);
    if (mParent)
      mParent->pressed (x, y);

    }
  //}}}
  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {
    if (mParent)
      mParent->moved (x, y,z, xinc, yinc);
    }
  //}}}
  //{{{
  virtual void released() {
    mSelectedIdRef = mSelectId;
    mChangedFlagRef = true;
    cTextBox::released();
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {
    setTextColour (mSelectId == mSelectedIdRef ? LCD_WHITE : LCD_DARKGREY);
    cTextBox::draw (lcd);
    }
  //}}}

private:
  int mSelectId;
  int& mSelectedIdRef;
  bool& mChangedFlagRef;
  };
