// cRadioChan.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
//}}}

class cRadioChan {
public:
  cRadioChan() : mChan(0), mBaseSeqNum(0) {}
  ~cRadioChan() {}

  // gets
  //{{{
  int getChan() {
    return mChan;
    }
  //}}}
  //{{{
  int getSampleRate() {
    return kSampleRate;
    }
  //}}}
  //{{{
  int getSamplesPerFrame() {
    return kSamplesPerFrame;
    }
  //}}}
  //{{{
  int getFramesPerChunk() {
    return kFramesPerChunk[getRadioTv()];
    }
  //}}}
  //{{{
  float getFramesPerSecond() {
    return (float)kSampleRate / (float)kSamplesPerFrame;
    }
  //}}}
  //{{{
  int getFramesFromSec (int sec) {
    return int (sec * getFramesPerSecond());
    }
  //}}}

  //{{{
  int getLowBitrate() {
    return kLowBitrate [mChan];
    }
  //}}}
  //{{{
  int getMidBitrate() {
    return kMidBitrate [mChan];
    }
  //}}}
  //{{{
  int getHighBitrate() {
    return kHighBitrate [mChan];
    }
  //}}}

  //{{{
  int getRadioTv() {
    return kRadioTv[mChan];
    }
  //}}}
  //{{{
  int getBaseSeqNum() {
    return mBaseSeqNum;
    }
  //}}}

  //{{{
  std::string getHost() {
    return mHost;
    }
  //}}}
  //{{{
  std::string getTsPath (int seqNum, int bitrate) {

    return getPathRoot (bitrate) + '-' + toString (seqNum) + ".ts";
    }
  //}}}
  //{{{
  std::string getChanName (int chan) {
    return kChanNames [chan];
    }
  //}}}
  //{{{
  std::string getDateTime() {
    return mDateTime;
    }
  //}}}
  //{{{
  std::string getChanInfoStr() {
    return mChanInfoStr;
    }
  //}}}

  // set
  //{{{
  void setChan (cHttp* http, int chan) {

    mChan = chan;
    mHost = getRadioTv() ? "vs-hls-uk-live.bbcfmt.vo.llnwd.net" : "as-hls-uk-live.bbcfmt.vo.llnwd.net";

    if (http->get (mHost, getM3u8path()) == 302) {
      mHost = http->getRedirectedHost();
      http->get (mHost, getM3u8path());
      }
    else
      mHost = http->getRedirectedHost();

    //printf ("%s\n", (char*)http->getContent());

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
    }
  //}}}

private:
  //{{{  const
  const int kSampleRate = 48000;
  const int kSamplesPerFrame = 1024;
  const int kMaxFramesPerChunk = 375;
  const int kFramesPerChunk[2] = { 300, 375 }; // 6.4s, 8s

  const bool kRadioTv    [9] = { false,   false,  false,  false,  false,  false,  false,   true,   true };
  const int kPool        [9] = {      0,      7,      7,      7,      6,      6,      6,      4,      5 };
  const int kLowBitrate  [9] = {  48000,  48000,  48000,  48000,  48000,  48000,  48000,  96000,  96000 };
  const int kMidBitrate  [9] = { 128000, 128000, 128000, 128000, 128000, 128000, 128000,  96000,  96000 };
  const int kHighBitrate [9] = { 320000, 320000, 320000, 320000, 320000, 320000, 320000,  96000,  96000 };
  const char* kChanNames [9] = { "none", "radio1", "radio2", "radio3", "radio4", "radio5", "radio6", "bbc1", "bbc2" };
  const char* kPathNames [9] = { "none", "bbc_radio_one",    "bbc_radio_two", "bbc_radio_three", "bbc_radio_fourfm",
                                         "bbc_radio_five_live", "bbc_6music",      "bbc_one_hd",       "bbc_two_hd" };
  //}}}
  //{{{
  std::string getPathRoot (int bitrate) {

    std::string path = "pool_" + toString (kPool[mChan]) + "/live/" + kPathNames[mChan] + '/' + kPathNames[mChan] + ".isml/" + kPathNames[mChan];

    if (!getRadioTv()) // radio
      return path + "-audio=" + toString (bitrate);
    else if (bitrate == 128000)
      return path + "-pa4=128000";
    else if (bitrate == 320000)
      return path + "-pa5=320000";
    else if (bitrate == 64000)
      return path + "-pa2=64000";
    else if (bitrate == 24000)
      return path + "-pa1=24000";
    else // default
      return path + "-pa3=96000";
    }
  //}}}
  //{{{
  std::string getM3u8path () {
    return getPathRoot (getMidBitrate()) + ".norewind.m3u8";
    }
  //}}}

  // vars
  int mChan;
  int mBaseSeqNum;
  std::string mHost;
  std::string mDateTime;
  std::string mChanInfoStr;
  };
