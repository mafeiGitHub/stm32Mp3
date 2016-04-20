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

#include "../Bsp/ethernetif.h"

#include "../Bsp/stm32746g_discovery.h"
#include "../Bsp/stm32746g_discovery_ts.h"
#include "../Bsp/stm32746g_discovery_audio.h"
#include "../Bsp/cLcd.h"

#include "../widgets/cWidgetMan.h"
#include "../widgets/cWidget.h"
#include "../widgets/cTextBox.h"
#include "../widgets/cValueBox.h"

#include "../Bsp/stm32746g_discovery_sd.h"
#include "../fatfs/fatFs.h"

#include "../httpServer/httpServer.h"

#include "cMp3decoder.h"

using namespace std;
//}}}
//{{{  static const
static bool kStaticIp = true;
//}}}
//{{{  static vars
static struct netif gNetif;

static float mVolume = 0.8f;
static int mPlayFrame = 0;

static cMp3Decoder* mMp3Decoder = nullptr;
static float mPower [480*2];

static osSemaphoreId audSem;
static bool mAudHalf = false;

static cValueBox* mProgressBox = nullptr;
static vector <string> mMp3Files;
static string mPlayFileName;
//}}}
//{{{  audio callbacks
//{{{
void BSP_AUDIO_OUT_HalfTransfer_CallBack() {
  mAudHalf = true;
  osSemaphoreRelease (audSem);
  }
//}}}
//{{{
void BSP_AUDIO_OUT_TransferComplete_CallBack() {
  mAudHalf = false;
  osSemaphoreRelease (audSem);
  }
//}}}
//}}}

//{{{
class cVolumeBox : public cValueBox {
public:
  cVolumeBox (float value, uint32_t colour, uint16_t xlen, uint16_t ylen) :
    cValueBox (value, colour, xlen, ylen){}
  virtual ~cVolumeBox() {}

  virtual void setValue (float value) {
    if (cValueBox::setValue (value, 0.0f, 1.0f)) {
      mVolume = getValue() * 100;
      BSP_AUDIO_OUT_SetVolume (mVolume);
      }
    }
  };
//}}}
//{{{
class cFileNameBox : public cTextBox {
public:
  cFileNameBox (string text) :
    cTextBox (text, cLcd::getWidth() - 20 - 1) {}
  virtual ~cFileNameBox() {}

  virtual void released (int16_t x, int16_t y) {
    mPlayFileName = mText;
    cTextBox::released (x, y);
    }
  };
//}}}

//{{{
static void listDirectory (string directoryName, string indent) {

  cLcd::debug ("dir " + directoryName);

  cDirectory directory (directoryName);
  if (!directory.isOk()) {
    //{{{  open error
    cLcd::debug (LCD_RED, "directory open error:"  + cLcd::intStr (directory.getResult()));
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
      cLcd::instance()->setShowDebug (false, false, false, false);
      mMp3Files.push_back (directoryName + "/" + fileInfo.getName());
      cLcd::debug (indent + fileInfo.getName());
      cFile file (directoryName + "/" + fileInfo.getName(), FA_OPEN_EXISTING | FA_READ);
      cLcd::debug ("- filesize " + cLcd::intStr (file.getSize()));
      }
    }
  }
