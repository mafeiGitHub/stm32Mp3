// main.cpp
//{{{  includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

#include "cmsis_os.h"

#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "ipv4/lwip/ip_addr.h"
#include "lwip/api.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_sai.h"

#include "../Bsp/ethernetif.h"
#include "../Bsp/stm32746g_discovery.h"
#include "../Bsp/stm32746g_discovery_audio.h"
#include "../Bsp/stm32746g_discovery_ts.h"

#include "../Bsp/cLcd.h"

#include "../Bsp/stm32746g_discovery_sd.h"
#include "../fatfs/ff.h"

#include "../httpServer/httpServer.h"

#include "cMp3decoder.h"
//}}}
//{{{  sdram allocation
#define SDRAM_FRAME0    0xC0000000 // frameBuffer 272*480*4 = 0x7f800 = 512k-2048b leave bit of guard for clipping errors
#define SDRAM_FRAME1    0xC0080000

#define SDRAM_HEAP      0xC0100000 // sdram heap
#define SDRAM_HEAP_SIZE 0x00700000 // 7m
//}}}
//{{{  static vars
static struct netif gNetif;
static osSemaphoreId dhcpSem;

static cLcd mLcd;
static float mVolume = 0.7f;

static int mPlayFrame = 0;
static int mPlayBytes = 0;
static int mPlaySize = 0;
static bool mSkip = false;

static cMp3Decoder* mp3Decoder = nullptr;
static float mPower [480*2];

static osSemaphoreId audSem;
static bool audBufHalf = false;
static int16_t* audBuf = nullptr;
//}}}

//{{{
void BSP_AUDIO_OUT_HalfTransfer_CallBack() {
  audBufHalf = true;
  osSemaphoreRelease (audSem);
  }
//}}}
//{{{
void BSP_AUDIO_OUT_TransferComplete_CallBack() {
  audBufHalf = false;
  osSemaphoreRelease (audSem);
  }
//}}}

//{{{
static void playFile (std::string fileName) {

  mLcd.setTitle (fileName);
  mLcd.text ("playing " + fileName);

  mPlayBytes = 0;
  mPlaySize = 0;
  for (auto i = 0; i < 480*2; i++)
    mPower[i] = 0;

  FIL file;
  auto result = f_open (&file, fileName.c_str(), FA_OPEN_EXISTING | FA_READ);
  if (result != FR_OK) {
    mLcd.text ("- load failed " + mLcd.intStr (result));
    return;
    }
  mPlaySize = f_size (&file);

  // load file into fileBuffer, limit to 5m for now
  int size = mPlaySize;
  if (size > 0x00500000) // 5m
    size = 0x00500000;
  auto fileBuffer = (unsigned char*)pvPortMalloc (size);
  unsigned int bytesRead = 0;
  result = f_read (&file, fileBuffer, size, &bytesRead);
  if (result != FR_OK) {
    mLcd.text ("- read failed " + mLcd.intStr (result));
    f_close (&file);
    return;
    }
  f_close (&file);

  // play file from fileBuffer
  mLcd.text ("- loaded " + mLcd.intStr (bytesRead) + " of " +  mLcd.intStr (mPlaySize));

  memset (audBuf, 0, 1152*8);
  BSP_AUDIO_OUT_Play ((uint16_t*)audBuf, 1152*8);

  auto ptr = fileBuffer;
  mPlayFrame = 0;
  while (size > 0 && !mSkip) {
    if (osSemaphoreWait (audSem, 50) == osOK) {
      auto bytesUsed = mp3Decoder->decodeFrame (ptr, size, &mPower[(mPlayFrame % 480) * 2], audBufHalf ? audBuf : audBuf + (1152*2));
      if (bytesUsed > 0) {
        ptr += bytesUsed;
        mPlayBytes = int(ptr - fileBuffer);
        size -= bytesUsed;
        }
      else
        break;
      mPlayFrame++;
      }
    }
  if (mSkip) {
    mLcd.text ("- skipped at " + mLcd.intStr (mPlayBytes) + " of " +  mLcd.intStr (mPlaySize));
    mSkip = false;
    }

  vPortFree (fileBuffer);

  BSP_AUDIO_OUT_Stop (CODEC_PDWN_SW);
  }
