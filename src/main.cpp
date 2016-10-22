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
#include "clcd.h"

#include "cRootContainer.h"
#include "cWidget.h"
#include "cListWidget.h"
#include "cValueBox.h"
#include "cWaveWidget.h"
#include "cWaveCentredWidget.h"
#include "cWaveLensWidget.h"

#include "cMp3decoder.h"

#include "../httpServer/httpServer.h"
//}}}
//{{{  static vars
static osSemaphoreId mAudSem;
static bool mAudHalf = false;

static float mVolume = 0.8f;
static bool mVolumeChanged = false;

static std::vector<std::string> mMp3Files;
static cRootContainer* mRoot = nullptr;

static int fileIndex = 0;
static bool fileIndexChanged = false;

static int mPlayFrame = 0;
static int mLoadFrame = 0;

static int* mFrameOffsets = nullptr;
static uint8_t* mWave = nullptr;
static bool mWaveChanged = false;
static bool mWaveLoad = false;

static cLcd* mLcd = nullptr;
//}}}
//{{{  audio callbacks
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
static const bool kMaxTouch = 1;
static const bool kStaticIp = true;

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
static void uiThread (void const* argument) {

  mLcd->displayOn();
  cLcd::debug ("uiThread");
  //{{{  init vars
  bool button = false;
  int16_t x[kMaxTouch];
  int16_t y[kMaxTouch];
  uint8_t z[kMaxTouch];
  int pressed[kMaxTouch];
  for (auto touch = 0; touch < kMaxTouch; touch++)
    pressed[touch] = 0;
  //}}}

  // create widgets
  mRoot->addTopLeft (new cListWidget (mMp3Files, fileIndex, fileIndexChanged, mRoot->getWidth(), mRoot->getHeight()-mRoot->getHeight()/5));
  mRoot->addTopRight (new cValueBox (mVolume, mVolumeChanged, COL_YELLOW, cWidget::getBoxHeight()-1, mRoot->getHeight()));
  //mRoot->addBottomLeft (new cWaveCentredWidget (mWave, mPlayFrame, mLoadFrame, mLoadFrame, mWaveChanged, mRoot->getWidth(), mRoot->getHeight()/5));
  mRoot->addBottomLeft (new cWaveLensWidget (mWave, mPlayFrame, mLoadFrame, mLoadFrame, mWaveChanged, mRoot->getWidth(), mRoot->getHeight()/5));

  BSP_TS_Init (mRoot->getWidth(), mRoot->getHeight());
  while (true) {
    TS_StateTypeDef tsState;
    BSP_TS_GetState (&tsState);
    button = BSP_PB_GetState(BUTTON_WAKEUP) == GPIO_PIN_SET;
    button ? BSP_LED_On (LED1) : BSP_LED_Off (LED1);
    button ? BSP_LED_On (LED3) : BSP_LED_Off (LED3);
    tsState.touchDetected ? BSP_LED_On (LED2) : BSP_LED_Off (LED2);
    for (auto touch = 0; touch < kMaxTouch; touch++) {
      if (touch < tsState.touchDetected) { //) && tsState.touchWeight[touch]) {
        auto xinc = pressed[touch] ? tsState.touchX[touch] - x[touch] : 0;
        auto yinc = pressed[touch] ? tsState.touchY[touch] - y[touch] : 0;
        x[touch] = tsState.touchX[touch];
        y[touch] = tsState.touchY[touch];
        z[touch] = tsState.touchWeight[touch];
        if (!touch)
          button ? mLcd->press (pressed[0], x[0], y[0], z[0], xinc, yinc) : mRoot->press (pressed[0], x[0], y[0], z[0], xinc, yinc);
        pressed[touch]++;
        }
      else {
        x[touch] = 0;
        y[touch] = 0;
        z[touch] = 0;
        if (!touch && pressed[0])
          mRoot->release();
        pressed[touch] = 0;
        }
      }

    mLcd->startRender();
    button ? mLcd->clear (COL_BLACK) : mRoot->render (mLcd);
    if (tsState.touchDetected)
      mLcd->renderCursor (COL_MAGENTA, x[0], y[0], z[0] ? z[0] : cLcd::getHeight()/10);
    mLcd->endRender (button);

    if (mVolumeChanged) {
      BSP_AUDIO_OUT_SetVolume (int(mVolume * 100));
      mVolumeChanged = false;
      }
    }
  }
