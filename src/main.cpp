// main.cpp
//{{{  includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

#include "memory.h"
#include "cmsis_os.h"

#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "ipv4/lwip/ip_addr.h"
#include "lwip/api.h"
#include "os/ethernetif.h"

#include "cHttp.h"
#include "../httpServer/httpServer.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery.h"
  #include "stm32746g_discovery_ts.h"
  #include "stm32746g_discovery_audio.h"
  #include "stm32746g_discovery_sd.h"
#else
  #include "stm32f769i_discovery.h"
  #include "stm32f769i_discovery_ts.h"
  #include "stm32f769i_discovery_audio.h"
  #include "stm32f769i_discovery_sd.h"
#endif

#include "../fatfs/fatFs.h"

#include "decoders/cMp3decoder.h"
#include "../aac/neaacdec.h"

#include "cLcd.h"
#include "widgets/cRootContainer.h"
#include "widgets/cWidget.h"
#include "widgets/cListWidget.h"
#include "widgets/cTextBox.h"
#include "widgets/cValueBox.h"
#include "widgets/cSelectValueBox.h"
#include "widgets/cWaveWidget.h"
#include "widgets/cWaveCentreWidget.h"
#include "widgets/cWaveLensWidget.h"
//}}}

//{{{
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
  void setChan (cHttp& http, int chan) {

    mChan = chan;
    mHost = "as-hls-uk-live.bbcfmt.vo.llnwd.net";

    cLcd::debug (mHost);
    if (http.get (mHost, getM3u8path()) == 302) {
      mHost = http.getRedirectedHost();
      http.get (mHost, getM3u8path());
      cLcd::debug (mHost);
      }
    else
      mHost = http.getRedirectedHost();
    cLcd::debug (getM3u8path());

    // find #EXT-X-MEDIA-SEQUENCE in .m3u8, point to seqNum string, extract seqNum from playListBuf
    auto extSeq = strstr ((char*)http.getContent(), "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
    auto extSeqEnd = strchr (extSeq, '\n');
    *extSeqEnd = '\0';
    mBaseSeqNum = atoi (extSeq) + 3;

    auto extDateTime = strstr (extSeqEnd + 1, "#EXT-X-PROGRAM-DATE-TIME:") + strlen ("#EXT-X-PROGRAM-DATE-TIME:");
    auto extDateTimeEnd = strchr (extDateTime, '\n');
    *extDateTimeEnd = '\0';
    mDateTime = extDateTime;

    mChanInfoStr = toString (http.getResponse()) + ' ' + http.getInfoStr() + ' ' + getM3u8path() + ' ' + mDateTime;
    cLcd::debug (mDateTime + " " + cLcd::dec (mBaseSeqNum));
    }
  //}}}

private:
  //{{{  const
  const int kSampleRate = 48000;
  const int kSamplesPerFrame = 1024;
  const int kFramesPerChunk = 300;

  const int kPool        [7] = {      0,      7,      7,      7,      6,      6,      6 };
  const char* kChanNames [7] = { "none", "radio1", "radio2", "radio3", "radio4", "radio5", "radio6" };
  const char* kPathNames [7] = { "none", "bbc_radio_one", "bbc_radio_two", "bbc_radio_three",
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
//}}}
//{{{
class cHlsChunk {
public:
  //{{{
  cHlsChunk() {
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
  int getSeqNum() { return mSeqNum; }
  int getBitrate() { return mBitrate; }
  int getFramesLoaded() { return mFramesLoaded; }
  std::string getInfoStr() { return mInfoStr; }

  int16_t* getSamples (int frameInChunk) { return (mAudio + (frameInChunk * mSamplesPerFrame * 2)); }
  //{{{
  uint8_t* getPower (int frameInChunk, int& frames) {
    frames = getFramesLoaded() - frameInChunk;
    return mPower + (frameInChunk * 2);
    }
  //}}}

  //{{{
  bool load (cHttp& http, cHlsChan* hlsChan, int seqNum, int bitrate) {

    mFramesLoaded = 0;
    mSeqNum = seqNum;
    mBitrate = bitrate;

    auto response = http.get (hlsChan->getHost(), hlsChan->getTsPath (seqNum, mBitrate));
    if (response == 200) {
      // aacHE has double size frames, treat as two normal frames
      int framesPerAacFrame = mBitrate <= 48000 ? 2 : 1;
      mSamplesPerFrame = hlsChan->getSamplesPerFrame();
      int samplesPerAacFrame = mSamplesPerFrame * framesPerAacFrame;

      auto loadPtr = http.getContent();
      auto loadEnd = http.getContentEnd();
      loadEnd = packTsBuffer (loadPtr, loadEnd);

      int16_t* buffer = mAudio;

      if (!mDecoder) {
        //{{{  create, init aac decoder
        mDecoder = NeAACDecOpen();

        NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
        config->outputFormat = FAAD_FMT_16BIT;
        NeAACDecSetConfiguration (mDecoder, config);

        unsigned long sampleRate;
        uint8_t chans;
        NeAACDecInit (mDecoder, loadPtr, 2048, &sampleRate, &chans);

        NeAACDecFrameInfo frameInfo;
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
        }
        //}}}

      uint8_t* powerPtr = mPower;
      while (loadPtr < loadEnd) {
        NeAACDecFrameInfo frameInfo;
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

      http.freeContent();
      mInfoStr = "ok " + toString (seqNum) + ':' + toString (bitrate/1000) + 'k';
      return true;
      }
    else {
      mSeqNum = 0;
      mInfoStr = toString (response) + ':' + toString (seqNum) + ':' + toString (bitrate/1000) + "k " + http.getInfoStr();
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
  // pack transportStream to aac frames in same buffer

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
  int16_t* mAudio = nullptr;
  uint8_t* mPower = nullptr;

  int mSeqNum = 0;
  int mBitrate = 0;
  int mFramesLoaded = 0;
  int mSamplesPerFrame = 0;
  std::string mInfoStr;
  };
//}}}
//{{{
class cHlsLoader : public cHlsChan {
public:
  cHlsLoader() {}
  virtual ~cHlsLoader() {}

  int getBitrate() { return mBitrate; }
  int getLoading() { return mLoading; }
  //{{{
  std::string getInfoStr (int frame) {
    return getChanName (getChan()) + ':' + toString (getBitrate()/1000) + "k " + getFrameInfo (frame);
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
  int changeChan (int chan) {

    setChan (mHttp, chan);
    mBitrate = getMidBitrate();

    int hour = ((getDateTime()[11] - '0') * 10) + (getDateTime()[12] - '0');
    int min =  ((getDateTime()[14] - '0') * 10) + (getDateTime()[15] - '0');
    int sec =  ((getDateTime()[17] - '0') * 10) + (getDateTime()[18] - '0');
    int secsSinceMidnight = (hour * 60 * 60) + (min * 60) + sec;
    mBaseFrame = getFramesFromSec (secsSinceMidnight);

    invalidateChunks();
    return mBaseFrame;
    }
  //}}}
  void setBitrate (int bitrate) { mBitrate = bitrate; }

  //{{{
  bool load (int frame) {
  // return false if any load failed

    bool ok = true;

    mLoading = 0;
    int chunk;
    int seqNum = getSeqNumFromFrame (frame);

    if (!findSeqNumChunk (seqNum, mBitrate, 0, chunk)) {
      // load required chunk
      mLoading++;
      ok &= mChunks[chunk].load (mHttp, this, seqNum, mBitrate);
      }

    if (!findSeqNumChunk (seqNum, mBitrate, 1, chunk)) {
      // load chunk before
      mLoading++;
      ok &= mChunks[chunk].load (mHttp, this, seqNum+1, mBitrate);
      }

    if (!findSeqNumChunk (seqNum, mBitrate, -1, chunk)) {
      // load chunk after
      mLoading++;
      ok &= mChunks[chunk].load (mHttp, this, seqNum-1, mBitrate);
      }
    mLoading = 0;

    return ok;
    }
  //}}}

private:
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
  cHttp mHttp;
  cHlsChunk mChunks[3];
  int mBaseFrame = 0;
  int mBitrate = 0;
  int mLoading = 0;
  std::string mInfoStr;
  };
//}}}

//{{{  static vars
static osSemaphoreId mAudSem;
static bool mAudHalf = false;

static int mPlayFrame = 0;

static float mVolume = 0.7f;
static int mIntVolume = 0;
static bool mVolumeChanged = false;

// mp3
static int fileIndex = 0;
static bool fileIndexChanged = false;
static std::vector<std::string> mMp3Files;

static bool mWaveLoad = false;
static bool mWaveChanged = false;
static int mWaveLoadFrame = 0;
static uint8_t* mWave = nullptr;
static int* mFrameOffsets = nullptr;

// hls
static int mTuneChan = 4;
static bool mTuneChanChanged = false;
static cHlsLoader* mHlsLoader;
static osSemaphoreId mHlsLoaderSem;
static std::string mInfoStr;

// ui
static cLcd* mLcd = nullptr;
static cRootContainer* mRoot = nullptr;
//}}}
//{{{  audio callback
//{{{
void BSP_AUDIO_OUT_HalfTransfer_CallBack() {
  mAudHalf = true;
  osSemaphoreRelease (mAudSem);
  }
//}}}
//{{{
void BSP_AUDIO_OUT_TransferComplete_CallBack() {
  mAudHalf = false;
  osSemaphoreRelease (mAudSem);
  }
//}}}
//}}}

//{{{
class cPowerWidget : public cWidget {
public:
  cPowerWidget (cHlsLoader* hlsLoader, uint16_t width, uint16_t height) :
    cWidget (COL_BLUE, width, height), mHlsLoader(hlsLoader) {}
  virtual ~cPowerWidget() {}

  virtual void render (iDraw* draw) {
    uint8_t* power = nullptr;
    int frame = mPlayFrame - mWidth/2;
    int frames = 0;
    for (auto x = 0; x < mWidth; x++, frame++) {
      if (frames <= 0)
        power = mHlsLoader->getPower (frame, frames);
      if (power) {
        uint8_t top = *power++;
        uint8_t ylen = *power++;
        draw->rectClipped (COL_BLUE, x, top, 1, ylen);
        frames--;
        }
      }
    }
private:
  cHlsLoader* mHlsLoader;
  };
//}}}
//{{{
class cInfoTextBox : public cWidget {
public:
  cInfoTextBox (uint16_t width) : cWidget (width) {}
  virtual ~cInfoTextBox() {}

  virtual void render (iDraw* draw) {
    draw->text (COL_WHITE, getFontHeight(), mHlsLoader->getChunkInfoStr (0).c_str(), mX+2, mY+1, mWidth-1, mHeight-1);
    }
  };
//}}}

//{{{
static void aacLoadThread (void const* argument) {

  cLcd::debug ("aacLoadThread");
  mPlayFrame = mHlsLoader->changeChan (mTuneChan) - mHlsLoader->getFramesFromSec (19);
  mLcd->setShowDebug (false, false, false, true);  // debug - title, info, lcdStats, footer

  while (true) {
    if (mTuneChanChanged && (mHlsLoader->getChan() != mTuneChan)) {
      mPlayFrame = mHlsLoader->changeChan (mTuneChan) - mHlsLoader->getFramesFromSec (19);
      mTuneChanChanged = false;
      }

    if (!mHlsLoader->load (mPlayFrame))
      osDelay (1000);

    osSemaphoreWait (mHlsLoaderSem, osWaitForever);
    }
  }
//}}}
//{{{
static void aacPlayThread (void const* argument) {

  cLcd::debug ("aacPlayThread");

  #ifdef STM32F746G_DISCO
    BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_HEADPHONE, int(mVolume * 100), 48000);
    BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);
  #else
    BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_SPEAKER, int(mVolume * 100), 48000);
    BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_13);
  #endif

  memset ((void*)AUDIO_BUFFER, 0, 8192);
  BSP_AUDIO_OUT_Play ((uint16_t*)AUDIO_BUFFER, 8192);

  int lastSeqNum = 0;
  while (true) {
    if (osSemaphoreWait (mAudSem, 50) == osOK) {
      int seqNum;
      int16_t* audioSamples = mHlsLoader->getSamples (mPlayFrame, seqNum);
      if (audioSamples) {
        memcpy ((int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER + 4096), audioSamples, 4096);
        mPlayFrame++;
        }
      else
        memset ((int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER + 4096), 0, 4096);

      if (!seqNum || (seqNum != lastSeqNum)) {
        lastSeqNum = seqNum;
        osSemaphoreRelease (mHlsLoaderSem);
        }
      }
    }
  }
//}}}

//{{{
static void listDirectory (std::string directoryName, std::string indent) {

  cLcd::debug ("dir " + directoryName);

  cDirectory directory (directoryName);
  if (directory.getError()) {
    //{{{  open error
    cLcd::debug (COL_RED, "directory open error:"  + cLcd::dec (directory.getError()));
    return;
    }
    //}}}

  cFileInfo fileInfo;
  while ((directory.find (fileInfo) == FR_OK) && !fileInfo.getEmpty()) {
    if (fileInfo.getBack()) {
      //cLcd::debug (fileInfo.getName());
      }

    else if (fileInfo.isDirectory())
      listDirectory (directoryName + "/" + fileInfo.getName(), indent + "-");

    else if (fileInfo.matchExtension ("MP3")) {
      mMp3Files.push_back (directoryName + "/" + fileInfo.getName());
      cLcd::debug (indent + fileInfo.getName());
      cFile file (directoryName + "/" + fileInfo.getName(), FA_OPEN_EXISTING | FA_READ);
      //cLcd::debug ("- filesize " + cLcd::intStr (file.getSize()));
      }
    }
  }
//}}}
//{{{
static void mp3PlayThread (void const* argument) {

  cLcd::debug ("mp3PlayThread");

  //{{{  mount fatfs
  cFatFs* fatFs = cFatFs::create();
  if (fatFs->mount() != FR_OK) {
    //{{{  fatfs mount error, return
    cLcd::debug ("fatFs mount problem");
    osThreadTerminate (NULL);
    return;
    }
    //}}}
  cLcd::debug (fatFs->getLabel() + " vsn:" + cLcd::hex (fatFs->getVolumeSerialNumber()) +
               " freeSectors:" + cLcd::dec (fatFs->getFreeSectors()));
  //}}}
  mLcd->setShowDebug (false, false, false, true);  // disable debug - title, info, lcdStats, footer

  listDirectory ("", "");

  auto mp3Decoder = new cMp3Decoder;
  cLcd::debug ("play mp3Decoder");

  #ifdef STM32F746G_DISCO
    BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_HEADPHONE, int(mVolume * 100), 44100);
    BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);
  #else
    BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_SPEAKER, int(mVolume * 100), 44100);
    BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_13);
  #endif

  //{{{  chunkSize and buffer
  auto chunkSize = 4096;
  auto fullChunkSize = 2048 + chunkSize;
  auto chunkBuffer = (uint8_t*)pvPortMalloc (fullChunkSize);
  //}}}
  while (true) {
    cFile file (mMp3Files[fileIndex], FA_OPEN_EXISTING | FA_READ);
    if (file.getError())
      cLcd::debug ("- play open failed " + cLcd::dec (file.getError()) + " " + mMp3Files[fileIndex]);
    else {
      cLcd::debug ("play " + mMp3Files[fileIndex] + " " + cLcd::dec (file.getSize()));
      mWaveLoad = true;

      memset ((void*)AUDIO_BUFFER, 0, AUDIO_BUFFER_SIZE);
      BSP_AUDIO_OUT_Play ((uint16_t*)AUDIO_BUFFER, AUDIO_BUFFER_SIZE);
      //{{{  play fileindex file
      int count = 0;
      int bytesLeft = 0;
      mPlayFrame = 0;

      do {
        count++;
        FRESULT fresult = file.read (chunkBuffer, fullChunkSize, bytesLeft);
        if (fresult) {
          //{{{  error
          cLcd::debug ("play read " + cLcd::dec (count) + " " + cLcd::dec (fresult));
          osDelay (100);
          goto exitPlay;
          }
          //}}}
        if (bytesLeft) {
          auto chunkPtr = chunkBuffer;
          int headerBytes;
          do {
            headerBytes = mp3Decoder->findNextHeader (chunkPtr, bytesLeft);
            if (headerBytes) {
              chunkPtr += headerBytes;
              bytesLeft -= headerBytes;
              if (bytesLeft < mp3Decoder->getFrameBodySize() + 4) {
                // partial frameBody+nextHeader, move bytesLeft to front of chunkBuffer,  next read partial buffer 32bit aligned
                auto nextChunkPtr = chunkBuffer + ((4 - (bytesLeft & 3)) & 3);
                memcpy (nextChunkPtr, chunkPtr, bytesLeft);

                // read next chunks worth, including rest of frame, 32bit aligned
                int bytesLoaded;
                count++;
                FRESULT fresult = file.read (nextChunkPtr + bytesLeft, chunkSize, bytesLoaded);
                if (fresult) {
                  //{{{  error
                  cLcd::debug ("play read " + cLcd::dec (count) + " " + cLcd::dec (fresult));
                  osDelay (100);
                  goto exitPlay;
                  }
                  //}}}
                if (bytesLoaded) {
                  chunkPtr = nextChunkPtr;
                  bytesLeft += bytesLoaded;
                  }
                else
                  bytesLeft = 0;
                }
              if (bytesLeft >= mp3Decoder->getFrameBodySize()) {
                osSemaphoreWait (mAudSem, 100);
                auto frameBytes = mp3Decoder->decodeFrameBody (chunkPtr, nullptr, (int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER_HALF));
                if (frameBytes) {
                  chunkPtr += frameBytes;
                  bytesLeft -= frameBytes;
                  }
                else
                  bytesLeft = 0;
                mPlayFrame++;
                }
              }
            if (mWaveChanged) {
              //{{{  seek new position
              file.seek (mFrameOffsets [mPlayFrame] & 0xFFFFFFE0);
              mWaveChanged = false;
              headerBytes = 0;
              }
              //}}}
            } while (!fileIndexChanged && headerBytes && (bytesLeft > 0));
          }
        } while (!fileIndexChanged && (bytesLeft > 0));
      exitPlay:
      //}}}
      BSP_AUDIO_OUT_Stop (CODEC_PDWN_SW);
      }

    if (!fileIndexChanged)
      fileIndex = fileIndex >= (int)mMp3Files.size() ? 0 : fileIndex + 1;
    fileIndexChanged = false;
    }
  }
