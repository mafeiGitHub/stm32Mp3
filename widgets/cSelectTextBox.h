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
    mMoved = false;
    mMove = 0;

    cTextBox::pressed (x, y);
    if (mParent)
      mParent->pressed (x, y);
    }
  //}}}
  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {

    mMove += yinc;
    if (abs(mMove) > 2) {
      mMoved = true;
      mOn = false;
      }
    if (mParent && mMoved)
      mParent->moved (x, y, z, xinc, yinc);
    }
  //}}}
  //{{{
  virtual void released() {
    if (!mMoved) {
      mSelectedIdRef = mSelectId;
      mChangedFlagRef = true;
      }

    if (mParent)
      mParent->released();

    cTextBox::released();

    mMoved = false;
    mMove = 0;
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
  bool mMoved = false;
  int mMove = 0.0f;
  };