//}}}
//{{{
static void playFile (string fileName) {

  cLcd::instance()->setTitle (fileName);

  cFile file (fileName, FA_OPEN_EXISTING | FA_READ);
  if (!file.isOk()) {
    //{{{  error, return
    cLcd::debug ("- open failed " + cLcd::intStr (file.getResult()) + " " + fileName);
    return;
    }
    //}}}
  cLcd::debug ("play " + fileName + " " + cLcd::intStr (file.getSize()));

  //{{{  chunkSize and buffer
  int chunkSize = 4096;
  int fullChunkSize = 2048 + chunkSize;

  auto chunkBuffer = (uint8_t*)pvPortMalloc (fullChunkSize);
  //}}}
  //{{{  bsp play
  for (auto i = 0; i < 480*2; i++)
    mPower[i] = 0;
  memset ((void*)AUDIO_BUFFER, 0, AUDIO_BUFFER_SIZE);
  BSP_AUDIO_OUT_Play ((uint16_t*)AUDIO_BUFFER, AUDIO_BUFFER_SIZE);
  //}}}

  mPlayFrame = 0;
  int bytesLeft;
  do {
    file.read (chunkBuffer, fullChunkSize, bytesLeft);
    if (bytesLeft) {
      auto chunkPtr = chunkBuffer;
      int headerBytes;
      do {
        headerBytes = mMp3Decoder->findNextHeader (chunkPtr, bytesLeft);
        if (headerBytes) {
          chunkPtr += headerBytes;
          bytesLeft -= headerBytes;
          if (bytesLeft < mMp3Decoder->getFrameBodySize() + 4) { // not enough for frameBody and next header
            //{{{  move bytesLeft to front of chunkBuffer,  next read partial buffer 32bit aligned
            auto nextChunkPtr = chunkBuffer + ((4 - (bytesLeft & 3)) & 3);
            memcpy (nextChunkPtr, chunkPtr, bytesLeft);

            // read next chunks worth, including rest of frame, 32bit aligned
            int bytesLoaded;
            file.read (nextChunkPtr + bytesLeft, chunkSize, bytesLoaded);
            if (bytesLoaded) {
              chunkPtr = nextChunkPtr;
              bytesLeft += bytesLoaded;
              }
            else
              bytesLeft = 0;
            }
            //}}}
          if (bytesLeft >= mMp3Decoder->getFrameBodySize()) {
            osSemaphoreWait (audSem, 100);
            auto frameBytes = mMp3Decoder->decodeFrameBody (chunkPtr, &mPower[(mPlayFrame % 480) * 2], (int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER_HALF));
            if (frameBytes) {
              chunkPtr += frameBytes;
              bytesLeft -= frameBytes;
              mPlayFrame++;
              }
            else
              bytesLeft = 0;
            }
          }
        if (mPlayFileName != fileName)
          bytesLeft = 0;
        else if (mProgressBox->getPressed() == 1) {
          //{{{  skip
          if (mProgressBox->getValue() < 99.0f)
            file.seek (int(mProgressBox->getValue() * file.getSize()) & 0xFFFFFFE0);
          else
            bytesLeft = 0;

          headerBytes = 0;
          }
          //}}}
        else
          mProgressBox->setValue ((float)file.getPosition() / (float)file.getSize());
        } while (headerBytes && (bytesLeft > 0));
      }
    } while (bytesLeft > 0);

  vPortFree (chunkBuffer);
  BSP_AUDIO_OUT_Stop (CODEC_PDWN_SW);
  }
//}}}

