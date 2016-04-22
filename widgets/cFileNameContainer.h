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
    mMoved = false;
    mMove = 0;
    mPressed = true;
    mPressedIndex = y / kBoxHeight;
    }
  //}}}
  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {
    mMove += yinc;
    if (abs(mMove) > 2)
      mMoved = true;

    if (mMoved) {
      mScroll -= yinc;
      if (mScroll < 0)
        mScroll = 0;
      else if (mScroll > (int(mFileNames.size()) - mLines) * kBoxHeight)
       mScroll = (int(mFileNames.size()) - mLines) * kBoxHeight;

      mFileNameFirstIndex = mScroll / kBoxHeight;
      }
    }
  //}}}
  //{{{
  virtual void released() {
    if (!mMoved) {
      mSelectedIndexRef = mFileNameFirstIndex + mPressedIndex;
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
    int y = -(mScroll % kBoxHeight);
    int index = mFileNameFirstIndex;
    for (int i = 0; i < mLines; i++, index++) {
      if (y >= 0) {
        if ((i == mPressedIndex) && mPressed && !mMoved)
          lcd->rectClipped (LCD_GREY, mX, mY+y+1, mWidth-1, kBoxHeight-1);
        lcd->string (index == mSelectedIndexRef ? LCD_WHITE : LCD_LIGHTGREY, lcd->getFontHeight(),
                     mFileNames[index], mX+2, mY+y+1, mWidth-1, mHeight-1);
        }
      y += kBoxHeight;
      }
    }
  //}}}

private:
  std::vector<std::string>& mFileNames;
  int& mSelectedIndexRef;
  bool& mChangedFlagRef;

  int mScroll = 0;
  int mFileNameFirstIndex = 0;

  bool mPressed = 0;
  int mPressedIndex = -1;
  bool mMoved = false;
  int mMove = 0;
  int mLines = 0;
  };
