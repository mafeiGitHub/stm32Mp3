// cTextBox.h
#pragma once
#include <string>
#include "cWidget.h"

class cTextBox : public cWidget {
public:
  cTextBox (std::string& text, uint16_t width) : cWidget (width), mText(text) {}
  cTextBox (std::string& text, uint32_t colour, uint16_t width, uint16_t height) : cWidget (colour, width, height), mText(text) {}
  virtual ~cTextBox() {}

  void setText (string& text) { mTextRef = text; }
  void setTextColour (uint32_t colour) { mTextColour = colour; }

  virtual void draw (cLcd* lcd) {
    cWidget::draw (lcd);
    lcd->text (mTextColour, lcd->getFontHeight(), mText, mX+2, mY+1, mWidth-1, mHeight-1);
    }

protected:
  std::string& mText;
  uint32_t mTextColour = LCD_DARKGREY;
  };
