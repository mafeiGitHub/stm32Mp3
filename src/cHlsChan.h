// cRadioChan.h
#pragma once
#include "cHttp.h"
#include "cLcd.h"

class cHlsChan {
public:
  cHlsChan() {}
  virtual ~cHlsChan() {}

  // gets
  int getChan() { return mChan; }
  int getSampleRate() { return kSampleRate; }
  int getSamplesPerFrame() { return kSamplesPerFrame; }
  int getFramesPerChunk() { return kFramesPerChunk; }
  int getFramesFromSec (int sec) { return int (sec * getFramesPerSecond()); }
  float getFramesPerSecond() { return (float)kSampleRate / (float)kSamplesPerFrame; }

  int getLowBitrate() { return 48000; }
  int getMidBitrate() { return 128000; }
  int getHighBitrate() { return 320000; }

  int getBaseSeqNum() { return mBaseSeqNum; }

  std::string getHost() { return mHost; }
  std::string getTsPath (int seqNum, int bitrate) { return getPathRoot (bitrate) + '-' + toString (seqNum) + ".ts"; }
  std::string getChanName (int chan) { return kChanNames [chan]; }
  std::string getDateTime() { return mDateTime; }
  std::string getChanInfoStr() { return mChanInfoStr; }

  // set
  //{{{
  void setChan (cHttp* http, int chan) {

    mChan = chan;
    mHost = "as-hls-uk-live.bbcfmt.vo.llnwd.net";

    cLcd::debug (mHost);
    if (http->get (mHost, getM3u8path()) == 302) {
      mHost = http->getRedirectedHost();
      http->get (mHost, getM3u8path());
      cLcd::debug (mHost);
      }
    else
      mHost = http->getRedirectedHost();
    cLcd::debug (getM3u8path());

    // find #EXT-X-MEDIA-SEQUENCE in .m3u8, point to seqNum string, extract seqNum from playListBuf
    auto extSeq = strstr ((char*)http->getContent(), "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
    auto extSeqEnd = strchr (extSeq, '\n');
    *extSeqEnd = '\0';
    mBaseSeqNum = atoi (extSeq) + 3;

    auto extDateTime = strstr (extSeqEnd + 1, "#EXT-X-PROGRAM-DATE-TIME:") + strlen ("#EXT-X-PROGRAM-DATE-TIME:");
    auto extDateTimeEnd = strchr (extDateTime, '\n');
    *extDateTimeEnd = '\0';
    mDateTime = extDateTime;

    mChanInfoStr = toString (http->getResponse()) + ' ' + http->getInfoStr() + ' ' + getM3u8path() + ' ' + mDateTime;
    cLcd::debug (mDateTime + " " + cLcd::dec (mBaseSeqNum));
    }
  //}}}

private:
  //{{{  const
  const int kSampleRate = 48000;
  const int kSamplesPerFrame = 1024;
  const int kFramesPerChunk = 300;

  const int kPool        [7] = {      0,      7,      7,      7,      6,      6,      6 };
  const int kLowBitrate  [7] = {  48000,  48000,  48000,  48000,  48000,  48000,  48000 };
  const int kMidBitrate  [7] = { 128000, 128000, 128000, 128000, 128000, 128000, 128000 };
  const int kHighBitrate [7] = { 320000, 320000, 320000, 320000, 320000, 320000, 320000 };
  const char* kChanNames [7] = { "none", "radio1", "radio2", "radio3", "radio4", "radio5", "radio6" };
  const char* kPathNames [7] = { "none", "bbc_radio_one",    "bbc_radio_two",       "bbc_radio_three",
                                         "bbc_radio_fourfm", "bbc_radio_five_live", "bbc_6music" };
  //}}}
  //{{{
  std::string getPathRoot (int bitrate) {
    return "pool_" + toString (kPool[mChan]) + "/live/" + kPathNames[mChan] + '/' + kPathNames[mChan] +
           ".isml/" + kPathNames[mChan] + "-audio=" + toString (bitrate);
    }
  //}}}
  std::string getM3u8path () { return getPathRoot (getMidBitrate()) + ".norewind.m3u8"; }

  // vars
  int mChan = 0;
  int mBaseSeqNum = 0;
  std::string mHost;
  std::string mDateTime;
  std::string mChanInfoStr;
  };
