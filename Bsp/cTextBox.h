// cTextBox.h
#pragma once
//{{{  includes
#include <string>
#include "cWidget.h"
//}}}

class cTextBox : public cWidget {
public:
  cTextBox (std::string text, uint32_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) :
    cWidget (colour, xorg, yorg, xlen, ylen), mText(text) {}
  ~cTextBox() {}

  void setText (std::string text) {
    mText = text;
    }

  virtual void draw (cLcd* lcd) {
    cWidget::draw (lcd);
    lcd->string (LCD_BLACK, lcd->getFontHeight(), mText, mXorg, mYorg, mXlen, mYlen);
    }

private:
  std::string mText;
  };
