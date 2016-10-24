// cHlsLoader.h
#pragma once
#include "cHlsChunk.h"

class cHlsLoader : public cHlsChan {
public:
  cHlsLoader() {}
  virtual ~cHlsLoader() {}

  //{{{
  int getBitrate() {
    return mBitrate;
    }
  //}}}
  //{{{
  int getLoading() {
    return mLoading;
    }
  //}}}
  //{{{
  std::string getInfoStr (int frame) {
    return getChanName (getChan()) + ':' + toString (getBitrate()/1000) + "k " + getFrameInfo (frame);
           //+ getChunkNumStr (0) + ':' + getChunkNumStr (1) + ':' + getChunkNumStr (2);
    }
  //}}}
  //{{{
  std::string getFrameInfo (int frame) {

    int secsSinceMidnight = int (frame / getFramesPerSecond());
    int secs = secsSinceMidnight % 60;
    int mins = (secsSinceMidnight / 60) % 60;
    int hours = secsSinceMidnight / (60*60);

    return toString (hours) + ':' + toString (mins) + ':' + toString (secs);
    }
  //}}}
  //{{{
  std::string getChunkInfoStr (int chunk) {
    return getChunkNumStr (chunk) + ':' + mChunks[chunk].getInfoStr();
    }
  //}}}
  //{{{
  uint8_t* getPower (int frame, int& frames) {
  // return pointer to frame power org,len uint8_t pairs
  // frames = number of valid frames

    int seqNum;
    int chunk = 0;
    int frameInChunk;
    frames = 0;
    return findFrame (frame, seqNum, chunk, frameInChunk) ? mChunks[chunk].getPower (frameInChunk, frames) : nullptr;
    }
  //}}}
  //{{{
  int16_t* getSamples (int frame, int& seqNum) {
  // return audio buffer for frame

    int chunk;
    int frameInChunk;
    return findFrame (frame, seqNum, chunk, frameInChunk) ? mChunks[chunk].getSamples (frameInChunk) : nullptr;
    }
  //}}}

  //{{{
  int changeChan (cHttp* http, int chan) {

    printf ("cHlsChunks::setChan %d\n", chan);
    setChan (http, chan);
    mBitrate = getMidBitrate();

    int hour = ((getDateTime()[11] - '0') * 10) + (getDateTime()[12] - '0');
    int min =  ((getDateTime()[14] - '0') * 10) + (getDateTime()[15] - '0');
    int sec =  ((getDateTime()[17] - '0') * 10) + (getDateTime()[18] - '0');
    int secsSinceMidnight = (hour * 60 * 60) + (min * 60) + sec;
    mBaseFrame = getFramesFromSec (secsSinceMidnight);
    printf ("cHls::changeChan- baseSeqNum:%d dateTime:%s %dh %dm %ds %d baseFrame:%d\n",
            getBaseSeqNum(), getDateTime().c_str(), hour, min, sec, secsSinceMidnight, mBaseFrame);

    invalidateChunks();
    return mBaseFrame;
    }
  //}}}
  //{{{
  void setBitrate (int bitrate) {

    mBitrate = bitrate;
    }
  //}}}

  //{{{
  bool load (cHttp* http, int frame) {
  // return false if any load failed

    bool ok = true;

    mLoading = 0;
    int chunk;
    int seqNum = getSeqNumFromFrame (frame);

    if (!findSeqNumChunk (seqNum, mBitrate, 0, chunk)) {
      // load required chunk
      mLoading++;
      ok &= mChunks[chunk].load (http, this, seqNum, mBitrate);
      }

    if (!mJumped) {
      if (!findSeqNumChunk (seqNum, mBitrate, 1, chunk)) {
        // load chunk before
        mLoading++;
        ok &= mChunks[chunk].load (http, this, seqNum+1, mBitrate);
        }

      if (!findSeqNumChunk (seqNum, mBitrate, -1, chunk)) {
        // load chunk after
        mLoading++;
        ok &= mChunks[chunk].load (http, this, seqNum-1, mBitrate);
        }
      }
    mJumped = false;
    mLoading = 0;

    return ok;
    }
  //}}}

private:
  const int kFramesPerPlay = 1;
  //{{{
  int getSeqNumFromFrame (int frame) {
  // works for -ve frame

    int r = frame - mBaseFrame;
    if (r >= 0)
      return getBaseSeqNum() + (r / getFramesPerChunk());
    else
      return getBaseSeqNum() - 1 - ((-r-1)/ getFramesPerChunk());
    }
  //}}}
  //{{{
  int getFrameInChunkFromFrame (int frame) {
  // works for -ve frame

    int r = (frame - mBaseFrame) % getFramesPerChunk();
    return r < 0 ? r + getFramesPerChunk() : r;
    }
  //}}}
  //{{{
  std::string getChunkNumStr (int chunk) {
    return mChunks[chunk].getSeqNum() ? toString (mChunks[chunk].getSeqNum() - getBaseSeqNum()) : "*";
    }
  //}}}

  //{{{
  bool findFrame (int frame, int& seqNum, int& chunk, int& frameInChunk) {
  // return true, seqNum, chunk and frameInChunk of loadedChunk from frame
  // - return false if not found

    auto findSeqNum = getSeqNumFromFrame (frame);
    for (auto i = 0; i < 3; i++) {
      if ((mChunks[i].getSeqNum() != 0) && (findSeqNum == mChunks[i].getSeqNum())) {
        auto findFrameInChunk = getFrameInChunkFromFrame (frame);
        if ((mChunks[i].getFramesLoaded() > 0) && (findFrameInChunk < mChunks[i].getFramesLoaded())) {
          seqNum = findSeqNum;
          chunk = i;
          frameInChunk = findFrameInChunk;
          return true;
          }
        }
      }

    seqNum = 0;
    chunk = 0;
    frameInChunk = 0;
    return false;
    }
  //}}}
  //{{{
  bool findSeqNumChunk (int seqNum, int bitrate, int offset, int& chunk) {
  // return true if match found
  // - if not, chunk = best reuse
  // - reuse same seqNum chunk if diff bitrate ?

    // look for matching chunk
    chunk = 0;
    while (chunk < 3) {
      if (seqNum + offset == mChunks[chunk].getSeqNum())
        return true;
        //return bitrate != mChunks[chunk].getBitrate();
      chunk++;
      }

    // look for stale chunk
    chunk = 0;
    while (chunk < 3) {
      if ((mChunks[chunk].getSeqNum() < seqNum-1) || (mChunks[chunk].getSeqNum() > seqNum+1))
        return false;
      chunk++;
      }

    printf ("cHls::findSeqNumChunk problem %d", seqNum);
    chunk = 0;
    return false;
    }
  //}}}
  //{{{
  void invalidateChunks() {
    for (auto i = 0; i < 3; i++)
      mChunks[i].invalidate();
    }
  //}}}

  // private vars
  int mBaseFrame = 0;
  int mBitrate = 0;
  int mLoading = 0;
  bool mJumped = false;
  std::string mInfoStr;
  cHlsChunk mChunks[3];
  };
