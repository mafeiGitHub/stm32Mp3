// main.cpp
//{{{  includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

#include "memory.h"

#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "ipv4/lwip/ip_addr.h"
#include "lwip/api.h"
#include "os/ethernetif.h"

#include "utils.h"

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_msc.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery.h"
  #include "stm32746g_discovery_ts.h"
  #include "stm32746g_discovery_audio.h"
  #include "stm32746g_discovery_qspi.h"
#else
  #include "stm32f769i_discovery.h"
  #include "stm32f769i_discovery_ts.h"
  #include "stm32f769i_discovery_audio.h"
  #include "stm32F769i_discovery_qspi.h"
#endif

#include "cSd.h"
#include "../fatfs/fatFs.h"

#include "cLcd.h"
#include "widgets/cRootContainer.h"
#include "widgets/cListWidget.h"
#include "widgets/cTextBox.h"
#include "widgets/cValueBox.h"
#include "widgets/cSelectText.h"
#include "widgets/cBmpWidget.h"
#include "widgets/cWaveCentreWidget.h"
#include "widgets/cWaveLensWidget.h"

#include "net/cLwipHttp.h"
#include "net/cUartEsp8266Http.h"
//#include "../httpServer/httpServer.h"
//#include "../httpServer/ftpServer.h"
#include "libfaad/neaacdec.h"
#include "hls/hls.h"

#include "decoders/cMp3.h"
//}}}

#ifdef STM32F746G_DISCO
#else
  //#define ESP8266
#endif

UART_HandleTypeDef DebugUartHandle;

const bool kSdDebug = false;
const bool kStaticIp = false;
const std::string kHello = "Player built at " + std::string(__TIME__) + " on " + std::string(__DATE__);
//{{{  new, delete
//void* operator new (size_t size) { return pvPortMalloc (size); }
//void operator delete (void* ptr) { vPortFree (ptr); }
void* operator new (size_t size) { return malloc (size); }
void operator delete (void* ptr) { free (ptr); }

void* operator new[](size_t size) { return malloc (size); }
void operator delete[](void *ptr) { free (ptr); }
//}}}

//{{{
static const uint8_t SD_InquiryData[] = {
  0x00, 0x80, 0x02, 0x02, (24 - 5), 0x00, 0x00, 0x00,
  'C', 'o', 'l', 'i', 'n', ' ', ' ', ' ',                                         // Manufacturer: 8 bytes
  'S', 'D', ' ', 'd', 'i', 's', 'k', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // Product     : 16 Bytes
  '0', '.', '9','9',                                                              // Version     : 4 Bytes
  };
//}}}
//{{{
static const USBD_StorageTypeDef USBD_DISK_fops = {
  SD_IsReady,
  SD_GetCapacity,
  SD_ReadCached,
  SD_WriteCached,
  (int8_t*)SD_InquiryData,
  };
//}}}
//{{{  static vars
static USBD_HandleTypeDef USBD_Device;

static SemaphoreHandle_t mAudSem;
static bool mAudHalf = false;
static int mIntVolume = 0;

// ui
static cLcd* mLcd = nullptr;
static cRootContainer* mRoot = nullptr;

// mp3
static int fileIndex = 0;
static bool fileIndexChanged = false;
static std::vector<std::string> mMp3Files;

static int mMp3PlayFrame = 0;
static bool mWaveLoad = false;
static bool mWaveChanged = false;
static int mWaveLoadFrame = 0;
static uint8_t* mWave = nullptr;
static int* mFrameOffsets = nullptr;

static float mMp3Volume = 0.8f;
static bool mMp3VolumeChanged = false;

// hls
static cHls* mHls;
static SemaphoreHandle_t mHlsSem;
static int16_t* mReSamples;
//}}}

//{{{  audio callbacks
//{{{
void BSP_AUDIO_OUT_HalfTransfer_CallBack() {

  mAudHalf = true;

  portBASE_TYPE taskWoken = pdFALSE;
  if (xSemaphoreGiveFromISR (mAudSem, &taskWoken) == pdTRUE)
    portEND_SWITCHING_ISR (taskWoken);
  }
