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

#include "../fatfs/ff.h"
#include "../fatfs/ff_gen_drv.h"
#include "../fatfs/sd_diskio.h"

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
static osSemaphoreId audioSem;

static cLcd mLcd;
static float mVolume = 0.8f;
//}}}
const bool kLcdInterrupts = false;
FATFS fatFs;
char SD_Path[4];

//{{{
void BSP_AUDIO_OUT_HalfTransfer_CallBack() {
  osSemaphoreRelease (audioSem);
  }
//}}}
//{{{
void BSP_AUDIO_OUT_TransferComplete_CallBack() {
  osSemaphoreRelease (audioSem);
  }
//}}}

//{{{
static void loadFile (std::string fileName) {

  mLcd.text ("loadFile opening " + fileName);
  auto buffer = (unsigned char*)pvPortMalloc (4096);
  if ((((uint32_t)buffer) & 0x3) != 0) {
    mLcd.text ("alignment");
    mLcd.drawText();
    }

  FIL file;
  int result = f_open (&file, fileName.c_str(), FA_OPEN_EXISTING | FA_READ);
  if (result == FR_OK) {
    mLcd.text ("- opened - size " + mLcd.intStr (f_size (&file)) + " " + mLcd.hexStr ((int)buffer, 8));
    unsigned int bytesRead = 0;
    unsigned int bytes = 4096;
    while (bytes > 0) {
      result = f_read (&file, buffer, 4096, &bytes);
      bytesRead += bytes;
      //mLcd.text ("- read " + mLcd.intStr (bytesRead) + " " +  mLcd.intStr (bytes), false);
      //mLcd.drawText();
      }
    mLcd.text ("- read " + mLcd.intStr (bytesRead));
    mLcd.drawText();
    f_close (&file);
    }
  else {
    mLcd.text ("- open failed - result:" + mLcd.intStr (result));
    mLcd.drawText();
    }

  vPortFree (buffer);
  }
//}}}

// threads
//{{{
static void uiThread (void const* argument) {

  const int kInfo = 150;
  const int kVolume = 440;

  mLcd.text ("uiThread started");

  int pressed [5] = {0, 0, 0, 0, 0};
  int lastx [5];
  int lasty [5];

  BSP_TS_Init (mLcd.getWidth(), mLcd.getHeight());
  while (true) {
    TS_StateTypeDef tsState;
    BSP_TS_GetState (&tsState);

    mLcd.startDraw();

    //  touchscreen
    for (auto touch = 0; touch < 5; touch++) {
      if ((touch < tsState.touchDetected) && tsState.touchWeight[touch]) {
        auto x = tsState.touchX[touch];
        auto y = tsState.touchY[touch];
        mLcd.ellipse (touch > 0 ? LCD_LIGHTGREY : x < kInfo ? LCD_GREEN : x < kVolume ? LCD_MAGENTA : LCD_YELLOW,
                      x, y, tsState.touchWeight[touch], tsState.touchWeight[touch]);
        if (touch == 0) {
          if (x < kInfo)
            mLcd.pressed (pressed[touch], x, y, pressed[touch] ? x - lastx[touch] : 0, pressed[touch] ? y - lasty[touch] : 0);
          else if (x < kVolume)
            mLcd.text (mLcd.intStr (x) + "," + mLcd.intStr (y) + "," + mLcd.intStr (tsState.touchWeight[touch]));
          else {
            //{{{  adjust volume
            auto volume = pressed[touch] ? mVolume + float(y - lasty[touch]) / mLcd.getHeight(): float(y) / mLcd.getHeight();

            if (volume < 0)
              volume = 0;
            else if (volume > 1.0f)
              volume = 1.0f;

            if (volume != mVolume) {
              mLcd.text ("vol " + mLcd.intStr(int(volume*100)) + "%");
              mVolume = volume;
              }
            }
            //}}}
          }
        lastx[touch] = x;
        lasty[touch] = y;
        pressed[touch]++;
        }
      else
        pressed[touch] = 0;
      }

    //{{{  volume bar
    mLcd.rect (LCD_YELLOW, mLcd.getWidth()-20, 0, 20, int(mVolume * mLcd.getHeight()));
    //}}}
    //{{{  waveform
    //int frames = 0;
    //uint8_t* power = nullptr;
    //int frame = 0 - lcdGetXSize()/2;
    //for (auto x = 0; x < lcdGetXSize(); x++, frame++) {
      //if (frames <= 0)
        //power = nullptr;
      //if (power) {
        //uint8_t top = *power++;
        //uint8_t ylen = *power++;
        //lcdRect (LCD_BLUE, x, top, 1, ylen);
        //frames--;
        //}
      //}
    //}}}

    mLcd.endDraw();
    if (!kLcdInterrupts)
      osDelay (100);
    }
  }