//}}}
//{{{
static void waveThread (void const* argument) {

  cLcd::debug ("waveThread");
  auto mp3Decoder = new cMp3Decoder;
  cLcd::debug ("wave mp3Decoder");

  auto chunkSize = 0x10000 - 2048; // 64k
  auto fullChunkSize = 2048 + chunkSize;
  auto chunkBuffer = (uint8_t*)pvPortMalloc (fullChunkSize);

  int loadedFileIndex = -1;
  while (true) {
    loadedFileIndex = fileIndex;

    mWaveLoadFrame = 0;
    mWave[0] = 0;
    auto wavePtr = mWave + 1;

    int count = 0;
    cLcd::debug ("wave load " + mMp3Files[fileIndex]);
    cFile file (mMp3Files[fileIndex], FA_OPEN_EXISTING | FA_READ);
    if (file.getError())
      cLcd::debug ("- wave open failed " + cLcd::dec (file.getError()) + " " + mMp3Files[fileIndex]);
    else {
      int bytesLeft = 0;
      do {
        count++;
        FRESULT fresult = file.read (chunkBuffer, fullChunkSize, bytesLeft);;
        if (fresult) {
          //{{{  error
          cLcd::debug ("wave read " + cLcd::dec (count) + " " + cLcd::dec (fresult));
          osDelay (100);
          goto exitWave;
          }
          //}}}
        if (bytesLeft) {
          auto chunkPtr = chunkBuffer;
          int headerBytes;
          do {
            headerBytes = mp3Decoder->findNextHeader (chunkPtr, bytesLeft);
            if (headerBytes) {
              chunkPtr += headerBytes;
              bytesLeft -= headerBytes;
              if (bytesLeft < mp3Decoder->getFrameBodySize() + 4) { // not enough for frameBody and next header
                // move bytesLeft to front of chunkBuffer,  next read partial buffer 32bit aligned
                auto nextChunkPtr = chunkBuffer + ((4 - (bytesLeft & 3)) & 3);
                memcpy (nextChunkPtr, chunkPtr, bytesLeft);

                // read next chunks worth, including rest of frame, 32bit aligned
                count++;
                int bytesLoaded;
                FRESULT fresult = file.read (nextChunkPtr + bytesLeft, chunkSize, bytesLoaded);
                if (fresult) {
                  //{{{  error
                  cLcd::debug ("wave read " + cLcd::dec (count) + " " + cLcd::dec (fresult));
                  osDelay (100);
                  goto exitWave;
                  }
                  //}}}
                if (bytesLoaded) {
                  chunkPtr = nextChunkPtr;
                  bytesLeft += bytesLoaded;
                  }
                else
                  bytesLeft = 0;
                }
              if (bytesLeft >= mp3Decoder->getFrameBodySize()) {
                mFrameOffsets[mWaveLoadFrame] = file.getPosition(); // not right !!!!
                auto frameBytes = mp3Decoder->decodeFrameBody (chunkPtr, mWave + 1 + (mWaveLoadFrame * 2), nullptr);
                if (*wavePtr > *mWave)
                  *mWave = *wavePtr;
                wavePtr++;
                if (*wavePtr > *mWave)
                  *mWave = *wavePtr;
                wavePtr++;
                mWaveLoadFrame++;

                if (frameBytes) {
                  chunkPtr += frameBytes;
                  bytesLeft -= frameBytes;
                  }
                else
                  bytesLeft = 0;
                }
              }
            } while ((fileIndex == loadedFileIndex) && headerBytes && (bytesLeft > 0));
          }
        } while ((fileIndex == loadedFileIndex) && (bytesLeft > 0));
      }
  exitWave:
    cLcd::debug ("wave loaded");

    // wait for file change
    while (fileIndex == loadedFileIndex)
      osDelay (100);
    }
  }