//}}}
//{{{
void BSP_AUDIO_OUT_TransferComplete_CallBack() {

  mAudHalf = false;

  portBASE_TYPE taskWoken = pdFALSE;
  if (xSemaphoreGiveFromISR (mAudSem, &taskWoken) == pdTRUE)
    portEND_SWITCHING_ISR (taskWoken);
  }
//}}}
//}}}
//{{{
static void listDirectory (std::string directoryName, std::string ext) {

  debug ("dir " + directoryName);

  cDirectory directory (directoryName);
  if (directory.getError()) {
    //{{{  open error
    cLcd::get()->info (COL_RED, "directory open error:"  + dec (directory.getError()));
    return;
    }
    //}}}

  cFileInfo fileInfo;
  while ((directory.find (fileInfo) == FR_OK) && !fileInfo.getEmpty()) {
    if (fileInfo.getBack()) {
      //cLcd::debug (fileInfo.getName());
      }

    else if (fileInfo.isDirectory())
      listDirectory (directoryName + "/" + fileInfo.getName(), ext);

    else if (fileInfo.matchExtension (ext.c_str())) {
      mMp3Files.push_back (directoryName + "/" + fileInfo.getName());
      //cLcd::debug (fileInfo.getName());
      cFile file (directoryName + "/" + fileInfo.getName(), FA_OPEN_EXISTING | FA_READ);
      //cLcd::debug ("- filesize " + cLcd::intStr (file.getSize()));
      }
    }
  }
//}}}

//{{{
static void hlsLoaderThread (void const* argument) {

  #ifdef ESP8266
    cUartEsp8266Http http;
  #else
    cLwipHttp http;
  #endif

  http.initialise();

  while (true) {
    if (mHls->mChanChanged)
      mHls->setChan (http, mHls->mHlsChan, mHls->mHlsBitrate);

    mHls->loadPicAtPlayFrame (http);
    if (!mHls->loadAtPlayFrame (http))
      vTaskDelay (500);

    xSemaphoreTake (mHlsSem, portMAX_DELAY);
    }
  }
//}}}
//{{{
static void hlsPlayerThread (void const* argument) {

  // 8192 = 1024 samplesPerFrame * 2 chans * 2 bytesPperSample * 2 swing buffers
  const int kAudioBuffer = 1024 * 2 * 2 * 2;
  memset ((void*)AUDIO_BUFFER, 0, kAudioBuffer);
  BSP_AUDIO_OUT_Play ((uint16_t*)AUDIO_BUFFER, kAudioBuffer);

  uint32_t seqNum = 0;
  uint32_t lastSeqNum = 0;
  uint32_t numSamples = 0;
  uint16_t scrubCount = 0;
  double scrubSample = 0;

  while (true) {
    if (xSemaphoreTake (mAudSem, 50) == pdTRUE) {
      int16_t* sample = nullptr;
      if (mHls->getScrubbing()) {
        if (scrubCount == 0)
          scrubSample = mHls->getPlaySample();
        sample = mHls->getPlaySamples (scrubSample + (scrubCount * kSamplesPerFrame), seqNum, numSamples);
        if (scrubCount++ > 3) {
          sample = nullptr;
          scrubCount = 0;
          }
        }
      else if (mHls->getPlaying()) {
        sample = mHls->getPlaySamples (mHls->getPlaySample(), seqNum, numSamples);
        if (sample)
          mHls->incPlayFrame (1);
        }

      if (sample)
        memcpy ((int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER + kAudioBuffer/2), sample, kAudioBuffer/2);
      else
        memset ((int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER + kAudioBuffer/2), 0, kAudioBuffer/2);

      if (mHls->mChanChanged || !seqNum || (seqNum != lastSeqNum)) {
        lastSeqNum = seqNum;
        xSemaphoreGive (mHlsSem);
        }
      }
    }
  }
