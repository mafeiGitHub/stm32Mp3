// cWidgetMan.h - singleton widget manager
#pragma once
class cLcd;
#include "cContainer.h"

class cWidgetMan {
public:
  // static members
  //{{{
  static cWidgetMan* create (uint16_t width, uint16_t height) {
    if (!mWidgetMan)
      mWidgetMan = new cWidgetMan (width, height);
    return mWidgetMan;
    }
  //}}}
  static cWidgetMan* instance() { return mWidgetMan; }

  //{{{
  cWidgetMan (uint16_t width, uint16_t height) {
    mRootContainer = new cContainer (width, height);
    }
  //}}}
  ~cWidgetMan() {}

  //{{{
  void press (int pressCount, int x, int y, int z, int xinc, int yinc) {

    if (!pressCount) {
      mPressedWidget = mRootContainer->picked (x, y, z);
      if (mPressedWidget)
        mPressedWidget->pressed (x - mPressedWidget->getXorg(), y - mPressedWidget->getYorg());
      }
    else if (mPressedWidget)
      mPressedWidget->moved (x - mPressedWidget->getXorg(), y - mPressedWidget->getYorg(), z, xinc, yinc);
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

  void add (cWidget* widget, int16_t x, int16_t y) { mRootContainer->add (widget, x, y); }
  bool addBelow (cWidget* widget) { return mRootContainer->addBelow (widget); }

  void draw (cLcd* lcd) { mRootContainer->draw (lcd); }

private:
  // static vars
  static cWidgetMan* mWidgetMan;

  // member vars
  cWidget* mPressedWidget = nullptr;
  cContainer* mRootContainer;
  };

// static member var
cWidgetMan* cWidgetMan::mWidgetMan = nullptr;
