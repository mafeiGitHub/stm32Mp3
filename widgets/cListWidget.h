// cFileNameContainer.h - fileName container
#pragma once
#include "cWidget.h"

class cListWidget : public cWidget {
public:
	//{{{
	cListWidget (std::vector<std::string>& fileNames, int& selectedIndex, bool& indexChangedFlag, uint16_t width, uint16_t height)
			: cWidget (LCD_BLACK, width, height),
				mFileNames(fileNames), mSelectedIndex(selectedIndex), mIndexChangedFlag(indexChangedFlag) {
		mIndexChangedFlag = false;
		mLines = mHeight / kBoxHeight;
		}
	//}}}
	virtual ~cListWidget() {}

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
			mSelectedIndex = (int(mScroll) / kBoxHeight) + mPressedIndex;
			mIndexChangedFlag = true;
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

		for (int i = 0; i < mLines; i++, index++, y += kBoxHeight)
			lcd->string (
				mPressed && !mMoved && (i == mPressedIndex) ? LCD_YELLOW : (index == mSelectedIndex) ? LCD_WHITE : LCD_LIGHTGREY,
				lcd->getFontHeight(), mFileNames[index], mX+2, mY+y+1, mWidth-1, mHeight-1);
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

		mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
		}
	//}}}

	std::vector<std::string>& mFileNames;
	int& mSelectedIndex;
	bool& mIndexChangedFlag;

	bool mPressed = 0;
	int mPressedIndex = -1;
	bool mMoved = false;
	int mMove = 0;

	int mLines = 0;
	float mScroll = 0;
	float mScrollInc = 0.0f;
	};
