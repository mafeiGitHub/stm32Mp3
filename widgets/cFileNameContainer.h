// cFileNameContainer.h - fileName container
#pragma once
#include "cContainer.h"

class cFileNameContainer : public cContainer {
public:
  //{{{
  cFileNameContainer (std::vector<std::string>& fileNames, int& selectedIndex, bool& changedFlag, uint16_t width, uint16_t height)
      : cContainer (width, height), mFileNames(fileNames), mSelectedIndexRef(selectedIndex), mChangedFlagRef(changedFlag) {
    mChangedFlagRef = false;
    mLines = mHeight / kBoxHeight;
    }
  //}}}
  virtual ~cFileNameContainer() {}

  //{{{
  virtual void pressed (int16_t x, int16_t y) {
    mPressed = true;
    mPressedIndex = y / kBoxHeight;
    mMoved = false;
    mMove = 0;
    mScrollInc = 0;
    }
  //}}}
  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {

    mMove += yinc;
    if (abs(mMove) > 2)
      mMoved = true;

    if (mMoved)
      incScroll (yinc);
    }
  //}}}
  //{{{
  virtual void released() {
    if (!mMoved) {
      mSelectedIndexRef = (int(mScroll) / kBoxHeight) + mPressedIndex;
      mChangedFlagRef = true;
      }

    mPressed = false;
    mPressedIndex = -1;
    mMoved = false;
    mMove = 0;
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {

    if (!mPressed && mScrollInc)
      incScroll (mScrollInc / 1.1f);

    int y = -(int(mScroll) % kBoxHeight);
    int index = int(mScroll) / kBoxHeight;

    for (int i = 0; i < mLines; i++, index++,y += kBoxHeight) {
      if (y >= 0) {
        if ((i == mPressedIndex) && mPressed && !mMoved)
          lcd->rectClipped (LCD_GREY, mX, mY+y+1, mWidth-1, kBoxHeight-1);
        lcd->string (index == mSelectedIndexRef ? LCD_WHITE : LCD_LIGHTGREY, lcd->getFontHeight(),
                     mFileNames[index], mX+2, mY+y+1, mWidth-1, mHeight-1);
        }
      }
    }
  //}}}

private:
  //{{{
  void incScroll (float inc) {

    mScroll -= inc;
    if (mScroll < 0.0f)
      mScroll = 0.0f;
    else if (mScroll > (int(mFileNames.size()) - mLines) * kBoxHeight)
      mScroll = (int(mFileNames.size()) - mLines) * kBoxHeight;

    mScrollInc = inc;
    }
  //}}}

  std::vector<std::string>& mFileNames;
  int& mSelectedIndexRef;
  bool& mChangedFlagRef;

  bool mPressed = 0;
  int mPressedIndex = -1;
  bool mMoved = false;
  int mMove = 0;

  int mLines = 0;
  float mScroll = 0;
  float mScrollInc = 0.0f;
  };