//}}}

//{{{
static void netThread (void const* argument) {

  const bool kStaticIp = false;
  tcpip_init (NULL, NULL);
  cLcd::debug ("configuring ethernet");

  // init LwIP stack
  struct ip_addr ipAddr;
  IP4_ADDR (&ipAddr, 192, 168, 1, 67);
  struct ip_addr netmask;
  IP4_ADDR (&netmask, 255, 255 , 255, 0);
  struct ip_addr gateway;
  IP4_ADDR (&gateway, 192, 168, 0, 1);

  struct netif netIf;
  netif_add (&netIf, &ipAddr, &netmask, &gateway, NULL, &ethernetif_init, &tcpip_input);
  netif_set_default (&netIf);

  if (netif_is_link_up (&netIf)) {
    netif_set_up (&netIf);
    if (kStaticIp)
      //{{{  static ip
      cLcd::debug (COL_YELLOW, "ethernet static ip " + cLcd::dec ((int) (netIf.ip_addr.addr & 0xFF)) + "." +
                                                       cLcd::dec ((int)((netIf.ip_addr.addr >> 16) & 0xFF)) + "." +
                                                       cLcd::dec ((int)((netIf.ip_addr.addr >> 8) & 0xFF)) + "." +
                                                       cLcd::dec ((int) (netIf.ip_addr.addr >> 24)));
      //}}}
    else {
      //{{{  dhcp ip
      struct ip_addr nullIpAddr;
      IP4_ADDR (&nullIpAddr, 0, 0, 0, 0);
      netIf.ip_addr = nullIpAddr;
      netIf.netmask = nullIpAddr;
      netIf.gw = nullIpAddr;
      dhcp_start (&netIf);

      while (true) {
        if (netIf.ip_addr.addr) {
          //{{{  dhcp allocated
          cLcd::debug (COL_YELLOW, "dhcp allocated " + cLcd::dec ( (int)(netIf.ip_addr.addr & 0xFF)) + "." +
                                                       cLcd::dec ((int)((netIf.ip_addr.addr >> 16) & 0xFF)) + "." +
                                                       cLcd::dec ((int)((netIf.ip_addr.addr >> 8) & 0xFF)) + "." +
                                                       cLcd::dec ( (int)(netIf.ip_addr.addr >> 24)));
          dhcp_stop (&netIf);
          break;
          }
          //}}}
        else if (netIf.dhcp->tries > 4) {
          //{{{  dhcp timeout
          cLcd::debug (COL_RED, "dhcp timeout");
          dhcp_stop (&netIf);

          // use static address
          struct ip_addr ipAddr;
          IP4_ADDR (&ipAddr, 192 ,168 , 1 , 67 );
          struct ip_addr netmask;
          IP4_ADDR (&netmask, 255, 255, 255, 0);
          struct ip_addr gateway;
          IP4_ADDR (&gateway, 192, 168, 0, 1);
          netif_set_addr (&netIf, &ipAddr , &netmask, &gateway);
          break;
          }
          //}}}
        osDelay (250);
        }
      }
      //}}}

    const osThreadDef_t osThreadAacLoad =  { (char*)"AacLoad", aacLoadThread, osPriorityNormal, 0, 15000 };
    osThreadCreate (&osThreadAacLoad, NULL);
    const osThreadDef_t osThreadAacPlay =  { (char*)"AacPlay", aacPlayThread, osPriorityAboveNormal, 0, 2000 };
    osThreadCreate (&osThreadAacPlay, NULL);

    httpServerInit();
    }
  else {
    //{{{  no ethernet
    netif_set_down (&netIf);
    cLcd::debug (COL_RED, "no ethernet");
    }
    //}}}

  osThreadTerminate (NULL);
  }
