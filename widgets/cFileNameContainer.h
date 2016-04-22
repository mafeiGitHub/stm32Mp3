// cFileNameContainer.h - fileName container
#pragma once
#include "cContainer.h"
#include "cSelectTextBox.h"

class cFileNameContainer : public cContainer {
public:
  //{{{
  cFileNameContainer (std::vector<std::string>& fileNames, std::string& selectedFileName, bool& selectedFileChanged,
                      uint16_t width, uint16_t height) : cContainer (width, height), mFileNames(fileNames) {

    for (unsigned int i = 0; i < getHeight()/cWidget::kBoxHeight; i++)
      addNextBelow (
        new cSelectTextBox (i < mFileNames.size() ? mFileNames[i] : mEmptyString, selectedFileName, selectedFileChanged, getWidth()));
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

    auto scroll = int (mScroll);
    for (auto widget : mSubWidgets)
      ((cSelectTextBox*)(widget))->setText (mFileNames[scroll++]);
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {
    cContainer::draw (lcd);
    }
  //}}}

private:
  std::vector<std::string>& mFileNames;
  std::string mEmptyString;
  float mScroll = 0.0f;
  };