//}}}

// threads
//{{{
static void uiThread (void const* argument) {

  mLcd.text ("uiThread started");

  const int kInfo = 150;
  const int kVolume = 440;

  // init touch
  BSP_TS_Init (mLcd.getWidth(), mLcd.getHeight());
  int pressed [5] = {0, 0, 0, 0, 0};
  int lastx [5];
  int lasty [5];
  int lastz [5];

  while (true) {
    //{{{  touch
    TS_StateTypeDef tsState;
    BSP_TS_GetState (&tsState);

    mSkip = false;

    for (auto touch = 0; touch < 5; touch++) {
      if ((touch < tsState.touchDetected) && tsState.touchWeight[touch]) {
        auto x = tsState.touchX[touch];
        auto y = tsState.touchY[touch];
        auto z = tsState.touchWeight[touch];

        if (touch == 0) {
          if (x < kInfo)
            //{{{  pressed mLcd info
            mLcd.pressed (pressed[touch], x, y, pressed[touch] ? x - lastx[touch] : 0, pressed[touch] ? y - lasty[touch] : 0);
            //}}}
          else if (x < kVolume) {
            //{{{  pressed middle
            //mLcd.text (mLcd.intStr (x) + "," + mLcd.intStr (y) + "," + mLcd.intStr (tsState.touchWeight[touch]));
            if (y < 20)
              mSkip = true;
            }
            //}}}
          else {
             //{{{  pressed volume
             auto volume = pressed[touch] ? mVolume + float(y - lasty[touch]) / mLcd.getHeight(): float(y) / mLcd.getHeight();

             if (volume < 0)
               volume = 0;
             else if (volume > 1.0f)
               volume = 1.0f;

             if (volume != mVolume) {
               mVolume = volume;
               BSP_AUDIO_OUT_SetVolume (int(mVolume * 100));
               }
             }
             //}}}
          }

        lastx[touch] = x;
        lasty[touch] = y;
        lastz[touch] = z;
        pressed[touch]++;
        }
      else
        pressed[touch] = 0;
      }
    //}}}

    mLcd.startDraw();
    //{{{  draw cursors
    auto drawTouch = 0;
    while ((drawTouch < 5) && pressed[drawTouch]) {
      mLcd.ellipse (drawTouch > 0 ? LCD_LIGHTGREY : lastx[0] < kInfo ? LCD_GREEN : lastx[0] < kVolume ? LCD_MAGENTA : LCD_YELLOW,
                    lastx[drawTouch], lasty[drawTouch], lastz[drawTouch], lastz[drawTouch]);
      drawTouch++;
      }
    //}}}
    //{{{  draw yellow volume
    mLcd.rect (LCD_YELLOW, mLcd.getWidth()-20, 0, 20, int(mVolume * mLcd.getHeight()));
    //}}}
    //{{{  draw blue play progress
    mLcd.rect (LCD_BLUE, 0, 0, (mPlayBytes * mLcd.getWidth()) / mPlaySize, 2);
    //}}}
    //{{{  draw blue waveform
    for (auto x = 0; x < mLcd.getWidth(); x++) {
      int frame = mPlayFrame - mLcd.getWidth() + x;
      if (frame > 0) {
        auto index = (frame % 480) * 2;
        uint8_t top = (mLcd.getHeight()/2) - (int)mPower[index]/2;
        uint8_t ylen = (mLcd.getHeight()/2) + (int)mPower[index+1]/2 - top;
        mLcd.rectClipped (LCD_BLUE, x, top, 1, ylen);
        }
      }
    //}}}
    mLcd.endDraw();
    }

  }
