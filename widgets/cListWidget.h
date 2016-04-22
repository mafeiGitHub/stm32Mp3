// cFileNameContainer.h - fileName container
#pragma once
#include "cWidget.h"

class cListWidget : public cWidget {
public:
  //{{{
  cListWidget (vector<string>& names, int& index, bool& indexChanged, uint16_t width, uint16_t height)
      : cWidget (LCD_BLACK, width, height), mNames(names), mIndex(index), mIndexChanged(indexChanged) {
    mIndexChanged = false;
    mMaxLines = mHeight / kBoxHeight;
    }
  //}}}
  virtual ~cListWidget() {}

  //{{{
  virtual void pressed (int16_t x, int16_t y) {

    mPressedIndex = y / kBoxHeight;
    mTextPressed = x < mWidths[mPressedIndex];

    mMoved = false;
    mMoveInc = 0;
    mScrollInc = 0.0f;
    }
  //}}}
  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {

    mMoveInc += yinc;
    if (abs(mMoveInc) > 2)
      mMoved = true;
    if (mMoved)
      incScroll (yinc);
    }
  //}}}
  //{{{
  virtual void released() {
    if (mTextPressed && !mMoved) {
      mIndex = (int(mScroll) / kBoxHeight) + mPressedIndex;
      mIndexChanged = true;
      }

    mTextPressed = false;
    mPressedIndex = -1;
    mMoved = false;
    mMoveInc = 0;
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {

    if (!mTextPressed && mScrollInc)
      incScroll (mScrollInc * 0.9f);

    int y = -(int(mScroll) % kBoxHeight);
    int index = int(mScroll) / kBoxHeight;

    for (int i = 0; i < mMaxLines; i++, index++, y += kBoxHeight)
      mWidths[i] = lcd->string (
        mTextPressed && !mMoved && (i == mPressedIndex) ? LCD_YELLOW : (index == mIndex) ? LCD_WHITE : LCD_LIGHTGREY,
        lcd->getFontHeight(), cLcd::intStr(index) + " " + mNames[index], mX+2, mY+y+1, mWidth-1, mHeight-1);
    }
  //}}}

private:
  //{{{
  void incScroll (float inc) {

    mScroll -= inc;
    if (mScroll < 0.0f)
      mScroll = 0.0f;
    else if (mScroll > (int(mNames.size()) - mMaxLines) * kBoxHeight)
      mScroll = (int(mNames.size()) - mMaxLines) * kBoxHeight;

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  vector<string>& mNames;
  int& mIndex;
  bool& mIndexChanged;
  int mWidths[14];

  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  int mMoveInc = 0;

  int mMaxLines = 0;
  float mScroll = 0.0f;
  float mScrollInc = 0.0f;
  };