//}}}
//{{{
static void loadThread (void const* argument) {

  mLcd.text ("sd started");
  mLcd.drawText();
  BSP_SD_Init();
  while (BSP_SD_IsDetected() != SD_PRESENT) {
    mLcd.text (LCD_RED, "no SD card");
    mLcd.drawText();
    osDelay (1000);
    }
  mLcd.text ("SD card found");
  mLcd.drawText();

  //cMp3Decoder* mp3Decoder = new cMp3Decoder;
  //mLcd.text ("Mp3 decoder " + mLcd.hexStr ((int)mp3Decoder, 8));

  if (FATFS_LinkDriver (&SD_Driver, SD_Path) != 0) {
    mLcd.text (LCD_RED, "SD driver error");
    mLcd.drawText();
    }
  else {
    mLcd.text ("SD driver found");
    mLcd.drawText();

    f_mount (&fatFs, "", 0);
    mLcd.text ("FAT fileSystem mounted");
    mLcd.drawText();

    DIR dir;
    if (f_opendir (&dir, "/") != FR_OK) {
      mLcd.text (LCD_RED, "directory open error");
      mLcd.drawText();
      }
    else {
      mLcd.text ("directory opened");
      mLcd.drawText();

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
          while (filInfo.fname[i++] != '.') {;}
          if ((filInfo.fname[i] == extension[0]) &&
              (filInfo.fname[i+1] == extension[1]) &&
              (filInfo.fname[i+2] == extension[2])) {
            mLcd.text (filInfo.lfname[0] ? (char*)filInfo.lfname : (char*)&filInfo.fname);
            mLcd.drawText();
            loadFile (filInfo.lfname[0] ? (char*)filInfo.lfname : (char*)&filInfo.fname);
            }
          }
        }
      }
    }

  mLcd.text ("loadThread started");
  mLcd.drawText();

  int tick = 0;
  while (true) {
    mLcd.text ("load tick" + mLcd.intStr (tick++));
    mLcd.drawText();
    osDelay (2000);
    }
  }
//}}}
//{{{
static void playThread (void const* argument) {

  mLcd.text ("playThread started");

  int curVol = 80;
  BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_BOTH, curVol, 48000);
  BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);

  bool audioBufferFull = false;
  int16_t* audioBuf = (int16_t*)malloc (8192);
  memset (audioBuf, 0, 8192);
  BSP_AUDIO_OUT_Play ((uint16_t*)audioBuf, 8192);

  mLcd.text ("playThread loop");
  while (true) {
    if (osSemaphoreWait (audioSem, 50) == osOK) {
      //int16_t* audioPtr = audioBuf + (audioBufferFull * 2048);
      //int16_t* audioSamples = hlsRadio->getAudioSamples (hlsRadio->mPlayFrame, seqNum);
      //if (hlsRadio->mPlaying && audioSamples) {
      //  memcpy (audioPtr, audioSamples, 4096);
      //  hlsRadio->mPlayFrame++;
      //  }
      //else
      //  memset (audioPtr, 0, 4096);
      audioBufferFull = !audioBufferFull;
      }
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

  mLcd.init (SDRAM_FRAME0, SDRAM_FRAME1, kLcdInterrupts);
  mLcd.setTitle (__TIME__ __DATE__);
  mLcd.text ("startThread started");
  mLcd.drawText();

  //const osThreadDef_t osThreadUi = { (char*)"UI", uiThread, osPriorityNormal, 0, 2000 };
  //osThreadCreate (&osThreadUi, NULL);
  const osThreadDef_t osThreadLoad =  { (char*)"Load", loadThread, osPriorityNormal, 0, 20000 };
  osThreadCreate (&osThreadLoad, NULL);
  //const osThreadDef_t osThreadPlay =  { (char*)"Play", playThread, osPriorityNormal, 0, 2000 };
  //osThreadCreate (&osThreadPlay, NULL);

  if (false) {
  //if (true) {
    tcpip_init (NULL, NULL);
    mLcd.text ("tcpip_init ok");

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
// base Address is 0x20010000 since this memory interface is the AXI.
// region Size is 256KB, it is related to SRAM1 and SRAM2 memory size

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

  // Enable HSE Oscillator and activate PLL with HSE as source
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
  audioSem = osSemaphoreCreate (osSemaphore (aud), -1);

  const osThreadDef_t osThreadStart = { (char*)"Start", startThread, osPriorityNormal, 0, 4000 };
  osThreadCreate (&osThreadStart, NULL);

  osKernelStart();

  return 0;
  }
//}}}
