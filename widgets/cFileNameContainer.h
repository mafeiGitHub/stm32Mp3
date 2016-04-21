// cFileNameContainer.h - fileName container
#pragma once
#include "cContainer.h"
#include "cSelectTextBox.h"

class cFileNameContainer : public cContainer {
public:
  //{{{
  cFileNameContainer (std::vector<std::string>& fileNames, std::string& selectedFileName, bool& selectedFileChanged,
                      uint16_t width, uint16_t height) : cContainer (width, height), mFileNames(fileNames),
					  mSelectedFileNameRef (selectedFileName), mSelectedFileChangedRef(selectedFileChanged) {
    createSelectTextBoxes (selectedFileName, selectedFileChanged);
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

    scrollSelectTextBoxes (int(mScroll));
    }
  //}}}
  //{{{
  virtual void draw (cLcd* lcd) {
    if ((mSubWidgets.size() < mFileNames.size()) && (mSubWidgets.size() < (getHeight()/cWidget::kBoxHeight)))
      createSelectTextBoxes (mSelectedFileNameRef, mSelectedFileChangedRef);
    cContainer::draw (lcd);
    }
  //}}}

private:
  //{{{
  void createSelectTextBoxes (std::string& selectedFileName, bool& selectedFileChanged) {
    for (auto i = mSubWidgets.size(); i < (getHeight()/cWidget::kBoxHeight) && (i < mFileNames.size()); i++)
      addNextBelow (new cSelectTextBox (mFileNames[i], selectedFileName, selectedFileChanged, getWidth()));
    }
  //}}}
  //{{{
  void scrollSelectTextBoxes (int scroll) {
    for (auto widget : mSubWidgets)
      ((cSelectTextBox*)(widget))->setText (mFileNames[scroll++]);
    }
  //}}}

  std::vector<std::string>& mFileNames;
  float mScroll = 0.0f;

  std::string& mSelectedFileNameRef;
  bool& mSelectedFileChangedRef;
  };
