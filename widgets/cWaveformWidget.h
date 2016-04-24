// cWaveformWidget.h
#pragma once
#include "cWidget.h"

class cWaveformWidget : public cWidget {
public:
  //{{{
  cWaveformWidget (int& frame, float* waveform, uint16_t width, uint16_t height) :
      cWidget (LCD_BLUE, width, height), mFrameRef(frame), mWaveform(waveform) {
    memset (mSrc, 0xC0, 272);
    }
  //}}}
  virtual ~cWaveformWidget() {}

  virtual cWidget* picked (int16_t x, int16_t y, uint8_t z) {

    int frame = mFrameRef - mWidth + x;
    if (frame > 0) {
      auto index = (frame % 480) * 2;
      uint8_t top = (mHeight/2) - (int)mWaveform[index]/2;
      uint8_t ylen = (mHeight/2) + (int)mWaveform[index+1]/2 - top;
      if (y >= top && y <= top + ylen)
        return this;
      }

    return nullptr;
    }

  virtual void draw (iDraw* draw) {
    for (auto x = 0; x < mWidth; x++) {
      int frame = mFrameRef - mWidth + x;
      if (frame > 0) {
        auto index = (frame % 480) * 2;
        uint8_t top = (mHeight/2) - (int)mWaveform[index]/2;
        uint8_t ylen = (mHeight/2) + (int)mWaveform[index+1]/2 - top;
        draw->stampClipped (mOn ? LCD_LIGHTRED : mColour, mSrc, x, top, 1, ylen);
        }
      }
    }
private:
  int& mFrameRef;
  float* mWaveform;
  uint8_t mSrc[272];
  };