//}}}
//{{{
static void loadThread (void const* argument) {

  mLcd.text ("loadThread started");

  audBuf = (int16_t*)malloc (1152*8);
  memset (audBuf, 0, 1152*8);
  mLcd.text ("audioBuf allocated " + mLcd.hexStr ((int)audBuf));

  BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_BOTH, int(mVolume * 100), 44100);
  BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);

  mp3Decoder = new cMp3Decoder;
  mLcd.text ("Mp3 decoder allocated:" + mLcd.hexStr ((int)mp3Decoder, 8));

  BSP_SD_Init();
  while (BSP_SD_IsDetected() != SD_PRESENT) {
    mLcd.text (LCD_RED, "no SD card");
    osDelay (1000);
    }
  mLcd.text ("SD card found");

  FRESULT result = f_mount ((FATFS*)0x20000000, "", 0);
  if (result != FR_OK)
    mLcd.text ("FAT fileSystem mount error:" + mLcd.intStr (result));
  else {
    mLcd.text ("FAT fileSystem mounted");

    DIR dir;
    result = f_opendir (&dir, "/");
    if (result != FR_OK)
      mLcd.text (LCD_RED, "directory open error:"  + mLcd.intStr (result));
    else {
      mLcd.text ("directory opened - looking for mp3");

      FILINFO filInfo;
      filInfo.lfname = (char*)malloc (_MAX_LFN + 1);
      filInfo.lfsize = _MAX_LFN + 1;
      auto extension = "MP3";
      while (true) {
        if ((f_readdir (&dir, &filInfo) != FR_OK) || filInfo.fname[0] == 0)
          break;
        if (filInfo.fname[0] == '.')
          continue;
        if (!(filInfo.fattrib & AM_DIR)) {
          auto i = 0;
          while (filInfo.fname[i++] != '.') {}
          if ((filInfo.fname[i] == extension[0]) && (filInfo.fname[i+1] == extension[1]) && (filInfo.fname[i+2] == extension[2]))
            playFile (filInfo.lfname[0] ? (char*)filInfo.lfname : (char*)&filInfo.fname);
          }
        }
      }
    }

  int tick = 0;
  while (true) {
    mLcd.text ("load tick" + mLcd.intStr (tick++));
    osDelay (10000);
    }
  }
//}}}
//{{{
static void dhcpThread (void const* argument) {

  mLcd.text ("dhcpThread started");
  auto netif = (struct netif*)argument;

  struct ip_addr nullIpAddr;
  IP4_ADDR (&nullIpAddr, 0, 0, 0, 0);
  netif->ip_addr = nullIpAddr;
  netif->netmask = nullIpAddr;
  netif->gw = nullIpAddr;
  dhcp_start (netif);

  while (true) {
    if (netif->ip_addr.addr) {
      mLcd.text (LCD_YELLOW, "dhcp allocated " + mLcd.intStr ((int)(netif->ip_addr.addr & 0xFF)) + "." +
                                                 mLcd.intStr ((int)((netif->ip_addr.addr >> 16) & 0xFF)) + "." +
                                                 mLcd.intStr ((int)((netif->ip_addr.addr >> 8) & 0xFF)) + "." +
                                                 mLcd.intStr ((int)(netif->ip_addr.addr >> 24)));
      dhcp_stop (netif);
      osSemaphoreRelease (dhcpSem);
      break;
      }
    else if (netif->dhcp->tries > 4) {
      mLcd.text (LCD_RED, "dhcp timeout");
      dhcp_stop (netif);

      // use static address
      struct ip_addr ipAddr;
      IP4_ADDR (&ipAddr, 192 ,168 , 1 , 67 );
      struct ip_addr netmask;
      IP4_ADDR (&netmask, 255, 255, 255, 0);
      struct ip_addr gateway;
      IP4_ADDR (&gateway, 192, 168, 0, 1);
      netif_set_addr (netif, &ipAddr , &netmask, &gateway);
      osSemaphoreRelease (dhcpSem);
      break;
      }
    osDelay (250);
    }

  osThreadTerminate (NULL);
  }
