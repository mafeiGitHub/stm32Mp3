// cContainer.h
#pragma once
#include <vector>
#include "cWidget.h"

class cContainer : public cWidget {
public:
	cContainer (uint16_t width, uint16_t height) : cWidget (LCD_BLACK, width, height) {}
	virtual ~cContainer() {}

	//{{{
	void add (cWidget* widget, int16_t x, int16_t y) {

		widget->setOrg (x, y);
		mSubWidgets.push_back (widget);
		}
	//}}}
	//{{{
	bool addBelow (cWidget* widget) {

		if (!mSubWidgets.empty())
			widget->setOrg (mSubWidgets.back()->getXorg(), mSubWidgets.back()->getYorg() + mSubWidgets.back()->getHeight());

		bool fit = (widget->getYorg() + widget->getHeight() <= cLcd::getHeight()) &&
							 (widget->getXorg() + widget->getWidth() <= cLcd::getWidth());

		if (fit) 
			mSubWidgets.push_back (widget);

		return fit;
		}
	//}}}

	//{{{
	virtual cWidget* picked (int16_t x, int16_t y, int16_t z) {

		if (cWidget::picked (x, y, z)) {
			for (auto widget : mSubWidgets) {
				cWidget* pickedWidget = widget->picked (x, y, z);
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
	virtual void draw (cLcd* lcd) {

		lcd->rectClipped (mPressed ? LCD_LIGHTGREEN : mColour, mXorg, mYorg, mXlen, mYlen);
		for (auto widget : mSubWidgets)
			widget->draw (lcd);
		}
	//}}}

protected:
	std::vector<cWidget*> mSubWidgets;
	};
