// cFileNameContainer.h - fileName container
#pragma once
#include "cContainer.h"
#include "cSelectTextBox.h"


class cFileNameContainer : public cContainer {
public:
  cFileNameContainer (std::vector<std::string>& fileNames, std::string& selectedFileName, bool& selectedFileChanged,
                      uint16_t width, uint16_t height)
    : cContainer (width, height), mFileNames(fileNames) { createTextBoxes (selectedFileName, selectedFileChanged); }
  virtual ~cFileNameContainer() {}

private:
  void createTextBoxes (std::string& selectedFileName, bool& selectedFileChanged) {
    for (unsigned int i = 0; i < 14 && i < mFileNames.size(); i++)
      addNextBelow (new cSelectTextBox (mFileNames[i], selectedFileName, selectedFileChanged, getWidth() - cWidget::kBoxHeight));
    }

  std::vector<std::string>& mFileNames;
  };
