// cWidgetMan.h - singletom widget manager
#pragma once
#include <vector>
#include "cWidget.h"

class cWidgetMan {
public:
  // static members
  //{{{
  static cWidgetMan* create() {
    if (!mWidgetMan)
      mWidgetMan = new cWidgetMan();
    return mWidgetMan;
    }
  //}}}
  static cWidgetMan* instance() { return mWidgetMan; }

  cWidgetMan() {}
  ~cWidgetMan() {}

  //{{{
  void add (cWidget* widget, int16_t x, int16_t y) {

    widget->setOrg (x, y);
    mWidgets.push_back (widget);
    }
  //}}}
  //{{{
  bool addBelow (cWidget* widget) {

    if (!mWidgets.empty())
      widget->setOrg (mWidgets.back()->getXorg(), mWidgets.back()->getYorg() + mWidgets.back()->getYlen());

    bool fit = (widget->getYorg() + widget->getYlen() <= cLcd::getHeight()) &&
               (widget->getXorg() + widget->getXlen() <= cLcd::getWidth());

    if (fit)
      mWidgets.push_back (widget);

    return fit;
    }
  //}}}

  //{{{
  bool press (int pressCount, int x, int y, int z, int xinc, int yinc) {

    if (!pressCount) {
      for (auto widget : mWidgets) {
        if (widget->picked (x, y, z)) {
          mPressedx = x;
          mPressedy = y;
          mPressedWidget = widget;
          mPressedWidget->pressed (x - widget->getXorg(), y - widget->getYorg());
          return true;
          }
        }
      mPressedWidget = nullptr;
      }

    else if (mPressedWidget) {
      mPressedx = x;
      mPressedy = y;
      mPressedWidget->moved (x - mPressedWidget->getXorg(), y - mPressedWidget->getYorg(), z, xinc, yinc);
      }

    return mPressedWidget;
    }
  //}}}
  //{{{
  void release() {

    if (mPressedWidget) {
      mPressedWidget->released (mPressedx, mPressedy);
      mPressedWidget = nullptr;
      }
    }
  //}}}

  //{{{
  void draw (cLcd* lcd) {

    for (auto widget : mWidgets)
      (widget->draw (lcd));
    }
  //}}}

private:
  // static vars
  static cWidgetMan* mWidgetMan;

  // member vars
  cWidget* mPressedWidget = nullptr;
  std::vector <cWidget*> mWidgets;

  uint16_t mPressedx = 0;
  uint16_t mPressedy = 0;
  };

// static member var
cWidgetMan* cWidgetMan::mWidgetMan = nullptr;
