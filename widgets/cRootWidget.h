// cRootWidget.h - singleton root widget
#pragma once
#include "cContainer.h"

class cRootWidget : public cContainer {
public:
  // static
  static cRootWidget* get() { return mRootWidget; }

  // member
  cRootWidget (uint16_t width, uint16_t height) : cContainer (width, height) { mRootWidget = this; }
  virtual ~cRootWidget() {}

  //{{{
  void press (int pressCount,  int16_t x, int16_t y, uint8_t z, int16_t xinc, int16_t yinc) {
    if (!pressCount) {
      mPressedWidget = picked (x, y, z);
      if (mPressedWidget)
        mPressedWidget->pressed (x - mPressedWidget->getX(), y - mPressedWidget->getY());
      }
    else if (mPressedWidget)
      mPressedWidget->moved (x - mPressedWidget->getX(), y - mPressedWidget->getY(), z, xinc, yinc);
    }
  //}}}
  //{{{
  void release() {
    if (mPressedWidget) {
      mPressedWidget->released();
      mPressedWidget = nullptr;
      }
    }
  //}}}

  //{{{
  virtual void draw (cLcd* lcd) {
    lcd->rect (mPressed ? LCD_DARKGREEN : mColour, mX, mY, mWidth, mHeight);
    cContainer::draw (lcd);
    }
  //}}}

private:
  // static var
  static cRootWidget* mRootWidget;

  // member var
  cWidget* mPressedWidget = nullptr;
  };

// static member var init
cRootWidget* cRootWidget::mRootWidget = nullptr;
