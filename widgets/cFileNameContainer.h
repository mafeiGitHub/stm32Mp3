// cFileNameContainer.h - fileName container
#pragma once
#include "cContainer.h"
#include "cSelectTextBox.h"

class cFileNameContainer : public cContainer {
public:
  //{{{
  cFileNameContainer (std::vector<std::string>& fileNames, int& selectedId, bool& changedFlag,
                      uint16_t width, uint16_t height) : cContainer (width, height), mFileNames(fileNames) {

    for (auto i = 0; i < getHeight()/cWidget::kBoxHeight; i++)
      addNextBelow (
        new cSelectTextBox (i < (int)mFileNames.size() ? mFileNames[i] : mEmptyString, i, selectedId, changedFlag, getWidth()));
    }
  //}}}
  virtual ~cFileNameContainer() {}

  //{{{
  virtual void moved (int16_t x, int16_t y, int16_t z, int16_t xinc, int16_t yinc) {

    mScroll -= yinc * 2.0f / cWidget::kBoxHeight;

    if (mScroll < 0.0f)
      mScroll = 0.0f;
    else if (mScroll > mFileNames.size() - mSubWidgets.size())
      mScroll = mFileNames.size() - mSubWidgets.size();

    mFileNameFirstIndex = int (mScroll);
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {

    unsigned int fileNameIndex = mFileNameFirstIndex;
    unsigned int maxWidgetIndex = mFileNames.size() - mFileNameFirstIndex < mSubWidgets.size() ?
                                    mFileNames.size() - mFileNameFirstIndex : mSubWidgets.size();
    for (unsigned int widgetIndex = 0; widgetIndex < maxWidgetIndex; widgetIndex++, fileNameIndex++)
      ((cSelectTextBox*)(mSubWidgets[widgetIndex]))->setText (mFileNames[fileNameIndex], fileNameIndex);

    cContainer::draw (lcd);
    }
  //}}}

private:
  std::vector<std::string>& mFileNames;
  std::string mEmptyString;
  float mScroll = 0.0f;
  float mFileNameFirstIndex = 0;
  };
