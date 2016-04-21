// cRootWidget.h - singleton root widget manager
#pragma once
#include "cContainer.h"

class cRootWidget : public cContainer {
public:
  // static members
  static cRootWidget* get() { return mRootWidget; }

  cRootWidget (uint16_t width, uint16_t height) : cContainer (width, height) { mRootWidget = this; }
  virtual ~cRootWidget() {}

  void press (int pressCount,  int16_t x, int16_t y, uint8_t z, int16_t xinc, int16_t yinc) {
    if (!pressCount) {
      mPressedWidget = picked (x, y, z);
      if (mPressedWidget)
        mPressedWidget->pressed (x - mPressedWidget->getX(), y - mPressedWidget->getY());
      }
    else if (mPressedWidget)
      mPressedWidget->moved (x - mPressedWidget->getX(), y - mPressedWidget->getY(), z, xinc, yinc);
    }

  void release() {
    if (mPressedWidget) {
      mPressedWidget->released();
      mPressedWidget = nullptr;
      }
    }

private:
  // static vars
  static cRootWidget* mRootWidget;

  // member vars
  cWidget* mPressedWidget = nullptr;
  };

// static member var
cRootWidget* cRootWidget::mRootWidget = nullptr;