//}}}
//{{{
static void startThread (void const* argument) {

  mLcd.init (SDRAM_FRAME0, SDRAM_FRAME1);
  mLcd.setTitle (__TIME__ __DATE__);
  mLcd.text ("startThread started");

  const osThreadDef_t osThreadUi = { (char*)"UI", uiThread, osPriorityNormal, 0, 2000 };
  osThreadCreate (&osThreadUi, NULL);

  const osThreadDef_t osThreadLoad =  { (char*)"Load", loadThread, osPriorityNormal, 0, 10000 };
  osThreadCreate (&osThreadLoad, NULL);

  if (true) {
    tcpip_init (NULL, NULL);
    mLcd.text ("configuring ethernet");

    // init LwIP stack
    struct ip_addr ipAddr;
    IP4_ADDR (&ipAddr, 192, 168, 1, 67);
    struct ip_addr netmask;
    IP4_ADDR (&netmask, 255, 255 , 255, 0);
    struct ip_addr gateway;
    IP4_ADDR (&gateway, 192, 168, 0, 1);
    netif_add (&gNetif, &ipAddr, &netmask, &gateway, NULL, &ethernetif_init, &tcpip_input);
    netif_set_default (&gNetif);

    if (netif_is_link_up (&gNetif)) {
      netif_set_up (&gNetif);
      mLcd.text ("ethernet connected");

      const osThreadDef_t osThreadDHCP =  { (char*)"DHCP", dhcpThread, osPriorityBelowNormal, 0, 1024 };
      osThreadCreate (&osThreadDHCP, &gNetif);
      while (osSemaphoreWait (dhcpSem, 1000) == osOK)
        osDelay (1000);

      httpServerInit();
      mLcd.text ("httpServer started");
      }
    else {
      //{{{  no ethernet
      netif_set_down (&gNetif);
      mLcd.text (LCD_RED, "no ethernet");
      }
      //}}}
    }

  for (;;)
    osThreadTerminate (NULL);
  }
//}}}

// init
//{{{
static void MPUconfigSRAM() {
// config MPU attributes as WriteThrough for SRAM
// base Address is 0x20010000, memory interface is the AXI region, size 256KB related to SRAM1 and SRAM2 memory size

  HAL_MPU_Disable();

  MPU_Region_InitTypeDef MPU_InitStruct;
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x20010000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  HAL_MPU_ConfigRegion (&MPU_InitStruct);

  HAL_MPU_Enable (MPU_PRIVILEGED_DEFAULT);
  }
//}}}
//{{{
static void MPUconfigSDRAM() {
// Configure the MPU attributes as writeThrough for SDRAM

  HAL_MPU_Disable();

  MPU_Region_InitTypeDef MPU_InitStruct;
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_8MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  HAL_MPU_ConfigRegion (&MPU_InitStruct);

  HAL_MPU_Enable (MPU_PRIVILEGED_DEFAULT);
  }
//}}}
//{{{
static void initSystemClock216() {
// Enable HSE Oscillator and activate PLL with HSE as source
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

  // init cpu caches, clocks, memory regions
  SCB_EnableICache();
  SCB_EnableDCache();
  HAL_Init();

  MPUconfigSRAM();
  MPUconfigSDRAM();
  initSystemClock216();

  // init heap
  HeapRegion_t xHeapRegions[] = { {(uint8_t*)SDRAM_HEAP, SDRAM_HEAP_SIZE }, { NULL, 0 } };
  vPortDefineHeapRegions (xHeapRegions);

  // init semaphores
  osSemaphoreDef (dhcp);
  dhcpSem = osSemaphoreCreate (osSemaphore (dhcp), -1);

  osSemaphoreDef (aud);
  audSem = osSemaphoreCreate (osSemaphore (aud), -1);

  const osThreadDef_t osThreadStart = { (char*)"Start", startThread, osPriorityNormal, 0, 4000 };
  osThreadCreate (&osThreadStart, NULL);

  osKernelStart();

  return 0;
  }
//}}}