//}}}
//{{{
static void mainThread (void const* argument) {

  const bool kMaxTouch = 1;
  mLcd->displayOn();
  cLcd::debug ("mainThread");

  BSP_SD_Init();
  if (BSP_SD_IsDetected() == SD_PRESENT) {
    //{{{  mp3 player
    mFrameOffsets = (int*)pvPortMalloc (60*60*40*sizeof(int));
    mWave = (uint8_t*)pvPortMalloc (60*60*40*2*sizeof(uint8_t));  // 1 hour of 40 mp3 frames per sec
    mWave[0] = 0;
    mWaveLoadFrame = 0;

    mRoot->addTopLeft (new cListWidget (mMp3Files, fileIndex, fileIndexChanged,
                                        mRoot->getWidth(), mRoot->getHeight()-2*mRoot->getHeight()/5));
    mRoot->addBottomLeft (new cWaveLensWidget (mWave, mPlayFrame, mWaveLoadFrame, mWaveLoadFrame, mWaveChanged,
                                               mRoot->getWidth(), mRoot->getHeight()/5));
    mRoot->addNextAbove (new cWaveCentreWidget (mWave, mPlayFrame, mWaveLoadFrame, mWaveLoadFrame, mWaveChanged,
                                                mRoot->getWidth(), mRoot->getHeight()/5));
    const osThreadDef_t osThreadPlay =  { (char*)"Play", mp3PlayThread, osPriorityNormal, 0, 8192 };
    osThreadCreate (&osThreadPlay, NULL);
    const osThreadDef_t osThreadWave =  { (char*)"Wave", waveThread, osPriorityNormal, 0, 8192 };
    osThreadCreate (&osThreadWave, NULL);
    }
    //}}}
  else {
    //{{{  hls aac player
    mHlsLoader = new cHlsLoader();
    osSemaphoreDef (hlsLoader);
    mHlsLoaderSem = osSemaphoreCreate (osSemaphore (hlsLoader), -1);

    mRoot->addBottomLeft (new cPowerWidget (mHlsLoader, mRoot->getWidth(), mRoot->getHeight()));
    //mRoot->addTopRight (new cInfoTextBox (mRoot->getWidth()/4));
    mRoot->addTopLeft (new cSelectValueBox ("radio1", 1, mTuneChan, mTuneChanChanged,
                                            cWidget::getBoxHeight()*3,cWidget::getBoxHeight()*2));
    mRoot->addNextRight (new cSelectValueBox ("radio2", 2, mTuneChan, mTuneChanChanged,
                                              cWidget::getBoxHeight()*3,cWidget::getBoxHeight()*2));
    mRoot->addNextRight (new cSelectValueBox ("radio3", 3, mTuneChan, mTuneChanChanged,
                                              cWidget::getBoxHeight()*3,cWidget::getBoxHeight()*2));
    mRoot->addNextRight (new cSelectValueBox ("radio4", 4, mTuneChan, mTuneChanChanged,
                                              cWidget::getBoxHeight()*3,cWidget::getBoxHeight()*2));
    mRoot->addNextRight (new cSelectValueBox ("radio5", 5, mTuneChan, mTuneChanChanged,
                                              cWidget::getBoxHeight()*3,cWidget::getBoxHeight()*2));
    mRoot->addNextRight (new cSelectValueBox ("radio6", 6, mTuneChan, mTuneChanChanged,
                                              cWidget::getBoxHeight()*3,cWidget::getBoxHeight()*2));

    const osThreadDef_t osThreadNet =  { (char*)"Net", netThread, osPriorityNormal, 0, 1024 };
    osThreadCreate (&osThreadNet, NULL);
    }
    //}}}
  mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW, cWidget::getBoxHeight()*2, mRoot->getHeight()));

  //{{{  init vars
  bool button = false;
  int16_t x[kMaxTouch];
  int16_t y[kMaxTouch];
  uint8_t z[kMaxTouch];
  int pressed[kMaxTouch];
  for (auto touch = 0; touch < kMaxTouch; touch++)
    pressed[touch] = 0;
  //}}}
  BSP_TS_Init (mRoot->getWidth(), mRoot->getHeight());
  while (true) {
    TS_StateTypeDef tsState;
    BSP_TS_GetState (&tsState);
    for (auto touch = 0; touch < kMaxTouch; touch++) {
      if (touch < tsState.touchDetected) {
        //{{{  pressed
        auto xinc = pressed[touch] ? tsState.touchX[touch] - x[touch] : 0;
        auto yinc = pressed[touch] ? tsState.touchY[touch] - y[touch] : 0;
        x[touch] = tsState.touchX[touch];
        y[touch] = tsState.touchY[touch];
        z[touch] = tsState.touchWeight[touch];
        if (touch == 0)
          button ? mLcd->press (pressed[0], x[0], y[0], z[0], xinc, yinc) : mRoot->press (pressed[0], x[0], y[0], z[0], xinc, yinc);
        pressed[touch]++;
        }
        //}}}
      else {
        //{{{  released
        x[touch] = 0;
        y[touch] = 0;
        z[touch] = 0;
        if (!touch && pressed[0])
          mRoot->release();
        pressed[touch] = 0;
        }
        //}}}
      }

    //{{{  button, leds
    button = BSP_PB_GetState(BUTTON_WAKEUP) == GPIO_PIN_SET;
    button ? BSP_LED_On (LED1) : BSP_LED_Off (LED1);
    #ifdef STM32F769I_DISCO
      tsState.touchDetected ? BSP_LED_On (LED2) : BSP_LED_Off (LED2);
      button ? BSP_LED_On (LED3) : BSP_LED_Off (LED3);
    #endif
    //}}}
    mLcd->startRender();
    button ? mLcd->clear (COL_BLACK) : mRoot->render (mLcd);
    if (tsState.touchDetected)
      mLcd->renderCursor (COL_MAGENTA, x[0], y[0], z[0] ? z[0] : cLcd::getHeight()/10);
    mLcd->endRender (button);

    if (mVolumeChanged && (int(mVolume * 100) != mIntVolume)) {
      //{{{  set volume
      mIntVolume = int(mVolume * 100);
      BSP_AUDIO_OUT_SetVolume (mIntVolume);
      mVolumeChanged = false;
      }
      //}}}
    }
  }
