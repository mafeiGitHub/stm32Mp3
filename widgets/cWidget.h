// cWidget.h - base widget, draws as box with width-1, height-1
#pragma once
class cContainer;
#include "iDraw.h"

class cWidget {
public:
	static uint16_t getBoxHeight() { return kBoxHeight; }
	static uint16_t getFontHeight() { return kFontHeight; }

	cWidget (uint16_t width) : mWidth(width) {}
	cWidget (float width) : mWidth(int(width* kBoxHeight)) {}
	cWidget (uint32_t colour, uint16_t height) : mColour(colour), mWidth(height) {}
	cWidget (uint32_t colour, uint16_t width, uint16_t height) : mColour(colour), mWidth(width), mHeight(height) {}
	virtual ~cWidget() {}

	int16_t getX() { return mX; }
	int16_t getY() { return mY; }
	uint16_t getWidth() { return mWidth; }
	uint16_t getHeight() { return mHeight; }
	int getPressedCount() { return mPressedCount; }
	bool isPressed() { return mPressedCount > 0; }
	bool isOn() { return mOn; }
	bool isVisible() { return mVisible; }

	void setXY (int16_t x, int16_t y) { mX = x; mY = y; }
	void setColour (uint32_t colour) { mColour = colour; }
	void setParent (cContainer* parent) { mParent = parent; }
	void setVisible (bool visible) { mVisible = visible; }

	//{{{
	virtual void pressed (int16_t x, int16_t y) {
		if (!mPressedCount)
			mOn = true;
		mPressedCount++;
		}
	//}}}
	virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {}
	//{{{
	virtual void released() {
		mPressedCount = 0;
		mOn = false;
		}
	//}}}

	//{{{
	virtual cWidget* picked (int16_t x, int16_t y, uint8_t z) {

		return (x >= mX) && (x < mX + mWidth) && (y >= mY) && (y < mY + mHeight) ? this : nullptr;
		}
	//}}}
	//{{{
	virtual void draw (iDraw* draw) {
		draw->rectClipped (mOn ? LCD_LIGHTRED : mColour, mX+1, mY+1, mWidth-1, mHeight-1);
		}
	//}}}

protected:
	uint32_t mColour = LCD_LIGHTGREY;

	int16_t mX = 0;
	int16_t mY = 0;
	uint16_t mWidth = 100;
	uint16_t mHeight = kBoxHeight;

	int mPressedCount = 0;
	bool mOn = false;

	cContainer* mParent;
	bool mVisible = true;

private:
	static const uint16_t kFontHeight = 16;
	static const uint16_t kBoxHeight = 19;

	};
