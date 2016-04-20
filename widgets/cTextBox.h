// cTextBox.h
#pragma once
#include <string>
#include "cWidget.h"
#include "cLcd.h"

class cTextBox : public cWidget {
public:
  cTextBox (std::string text, uint16_t xlen) : cWidget (xlen), mText(text) {}
  cTextBox (std::string text, uint32_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) :
    cWidget (colour, xorg, yorg, xlen, ylen), mText(text) {}
  virtual ~cTextBox() {}

  void setText (std::string text) {
    mText = text;
    }

  virtual void draw (cLcd* lcd) {
    cWidget::draw (lcd);
    lcd->string (LCD_DARKGREY, cLcd::instance()->getFontHeight(), mText, mXorg+2, mYorg+1, mXlen-2, mYlen-2);
    }

protected:
  std::string mText;
  };