// threads
//{{{
static void uiThread (void const* argument) {

  auto lcd = cLcd::instance();
  cLcd::debug ("uiThread started");

  //{{{  init touch
  BSP_TS_Init (cLcd::getWidth(), cLcd::getHeight());
  int pressed[5] = {0, 0, 0, 0, 0};
  int x[5];
  int y[5];
  int z[5];
  //}}}
  while (true) {
    //{{{  read touch and use it
    TS_StateTypeDef tsState;
    BSP_TS_GetState (&tsState);

    for (auto touch = 0; touch < 5; touch++) {
      if (touch < tsState.touchDetected) { //) && tsState.touchWeight[touch]) {
        auto xinc = pressed[touch] ? tsState.touchX[touch] - x[touch] : 0;
        auto yinc = pressed[touch] ? tsState.touchY[touch] - y[touch] : 0;
        x[touch] = tsState.touchX[touch];
        y[touch] = tsState.touchY[touch];
        z[touch] = tsState.touchWeight[touch];
        if (!touch)
          if (!cWidgetMan::instance()->press (pressed[0], x[0], y[0], z[0], xinc, yinc))
            lcd->press (pressed[0], x[0], y[0], z[0], xinc, yinc);
        pressed[touch]++;
        }
      else {
        x[touch] = 0;
        y[touch] = 0;
        z[touch] = 0;
        if (!touch && pressed[0])
          cWidgetMan::instance()->release();
        pressed[touch] = 0;
        }
      }
    //}}}

    lcd->startDraw();
    cWidgetMan::instance()->draw (lcd);
    //{{{  draw blue waveform
    for (auto x = 0; x < cLcd::getWidth(); x++) {
      int frame = mPlayFrame - cLcd::getWidth() + x;
      if (frame > 0) {
        auto index = (frame % 480) * 2;
        uint8_t top = (cLcd::getHeight()/2) - (int)mPower[index]/2;
        uint8_t ylen = (cLcd::getHeight()/2) + (int)mPower[index+1]/2 - top;
        lcd->rectClipped (LCD_BLUE, x, top, 1, ylen);
        }
      }
    //}}}
    //{{{  draw touch
    //for (auto touch = 0; (touch < 5) && pressed[touch]; touch++)
      //lcd->drawCursor (touch > 0 ? LCD_LIGHTGREY : LCD_YELLOW, x[touch], y[touch], z[touch]);
    //}}}
    lcd->endDraw();
    }

  }
//}}}
//{{{
static void loadThread (void const* argument) {

  cLcd::debug ("loadThread started");

  BSP_SD_Init();
  while (BSP_SD_IsDetected() != SD_PRESENT) {
    //{{{  wait for sd card loop
    cLcd::debug (LCD_RED, "no SD card");
    osDelay (1000);
    }
    //}}}
  cLcd::debug ("SD card found");

  cFatFs* fatFs = cFatFs::create();
  if (fatFs->mount() != FR_OK) {
    //{{{  mount error
    cLcd::debug ("fatFs mount problem");
    osThreadTerminate (NULL);
    return;
    }
    //}}}

  cLcd::debug (fatFs->getLabel() +
               " vsn:" + cLcd::hexStr (fatFs->getVolumeSerialNumber()) +
               " freeSectors:" + cLcd::intStr (fatFs->getFreeSectors()));

  listDirectory ("", "");
  for (auto fileName : mMp3Files)
    if (!cWidgetMan::instance()->addBelow (new cFileNameBox (fileName)))
      break;
  cWidgetMan::instance()->add (mProgressBox = new cValueBox (0.0f, LCD_BLUE, cLcd::getWidth(), 6), 0, cLcd::getHeight()-6);
  cWidgetMan::instance()->add (new cVolumeBox (mVolume, LCD_YELLOW, 20, cLcd::getHeight()), cLcd::getWidth()-20, 0);

  mMp3Decoder = new cMp3Decoder;
  cLcd::debug ("mp3Decoder created");

  osSemaphoreDef (aud);
  audSem = osSemaphoreCreate (osSemaphore (aud), -1);
  BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_SPEAKER, int(mVolume * 100), 44100);  // OUTPUT_DEVICE_HEADPHONE
  BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_13);             // CODEC_AUDIOFRAME_SLOT_02

  while (true) {
    while (mPlayFileName.empty())
      osDelay (100);
    playFile (mPlayFileName);
    }
  }