//}}}

//{{{
static void initMpuRegions() {
// init MPU regions

  // common MPU config for writeThrough
  HAL_MPU_Disable();

  MPU_Region_InitTypeDef MPU_InitStruct;
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  // config writeThrough for SRAM1,SRAM2 0x20010000, 256kb, AXI - region0
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x20010000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
  HAL_MPU_ConfigRegion (&MPU_InitStruct);

  // config writeThrough for SDRAM 0xC0000000, 16mb - region1
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
  HAL_MPU_ConfigRegion (&MPU_InitStruct);

  HAL_MPU_Enable (MPU_PRIVILEGED_DEFAULT);
  }
//}}}
//{{{
static void initClock() {

  // Enable Power Control clock
  __HAL_RCC_PWR_CLK_ENABLE();

  // The voltage scaling allows optimizing the power consumption when the device is
  // clocked below the maximum system frequency, to update the voltage scaling value
  // regarding system frequency refer to product datasheet
   __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  //  System Clock source            = PLL (HSE)
  //  SYSCLK(Hz)                     = 216000000
  //  HCLK(Hz)                       = 216000000
  //  HSE Frequency(Hz)              = 25000000
  //  PLL_M                          = 25
  //  PLL_N                          = 432
  //  PLL_P                          = 2
  //  PLL_Q                          = 9
  //  PLL_R                          = 7
  //  VDD(V)                         = 3.3
  //  Main regulator output voltage  = Scale1 mode
  //  AHB Prescaler                  = 1
  //  APB1 Prescaler                 = 4
  //  APB2 Prescaler                 = 2
  //  Flash Latency(WS)              = 7
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  #ifdef STM32F769x
    RCC_OscInitStruct.PLL.PLLR = 7;
  #endif
  if (HAL_RCC_OscConfig (&RCC_OscInitStruct) != HAL_OK)
    while (true) {;}

  // Activate the OverDrive to reach the 216 MHz Frequency
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
    while (true) {;}

  // Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig (&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
    while (true) {;}
  }