//}}}
//{{{
static void playThread (void const* argument) {

  cLcd::debug ("playThread");

  //{{{  init sd card
  BSP_SD_Init();
  while (BSP_SD_IsDetected() != SD_PRESENT) {
    // wait for sd card loop
    cLcd::debug (COL_RED, "no SD card");
    osDelay (1000);
    }

  cLcd::debug ("SD card found");
  //}}}
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
  cLcd::debug ("play mp3Decoder ok");

  BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_SPEAKER, int(mVolume * 100), 44100);
  BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_13);

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
  auto chunkSize = 0x10000; // 64k
  auto fullChunkSize = 2048 + chunkSize;
  auto chunkBuffer = (uint8_t*)pvPortMalloc (fullChunkSize);
  cLcd::debug ("wave mp3Decoder ok");

  int loadedFileIndex = -1;
  while (true) {
    while (fileIndex == loadedFileIndex)
      osDelay (100);
    loadedFileIndex = fileIndex;

    mLoadFrame = 0;
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
                mFrameOffsets[mLoadFrame] = file.getPosition(); // not right !!!!
                auto frameBytes = mp3Decoder->decodeFrameBody (chunkPtr, mWave + 1 + (mLoadFrame * 2), nullptr);
                if (*wavePtr > *mWave)
                  *mWave = *wavePtr;
                wavePtr++;
                if (*wavePtr > *mWave)
                  *mWave = *wavePtr;
                wavePtr++;
                mLoadFrame++;

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
    }
  }
//}}}
//{{{
static void netThread (void const* argument) {

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

    httpServerInit();
    cLcd::debug ("httpServer started");
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

  // config writeThrough for SRAM1,SRAM2 0x20010000, 256k, AXI - region0
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x20010000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
  HAL_MPU_ConfigRegion (&MPU_InitStruct);

  // config writeThrough for SDRAM 0xC0000000, 8m - region1
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

  SCB_EnableICache();
  SCB_EnableDCache();
  HAL_Init();
  initMpuRegions();
  initClock();

  BSP_LED_Init (LED1);
  BSP_LED_Init (LED2);
  BSP_LED_Init (LED3);
  BSP_PB_Init (BUTTON_WAKEUP, BUTTON_MODE_GPIO);

  // init freeRTOS heap_5c
  HeapRegion_t xHeapRegions[] = { {(uint8_t*)SDRAM_HEAP, SDRAM_HEAP_SIZE }, { nullptr, 0 } };
  vPortDefineHeapRegions (xHeapRegions);

  mLcd = cLcd::create ("mp3 player built at " + std::string(__TIME__) + " on " + std::string(__DATE__));
  mRoot = new cRootContainer (cLcd::getWidth(), cLcd::getHeight());
  mFrameOffsets = (int*)pvPortMalloc (60*60*40*sizeof(int));
  mWave = (uint8_t*)pvPortMalloc (60*60*40*2*sizeof(uint8_t));  // 1 hour of 40 mp3 frames per sec
  mWave[0] = 0;

  mLoadFrame = 0;
  mPlayFrame = 0;

  osSemaphoreDef (aud);
  mAudSem = osSemaphoreCreate (osSemaphore (aud), -1);

  const osThreadDef_t osThreadUi = { (char*)"UI", uiThread, osPriorityNormal, 0, 4000 };
  osThreadCreate (&osThreadUi, NULL);

  const osThreadDef_t osThreadPlay =  { (char*)"Play", playThread, osPriorityNormal, 0, 16000 };
  osThreadCreate (&osThreadPlay, NULL);

  const osThreadDef_t osThreadWave =  { (char*)"Wave", waveThread, osPriorityBelowNormal, 0, 16000 };
  osThreadCreate (&osThreadWave, NULL);

  const osThreadDef_t osThreadNet =  { (char*)"Net", netThread, osPriorityBelowNormal, 0, 1024 };
  osThreadCreate (&osThreadNet, NULL);

  osKernelStart();

  return 0;
  }
//}}}
