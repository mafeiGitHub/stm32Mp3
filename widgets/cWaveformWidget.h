// cTextBox.h
#pragma once
#include "cWidget.h"

class cWaveformWidget : public cWidget {
public:
  cWaveformWidget (int& frame, float* waveform) :
    cWidget (LCD_BLUE, cLcd::getWidth(), cLcd::getHeight()), mFrame(frame), mWaveform(waveform) {}
  virtual ~cWaveformWidget() {}

  virtual cWidget* picked (int16_t x, int16_t y, int16_t z) { return nullptr; }

  virtual void draw (cLcd* lcd) {
    for (auto x = 0; x < mXlen; x++) {
      int frame = mFrame - mXlen + x;
      if (frame > 0) {
        auto index = (frame % 480) * 2;
        uint8_t top = (mYlen/2) - (int)mWaveform[index]/2;
        uint8_t ylen = (mYlen/2) + (int)mWaveform[index+1]/2 - top;
        lcd->rectClipped (mColour, x, top, 1, ylen);
        }
      }
    }
private:
  int& mFrame;
  float* mWaveform;
  };
