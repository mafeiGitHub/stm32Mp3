// cHlsChunk.h
#pragma once
#include "cHlsChan.h"

class cHlsChunk {
public:
  //{{{
  cHlsChunk() {
    // create aac decoder
    mDecoder = NeAACDecOpen();
    NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
    config->outputFormat = FAAD_FMT_16BIT;
    NeAACDecSetConfiguration (mDecoder, config);

    mAudio = (int16_t*)pvPortMalloc (300 * 1024 * 2 * 2);
    mPower = (uint8_t*)malloc (300 * 2);
    }
  //}}}
  //{{{
  ~cHlsChunk() {
    NeAACDecClose (mDecoder);
    if (mPower)
      free (mPower);
    if (mAudio)
      vPortFree (mAudio);
    }
  //}}}

  // gets
  //{{{
  int getSeqNum() {
    return mSeqNum;
    }
  //}}}
  //{{{
  int getBitrate() {
    return mBitrate;
    }
  //}}}
  //{{{
  int getFramesLoaded() {
    return mFramesLoaded;
    }
  //}}}
  //{{{
  std::string getInfoStr() {
    return mInfoStr;
    }
  //}}}
  //{{{
  uint8_t* getPower (int frameInChunk, int& frames) {

    frames = getFramesLoaded() - frameInChunk;
    return mPower ? mPower + (frameInChunk * 2) : nullptr;
    }
  //}}}
  //{{{
  int16_t* getSamples (int frameInChunk) {
    return mAudio ? (mAudio + (frameInChunk * mSamplesPerFrame * 2)) : nullptr;
    }
  //}}}

  //{{{
  bool load (cHttp* http, cHlsChan* hlsChan, int seqNum, int bitrate) {

    //cLcd::debug ("chunk load " + cLcd::dec (seqNum));
    mFramesLoaded = 0;
    mSeqNum = seqNum;
    mBitrate = bitrate;

    auto response = http->get (hlsChan->getHost(), hlsChan->getTsPath (seqNum, mBitrate));
    if (response == 200) {
      // aacHE has double size frames, treat as two normal frames
      int framesPerAacFrame = mBitrate <= 48000 ? 2 : 1;
      mSamplesPerFrame = hlsChan->getSamplesPerFrame();
      int samplesPerAacFrame = mSamplesPerFrame * framesPerAacFrame;

      auto loadPtr = http->getContent();
      auto loadEnd = http->getContentEnd();
      loadEnd = packTsBuffer (loadPtr, loadEnd);

      int16_t* buffer = mAudio;
      NeAACDecFrameInfo frameInfo;

      if (!mDecoderInited) {
        unsigned long sampleRate;
        uint8_t chans;
        NeAACDecInit (mDecoder, loadPtr, 2048, &sampleRate, &chans);
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
        mDecoderInited = true;
        }

      uint8_t* powerPtr = mPower;
      while (loadPtr < loadEnd) {
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
        loadPtr += frameInfo.bytesconsumed;
        for (int i = 0; i < framesPerAacFrame; i++) {
          //{{{  calc left, right power
          int valueL = 0;
          int valueR = 0;
          for (int j = 0; j < mSamplesPerFrame; j++) {
            short sample = (*buffer++) >> 4;
            valueL += sample * sample;
            sample = (*buffer++) >> 4;
            valueR += sample * sample;
            }

          uint8_t leftPix = (uint8_t)sqrt(valueL / (mSamplesPerFrame * 32.0f));
          *powerPtr++ = (272/2) - leftPix;
          *powerPtr++ = leftPix + (uint8_t)sqrt(valueR / (mSamplesPerFrame * 32.0f));
          mFramesLoaded++;
          }
          //}}}
        }

      http->freeContent();
      mInfoStr = "ok " + toString (seqNum) + ':' + toString (bitrate/1000) + 'k';
      return true;
      }
    else {
      mSeqNum = 0;
      mInfoStr = toString(response) + ':' + toString (seqNum) + ':' + toString (bitrate/1000) + "k " + http->getInfoStr();
      return false;
      }
    }
  //}}}
  //{{{
  void invalidate() {

    mSeqNum = 0;
    mFramesLoaded = 0;
    }
  //}}}

private:
  //{{{
  uint8_t* packTsBuffer (uint8_t* ptr, uint8_t* endPtr) {
  // pack transportStream down to aac frames in same buffer

    auto toPtr = ptr;
    while ((ptr < endPtr) && (*ptr++ == 0x47)) {
      auto payStart = *ptr & 0x40;
      auto pid = ((*ptr & 0x1F) << 8) | *(ptr+1);
      auto headerBytes = (*(ptr+2) & 0x20) ? (4 + *(ptr+3)) : 3;
      ptr += headerBytes;
      auto tsFrameBytesLeft = 187 - headerBytes;
      if (pid == 34) {
        if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == 0xC0)) {
          int pesHeaderBytes = 9 + *(ptr+8);
          ptr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (toPtr, ptr, tsFrameBytesLeft);
        toPtr += tsFrameBytesLeft;
        }
      ptr += tsFrameBytesLeft;
      }

    return toPtr;
    }
  //}}}

  // vars
  NeAACDecHandle mDecoder = 0;
  bool mDecoderInited = false;
  int16_t* mAudio = nullptr;
  uint8_t* mPower = nullptr;

  int mSeqNum = 0;
  int mBitrate = 0;
  int mFramesLoaded = 0;
  int mSamplesPerFrame = 0;
  std::string mInfoStr;
  };
