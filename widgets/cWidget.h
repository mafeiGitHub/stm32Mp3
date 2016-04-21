// cWidget.h - base widget, draws as box with width-1, height-1
#pragma once
class cContainer;
#include "cLcd.h"

class cWidget {
public:
	static const uint16_t kBoxHeight = 19;

	cWidget (uint16_t width) : mWidth(width) {}
	cWidget (float width) : mWidth(int(width* kBoxHeight)) {}
	cWidget (uint32_t colour, uint16_t height) : mColour(colour), mWidth(height) {}
	cWidget (uint32_t colour, uint16_t width, uint16_t height) : mColour(colour), mWidth(width), mHeight(height) {}
	virtual ~cWidget() {}

	int16_t getX() { return mX; }
	int16_t getY() { return mY; }
	uint16_t getWidth() { return mWidth; }
	uint16_t getHeight() { return mHeight; }
	int getPressed() { return mPressed; }

	void setXY (int16_t x, int16_t y) { mX = x; mY = y; }
	void setColour (uint32_t colour) { mColour = colour; }
	void setParent (cContainer* parent) { mParent = parent; }

	virtual void pressed (int16_t x, int16_t y) { mPressed++; }
	virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {}
	virtual void released() { mPressed = 0; }

	//{{{
	virtual cWidget* picked (int16_t x, int16_t y, uint8_t z) {

		return (x >= mX) && (x < mX + mWidth) && (y >= mY) && (y < mY + mHeight) ? this : nullptr;
		}
	//}}}
	//{{{
	virtual void draw (cLcd* lcd) {

		lcd->rectClipped (mPressed ? LCD_LIGHTRED : mColour, mX+1, mY+1, mWidth-1, mHeight-1);
		}
	//}}}

protected:
	uint32_t mColour = LCD_LIGHTGREY;

	int16_t mX = 0;
	int16_t mY = 0;
	uint16_t mWidth = 100;
	uint16_t mHeight = kBoxHeight;

	int mPressed = 0;

	cContainer* mParent;
	};