//}}}
//{{{
static void dhcpThread (void const* argument) {

  auto netif = (struct netif*)argument;

  tcpip_init (NULL, NULL);
  cLcd::debug ("configuring ethernet");

  // init LwIP stack
  struct ip_addr ipAddr;
  IP4_ADDR (&ipAddr, 192, 168, 1, 67);
  struct ip_addr netmask;
  IP4_ADDR (&netmask, 255, 255 , 255, 0);
  struct ip_addr gateway;
  IP4_ADDR (&gateway, 192, 168, 0, 1);
  netif_add (netif, &ipAddr, &netmask, &gateway, NULL, &ethernetif_init, &tcpip_input);
  netif_set_default (netif);

  if (netif_is_link_up (netif)) {
    netif_set_up (netif);
    if (kStaticIp)
      //{{{  static ip
      cLcd::debug (LCD_YELLOW, "ethernet static ip " + cLcd::intStr ((int) (netif->ip_addr.addr & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netif->ip_addr.addr >> 16) & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netif->ip_addr.addr >> 8) & 0xFF)) + "." +
                                                       cLcd::intStr ((int) (netif->ip_addr.addr >> 24)));
      //}}}
    else {
      //{{{  dhcp ip
      struct ip_addr nullIpAddr;
      IP4_ADDR (&nullIpAddr, 0, 0, 0, 0);
      netif->ip_addr = nullIpAddr;
      netif->netmask = nullIpAddr;
      netif->gw = nullIpAddr;
      dhcp_start (netif);

      while (true) {
        if (netif->ip_addr.addr) {
          //{{{  dhcp allocated
          cLcd::debug (LCD_YELLOW, "dhcp allocated " + cLcd::intStr ( (int)(netif->ip_addr.addr & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netif->ip_addr.addr >> 16) & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netif->ip_addr.addr >> 8) & 0xFF)) + "." +
                                                       cLcd::intStr ( (int)(netif->ip_addr.addr >> 24)));
          dhcp_stop (netif);
          break;
          }
          //}}}
        else if (netif->dhcp->tries > 4) {
          //{{{  dhcp timeout
          cLcd::debug (LCD_RED, "dhcp timeout");
          dhcp_stop (netif);

          // use static address
          struct ip_addr ipAddr;
          IP4_ADDR (&ipAddr, 192 ,168 , 1 , 67 );
          struct ip_addr netmask;
          IP4_ADDR (&netmask, 255, 255, 255, 0);
          struct ip_addr gateway;
          IP4_ADDR (&gateway, 192, 168, 0, 1);
          netif_set_addr (netif, &ipAddr , &netmask, &gateway);
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
    netif_set_down (&gNetif);
    cLcd::debug (LCD_RED, "no ethernet");
    }
    //}}}

  osThreadTerminate (NULL);
  }
//}}}
//{{{
static void startThread (void const* argument) {

  cLcd::create ("mp3 player built at " + string(__TIME__) + " on " + string(__DATE__));
  cWidgetMan::create();

  const osThreadDef_t osThreadUi = { (char*)"UI", uiThread, osPriorityNormal, 0, 2000 };
  osThreadCreate (&osThreadUi, NULL);

  const osThreadDef_t osThreadLoad =  { (char*)"Load", loadThread, osPriorityNormal, 0, 10000 };
  osThreadCreate (&osThreadLoad, NULL);

  const osThreadDef_t osThreadDHCP =  { (char*)"DHCP", dhcpThread, osPriorityBelowNormal, 0, 1024 };
  osThreadCreate (&osThreadDHCP, &gNetif);

  for (;;)
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
  MPU_InitStruct.Size = MPU_REGION_SIZE_8MB;
  HAL_MPU_ConfigRegion (&MPU_InitStruct);

  HAL_MPU_Enable (MPU_PRIVILEGED_DEFAULT);
  }
//}}}
//{{{
static void initClock() {
//  System Clock source            = PLL (HSE)
//  SYSCLK(Hz)                     = 216000000
//  HCLK(Hz)                       = 216000000
//  HSE Frequency(Hz)              = 25000000
//  PLL_M                          = 25
//  PLL_N                          = 432
//  PLL_P                          = 2
//  PLL_Q                          = 9
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

  // init freeRTOS heap_5c
  HeapRegion_t xHeapRegions[] = { {(uint8_t*)SDRAM_HEAP, SDRAM_HEAP_SIZE }, { nullptr, 0 } };
  vPortDefineHeapRegions (xHeapRegions);

  // launch startThread
  const osThreadDef_t osThreadStart = { (char*)"Start", startThread, osPriorityNormal, 0, 4000 };
  osThreadCreate (&osThreadStart, NULL);

  osKernelStart();

  return 0;
  }
//}}}