//}}}
//{{{
static void hlsNetThread (void const* argument) {

  tcpip_init (NULL, NULL);
  cLcd::debug ("hlsNetThread");

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

  if (!netif_is_link_up (&netIf)) {
    netif_set_down (&netIf);
    cLcd::get()->info (COL_RED, "no ethernet");
    }
  else {
    netif_set_up (&netIf);
    if (kStaticIp)
      //{{{  static ip
      cLcd::get()->info (COL_YELLOW, "ethernet static ip " + dec ((int) (netIf.ip_addr.addr & 0xFF)) + "." +
                                                      dec ((int)((netIf.ip_addr.addr >> 16) & 0xFF)) + "." +
                                                      dec ((int)((netIf.ip_addr.addr >> 8) & 0xFF)) + "." +
                                                      dec ((int) (netIf.ip_addr.addr >> 24)));
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
          // dhcp allocated
          std::string address = dec ( (int)(netIf.ip_addr.addr & 0xFF)) + "." +
                                dec ((int)((netIf.ip_addr.addr >> 16) & 0xFF)) + "." +
                                dec ((int)((netIf.ip_addr.addr >> 8) & 0xFF)) + "." +
                                dec ( (int)(netIf.ip_addr.addr >> 24));
          cLcd::get()->info (COL_YELLOW, "dhcp " + address);

          dhcp_stop (&netIf);
          break;
          }
        else if (netIf.dhcp->tries > 4) {
          // dhcp timeout
          cLcd::get()->info (COL_RED, "dhcp timeout");
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

        vTaskDelay (250);
        }
      }
      //}}}

    TaskHandle_t handle;
    xTaskCreate ((TaskFunction_t)hlsLoaderThread, "hlsLoad", 14000, 0, 3, &handle);
    xTaskCreate ((TaskFunction_t)hlsPlayerThread, "hlsPlay", 2000, 0, 4, &handle);
    //xTaskCreate ((TaskFunction_t)httpServerThread, "hlsHttp", DEFAULT_THREAD_STACKSIZE, 0, 4, &handle);
    //xTaskCreate ((TaskFunction_t)ftpServerThread, "ftp", DEFAULT_THREAD_STACKSIZE, 0, 4, &handle);
    }

  vTaskDelete (NULL);
  }
//}}}

//{{{
static void initMp3Menu (cRootContainer* root) {

  root->add (new cListWidget (mMp3Files, fileIndex, fileIndexChanged, 0, -4));
  root->add (new cWaveCentreWidget (mWave, mMp3PlayFrame, mWaveLoadFrame, mWaveLoadFrame, mWaveChanged, 0, 2));
  root->add (new cWaveLensWidget (mWave, mMp3PlayFrame, mWaveLoadFrame, mWaveLoadFrame, mWaveChanged, 0, 2));

  root->addTopRight (new cValueBox (mMp3Volume, mMp3VolumeChanged, COL_YELLOW, 0.5, 0))->setOverPick (1.5);
  }