//}}}

//{{{
int main() {

  // init system
  SCB_EnableICache();
  SCB_EnableDCache();
  HAL_Init();
  initMpuRegions();
  initClock();

  // init freeRTOS heap_5c
  HeapRegion_t xHeapRegions[] = { {(uint8_t*)SDRAM_HEAP, SDRAM_HEAP_SIZE }, { nullptr, 0 } };
  vPortDefineHeapRegions (xHeapRegions);

  // init buttons, leds
  BSP_LED_Init (LED1);
  #ifdef STM32F769I_DISCO
    BSP_LED_Init (LED2);
    BSP_LED_Init (LED3);
  #endif
  BSP_PB_Init (BUTTON_WAKEUP, BUTTON_MODE_GPIO);

  // init lcd, root widget
  mLcd = cLcd::create ("Player built at " + std::string(__TIME__) + " on " + std::string(__DATE__));
  mRoot = new cRootContainer (cLcd::getWidth(), cLcd::getHeight());

  // init audio play semaphore
  osSemaphoreDef (aud);
  mAudSem = osSemaphoreCreate (osSemaphore (aud), -1);

  // main thread
  const osThreadDef_t osMainThread = { (char*)"main", mainThread, osPriorityNormal, 0, 1024 };
  osThreadCreate (&osMainThread, NULL);

  osKernelStart();

  return 0;
  }
//}}}
