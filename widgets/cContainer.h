// cContainer.h - widget container, no background draw
#pragma once
#include <vector>
#include "cWidget.h"

class cContainer : public cWidget {
public:
  cContainer (uint16_t width, uint16_t height) : cWidget (LCD_BLACK, width, height) {}
  virtual ~cContainer() {}

  //{{{
  void add (cWidget* widget, int16_t x, int16_t y) {
    widget->setParent (this);
    widget->setXY (x, y);
    mSubWidgets.push_back (widget);
    }
  //}}}
  //{{{
  void addTopLeft (cWidget* widget) {
    widget->setXY (0, 0);
    mSubWidgets.push_back (widget);
    }
  //}}}
  //{{{
  void addTopRight (cWidget* widget) {
    widget->setXY (mWidth - widget->getWidth(), 0);
    mSubWidgets.push_back (widget);
    }
  //}}}
  //{{{
  void addBottomLeft (cWidget* widget) {
    widget->setXY (0, mHeight - widget->getHeight());
    mSubWidgets.push_back (widget);
    }
  //}}}
  //{{{
  void addBottomRight (cWidget* widget) {
    widget->setXY (mWidth - widget->getWidth(),  mHeight - widget->getHeight());
    mSubWidgets.push_back (widget);
    }
  //}}}
  //{{{
  void addNextBelow (cWidget* widget) {
    add (widget, mSubWidgets.empty() ? 0 : mSubWidgets.back()->getX(),
                 mSubWidgets.empty() ? 0 : mSubWidgets.back()->getY() + mSubWidgets.back()->getHeight());
    }
  //}}}
  //{{{
  void addNextRight (cWidget* widget) {
    add (widget, mSubWidgets.empty() ? 0 : mSubWidgets.back()->getX() + mSubWidgets.back()->getWidth(),
                 mSubWidgets.empty() ? 0 : mSubWidgets.back()->getY());
    }
  //}}}

  //{{{
  virtual cWidget* picked (int16_t x, int16_t y, uint8_t z) {

    if (cWidget::picked (x, y, z)) {
      int i = (int)mSubWidgets.size();
      while (--i >= 0) {
        cWidget* pickedWidget = mSubWidgets[i]->picked (x, y, z);
        if (pickedWidget)
          return pickedWidget;
        }
      return this;
      }
    else
      return nullptr;
    }
  //}}}
  //{{{
  virtual void draw (iDraw* draw) {
    for (auto widget : mSubWidgets)
      if (widget->isVisible())
        widget->draw (draw);
    }
  //}}}

protected:
  std::vector<cWidget*> mSubWidgets;
  };
