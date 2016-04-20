// cTextBox.h
#pragma once
#include "cWidget.h"

class cWaveformWidget : public cWidget {
public:
  cWaveformWidget (int* playFramePtr, float* powerPtr) :
    cWidget (LCD_BLUE, cLcd::getWidth(), cLcd::getHeight()), mPlayFramePtr(playFramePtr), mPowerPtr(powerPtr) {}
  virtual ~cWaveformWidget() {}

  virtual bool picked (int16_t x, int16_t y, int16_t z) { return false; }

  virtual void draw (cLcd* lcd) {
    for (auto x = 0; x < mXlen; x++) {
      int frame = *mPlayFramePtr - mXlen + x;
      if (frame > 0) {
        auto index = (frame % 480) * 2;
        uint8_t top = (mYlen/2) - (int)mPowerPtr[index]/2;
        uint8_t ylen = (mYlen/2) + (int)mPowerPtr[index+1]/2 - top;
        lcd->rectClipped (mColour, x, top, 1, ylen);
        }
      }
    }
private:
  int* mPlayFramePtr;
  float* mPowerPtr;
  };
