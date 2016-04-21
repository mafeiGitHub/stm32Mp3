// cTextBox.h
#pragma once
#include <string>
#include "cWidget.h"

class cTextBox : public cWidget {
public:
  cTextBox (std::string text, uint16_t xlen) : cWidget (xlen), mText(text) {}
  cTextBox (std::string text, uint32_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) :
    cWidget (colour, xlen, ylen), mText(text) {}
  virtual ~cTextBox() {}

  void setText (std::string text) {
    mText = text;
    }

  virtual void draw (cLcd* lcd) {
    cWidget::draw (lcd);
    lcd->string (LCD_DARKGREY, lcd->getFontHeight(), mText, mX+2, mY+1, mWidth-1, mHeight-1);
    }

protected:
  std::string mText;
  };