//}}}
//{{{
static void mp3WaveThread (void const* argument) {

  cLcd::debug ("mp3WaveThread");

  auto mp3 = new cMp3;
  cLcd::debug ("wave mp3");

  auto chunkSize = 0x10000 - 2048; // 64k
  auto fullChunkSize = 2048 + chunkSize;
  auto chunkBuffer = (uint8_t*)bigMalloc (fullChunkSize, "waveChunkBuf");

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
      cLcd::debug ("- wave open failed " + dec (file.getError()) + " " + mMp3Files[fileIndex]);
    else {
      int bytesLeft = 0;
      do {
        count++;
        FRESULT fresult = file.read (chunkBuffer, fullChunkSize, bytesLeft);;
        if (fresult) {
          //{{{  error
          cLcd::debug ("wave read " + dec (count) + " " + dec (fresult));
          vTaskDelay (100);
          goto exitWave;
          }
          //}}}
        if (bytesLeft) {
          auto chunkPtr = chunkBuffer;
          int headerBytes;
          do {
            headerBytes = mp3->findNextHeader (chunkPtr, bytesLeft);
            if (headerBytes) {
              chunkPtr += headerBytes;
              bytesLeft -= headerBytes;
              if (bytesLeft < mp3->getFrameBodySize() + 4) { // not enough for frameBody and next header
                // move bytesLeft to front of chunkBuffer,  next read partial buffer 32bit aligned
                auto nextChunkPtr = chunkBuffer + ((4 - (bytesLeft & 3)) & 3);
                memcpy (nextChunkPtr, chunkPtr, bytesLeft);

                // read next chunks worth, including rest of frame, 32bit aligned
                count++;
                int bytesLoaded;
                FRESULT fresult = file.read (nextChunkPtr + bytesLeft, chunkSize, bytesLoaded);
                if (fresult) {
                  //{{{  error
                  cLcd::debug ("wave read " + dec (count) + " " + dec (fresult));
                  vTaskDelay (100);
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
              if (bytesLeft >= mp3->getFrameBodySize()) {
                mFrameOffsets[mWaveLoadFrame] = file.getPosition(); // not right !!!!
                auto frameBytes = mp3->decodeFrameBody (chunkPtr, mWave + 1 + (mWaveLoadFrame * 2), nullptr);
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
      vTaskDelay (100);
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
    vTaskDelete (NULL);
    return;
    }
    //}}}

  cLcd::debug (fatFs->getLabel() + " vsn:" + hex (fatFs->getVolumeSerialNumber()) +
               " freeSectors:" + dec (fatFs->getFreeSectors()));
  //}}}
  listDirectory ("", "MP3");

  TaskHandle_t handle;
  xTaskCreate ((TaskFunction_t)mp3WaveThread, "mp3Wave", 8192, 0, 2, &handle);

  auto mp3 = new cMp3;
  cLcd::debug ("play mp3");

  //{{{  chunkSize and buffer
  auto chunkSize = 4096;
  auto fullChunkSize = 2048 + chunkSize;
  auto chunkBuffer = (uint8_t*)bigMalloc (fullChunkSize, "mp3ChunkBuf");
  //}}}

  while (true) {
    mMp3PlayFrame = 0;
    cFile file (mMp3Files[fileIndex], FA_OPEN_EXISTING | FA_READ);
    if (file.getError())
      cLcd::debug ("- play open failed " + dec (file.getError()) + " " + mMp3Files[fileIndex]);
    else {
      cLcd::debug ("play " + mMp3Files[fileIndex] + " " + dec (file.getSize()));
      mWaveLoad = true;

      memset ((void*)AUDIO_BUFFER, 0, AUDIO_BUFFER_SIZE);
      BSP_AUDIO_OUT_Play ((uint16_t*)AUDIO_BUFFER, AUDIO_BUFFER_SIZE);
      //{{{  play fileindex file
      int count = 0;
      int bytesLeft = 0;

      do {
        count++;
        FRESULT fresult = file.read (chunkBuffer, fullChunkSize, bytesLeft);
        if (fresult) {
          //{{{  error
          cLcd::debug ("play read " + dec (count) + " " + dec (fresult));
          vTaskDelay (100);
          goto exitPlay;
          }
          //}}}
        if (bytesLeft) {
          auto chunkPtr = chunkBuffer;
          int headerBytes;
          do {
            headerBytes = mp3->findNextHeader (chunkPtr, bytesLeft);
            if (headerBytes) {
              chunkPtr += headerBytes;
              bytesLeft -= headerBytes;
              if (bytesLeft < mp3->getFrameBodySize() + 4) {
                // partial frameBody+nextHeader, move bytesLeft to front of chunkBuffer,  next read partial buffer 32bit aligned
                auto nextChunkPtr = chunkBuffer + ((4 - (bytesLeft & 3)) & 3);
                memcpy (nextChunkPtr, chunkPtr, bytesLeft);

                // read next chunks worth, including rest of frame, 32bit aligned
                int bytesLoaded;
                count++;
                FRESULT fresult = file.read (nextChunkPtr + bytesLeft, chunkSize, bytesLoaded);
                if (fresult) {
                  //{{{  error
                  cLcd::debug ("play read " + dec (count) + " " + dec (fresult));
                  vTaskDelay (100);
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
              if (bytesLeft >= mp3->getFrameBodySize()) {
                xSemaphoreTake (mAudSem, 100);
                auto frameBytes = mp3->decodeFrameBody (chunkPtr, nullptr, (int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER_HALF));
                if (frameBytes) {
                  chunkPtr += frameBytes;
                  bytesLeft -= frameBytes;
                  }
                else
                  bytesLeft = 0;
                mMp3PlayFrame++;
                }
              }
            if (mWaveChanged) {
              //{{{  seek new position
              file.seek (mFrameOffsets [mMp3PlayFrame] & 0xFFFFFFE0);
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
static void mainThread (void const* argument) {

  const bool kMaxTouch = 1;
  mLcd->displayOn();

  //{{{  setup BSP_AUDIO_OUT
  #ifdef STM32F746G_DISCO
    BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_HEADPHONE, int(mMp3Volume * 100), 48000);
    BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);
  #else
    //BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_SPEAKER, int(mVolume * 100), 48000);
    //BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_13);
    BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_HEADPHONE, int(mMp3Volume * 100), 48000);
    BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);
  #endif
  //}}}

  SD_Init();
  if (BSP_PB_GetState (BUTTON_WAKEUP) == GPIO_PIN_SET) {
    //{{{  usb sd MSC
    USBD_Init (&USBD_Device, &MSC_Desc, 0);
    USBD_RegisterClass (&USBD_Device, &USBD_MSC);
    USBD_MSC_RegisterStorage (&USBD_Device, (USBD_StorageTypeDef*)(&USBD_DISK_fops));
    USBD_Start (&USBD_Device);

    cLcd::debug ("USB ok");
    }
    //}}}
  else if (SD_present()) {
    //{{{  sd MP3
    mFrameOffsets = (int*)bigMalloc (60*60*40*sizeof(int), "mp3FrameOff");
    mWave = (uint8_t*)bigMalloc (60*60*40*2*sizeof(uint8_t), "mp3Wave");  // 1 hour of 40 mp3 frames per sec
    mWave[0] = 0;
    mWaveLoadFrame = 0;

    initMp3Menu (mRoot);
    mLcd->setShowDebug (false, false, false, true);  // disable debug - title, info, lcdStats, footer

    TaskHandle_t handle;
    xTaskCreate ((TaskFunction_t)mp3PlayThread, "mp3Play", 8192, 0, 3, &handle);
    }
    //}}}
  else {
    //{{{  net HLS
    mReSamples = (int16_t*)bigMalloc (4096, "hlsResamples");
    memset (mReSamples, 0, 4096);

    mLcd->setShowDebug (false, false, false, false);  // debug - title, info, lcdStats, footer

    mHls = new cHls();
    hlsMenu (mRoot, mHls);

    vSemaphoreCreateBinary (mHlsSem);

    TaskHandle_t handle;
    #ifdef ESP8266
      xTaskCreate ((TaskFunction_t)hlsLoaderThread, "hlsLoad", 14000, 0, 3, &handle);
      xTaskCreate ((TaskFunction_t)hlsPlayerThread, "hlsPlay", 2000, 0, 4, &handle);
    #else
      xTaskCreate ((TaskFunction_t)hlsNetThread, "Net", 1024, 0, 3, &handle);
    #endif
    }
    //}}}

  //{{{  init vars
  int16_t x[kMaxTouch];
  int16_t y[kMaxTouch];
  uint8_t z[kMaxTouch];
  int pressed[kMaxTouch];
  for (auto touch = 0; touch < kMaxTouch; touch++)
    pressed[touch] = 0;
  //}}}
  BSP_TS_Init (mRoot->getPixWidth(), mRoot->getPixHeight());
  while (true) {
    //bool button = true;

    bool button = BSP_PB_GetState (BUTTON_WAKEUP) == GPIO_PIN_SET;
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

    #ifdef STM32F769I_DISCO
      //{{{  update led2,3
      button ? BSP_LED_On (LED2) : BSP_LED_Off (LED2);
      tsState.touchDetected ? BSP_LED_On (LED3) : BSP_LED_Off (LED3);
      //}}}
    #endif

    mLcd->startRender();
    button ? mLcd->clear (COL_BLACK) : mRoot->render (mLcd);
    //{{{  cursor
    //if (tsState.touchDetected)
    //  mLcd->renderCursor (COL_MAGENTA, x[0], y[0], z[0] ? z[0] : cLcd::getHeight()/10);
    //}}}
    if (kSdDebug)
      mLcd->text (COL_YELLOW, cWidget::getFontHeight(), SD_info(),
                  mLcd->getLcdWidthPix()/2, mLcd->getLcdHeightPix()- cWidget::getBoxHeight(),
                  mLcd->getLcdWidthPix(), cWidget::getBoxHeight());
    mLcd->endRender (button);

    if (mHls) {
      if (mHls->mVolumeChanged && (int(mHls->mVolume * 100) != mIntVolume)) {
        //{{{  set volume
        mIntVolume = int(mHls->mVolume * 100);
        BSP_AUDIO_OUT_SetVolume (mIntVolume);
        mHls->mVolumeChanged = false;
        }
        //}}}
      }
    else {
      if (mMp3VolumeChanged && (int(mMp3Volume * 100) != mIntVolume)) {
        //{{{  set volume
        mIntVolume = int(mMp3Volume * 100);
        BSP_AUDIO_OUT_SetVolume (mIntVolume);
        mMp3VolumeChanged = false;
        }
        //}}}
      }
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
static void initDebugUart() {

  __USART1_FORCE_RESET();
  __USART1_RELEASE_RESET();

  __GPIOA_CLK_ENABLE();
  __USART1_CLK_ENABLE();

  // PA9 - USART1 tx pin configuration
  GPIO_InitTypeDef  GPIO_InitStruct;
  GPIO_InitStruct.Pin       = GPIO_PIN_9;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init (GPIOA, &GPIO_InitStruct);

  //#ifdef STM32F746G_DISCO
  // PA10 - USART1 rx pin configuration
  //GPIO_InitStruct.Pin = GPIO_PIN_10;
  //HAL_GPIO_Init (GPIOA, &GPIO_InitStruct);
  //#else
  // PB7 - USART1 rx pin configuration
  //__GPIOB_CLK_ENABLE();
  //GPIO_InitStruct.Pin = GPIO_PIN_7;
  //HAL_GPIO_Init (GPIOB, &GPIO_InitStruct);
  //#endif

  // 8 Bits, One Stop bit, Parity = None, RTS,CTS flow control
  DebugUartHandle.Instance   = USART1;
  DebugUartHandle.Init.BaudRate   = 115200;
  DebugUartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  DebugUartHandle.Init.StopBits   = UART_STOPBITS_1;
  DebugUartHandle.Init.Parity     = UART_PARITY_NONE;
  DebugUartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  DebugUartHandle.Init.Mode       = UART_MODE_TX;
  DebugUartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  HAL_UART_Init (&DebugUartHandle);

  printf ("%s\n", kHello.c_str());
  }
//}}}

//{{{
int main() {

  SCB_EnableICache();
  SCB_EnableDCache();
  HAL_Init();
  initMpuRegions();
  initClock();
  initDebugUart();

  HeapRegion_t xHeapRegions[] = { {(uint8_t*)SDRAM_HEAP, SDRAM_HEAP_SIZE }, { nullptr, 0 } };
  vPortDefineHeapRegions (xHeapRegions);

  BSP_QSPI_Init();
  BSP_QSPI_EnableMemoryMappedMode();
  QUADSPI->LPTR = 0xFFF; // Configure QSPI: LPTR register with the low-power time out value

  BSP_PB_Init (BUTTON_WAKEUP, BUTTON_MODE_GPIO);
  BSP_LED_Init (LED1);
  #ifdef STM32F769I_DISCO
    BSP_LED_Init (LED2);
    BSP_LED_Init (LED3);
  #endif

  mLcd = cLcd::create (kHello);
  mRoot = new cRootContainer (mLcd->getLcdWidthPix(), mLcd->getLcdHeightPix());

  // hard fault test
  //printf ("%x\n", *((uint32_t*)0x2FFF0000));

  vSemaphoreCreateBinary (mAudSem);

  TaskHandle_t handle;
  xTaskCreate ((TaskFunction_t)mainThread, "main", 2048, 0, 3, &handle);

  vTaskStartScheduler();

  return 0;
  }
//}}}
