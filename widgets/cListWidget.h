// cFileNameContainer.h - fileName container
#pragma once
#include "cWidget.h"

class cListWidget : public cWidget {
public:
  //{{{
  cListWidget (std::vector<std::string>& names, int& index, bool& indexChanged, uint16_t width, uint16_t height)
      : cWidget (LCD_BLACK, width, height), mNames(names), mIndex(index), mIndexChanged(indexChanged) {
    mIndexChanged = false;
    mMaxLines = 1 + (mHeight / getBoxHeight());
    }
  //}}}
  virtual ~cListWidget() {}

  //{{{
  virtual void pressed (int16_t x, int16_t y) {

    mPressedIndex = ((int)mScroll + y) / getBoxHeight();
    mTextPressed = x < mMeasure[y / getBoxHeight()];

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
      mIndex = mPressedIndex;
      mIndexChanged = true;
      }

    mTextPressed = false;
    mPressedIndex = -1;
    mMoved = false;
    mMoveInc = 0;
    }
  //}}}
  //{{{
  virtual void draw (iDraw* draw) {

    if (!mTextPressed && mScrollInc)
      incScroll (mScrollInc * 0.9f);

    int y = -(int(mScroll) % getBoxHeight());
    int index = int(mScroll) / getBoxHeight();
    for (int i = 0; i < mMaxLines; i++, index++, y += getBoxHeight())
      mMeasure[i] = draw->text (
        mTextPressed && !mMoved && (index == mPressedIndex) ? LCD_YELLOW : (index == mIndex) ? LCD_WHITE : LCD_LIGHTGREY,
        getFontHeight(), mNames[index], mX+2, mY+y+1, mWidth-1, mHeight-1);
    }
  //}}}

private:
  //{{{
  void incScroll (float inc) {

    mScroll -= inc;
    if (mScroll < 0.0f)
      mScroll = 0.0f;
    else if (mScroll > (mNames.size() * getBoxHeight()) - mHeight)
      mScroll = float(((int)mNames.size() * getBoxHeight()) - mHeight);

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  std::vector<std::string>& mNames;
  int& mIndex;
  bool& mIndexChanged;

  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  int mMoveInc = 0;

  int mMaxLines = 0;
  int mMeasure[14];

  float mScroll = 0.0f;
  float mScrollInc = 0.0f;
  };
