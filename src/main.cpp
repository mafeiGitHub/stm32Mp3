// main.cpp
//{{{  includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

#include "cmsis_os.h"

#include "../Bsp/ethernetif.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "ipv4/lwip/ip_addr.h"
#include "lwip/api.h"

#include "../fatfs/ff.h"
#include "../fatfs/ff_gen_drv.h"
#include "../fatfs/drivers/sd_diskio.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_sai.h"
#include "../Bsp/stm32746g_discovery_audio.h"

#include "../Bsp/stm32746g_discovery.h"
#include "../Bsp/stm32746g_discovery_ts.h"
#include "../Bsp/stm32746g_discovery_lcd.h"

#include "../sys/cpuUsage.h"
#include "../httpServer/httpServer.h"
//}}}
//{{{  ip defines
// static IP address
#define IP_ADDR0  192
#define IP_ADDR1  168
#define IP_ADDR2  1
#define IP_ADDR3  67

// netmask
#define NETMASK_ADDR0  255
#define NETMASK_ADDR1  255
#define NETMASK_ADDR2  255
#define NETMASK_ADDR3   0

// gateway address
#define GW_ADDR0  192
#define GW_ADDR1  168
#define GW_ADDR2  0
#define GW_ADDR3  1

// dhcp defines
#define DHCP_START             (uint8_t)1
#define DHCP_WAIT_ADDRESS      (uint8_t)2
#define DHCP_ADDRESS_ASSIGNED  (uint8_t)3
#define DHCP_TIMEOUT           (uint8_t)4
#define DHCP_LINK_DOWN         (uint8_t)5
//}}}
//{{{  sdram allocation
#define SDRAM_FRAME0   0xC0000000 // frameBuffer 272*480*4 = 0x7f800 = 512k-2048b leave bit of guard for clipping errors
#define SDRAM_FRAME1   0xC0080000
#define SDRAM_HEAP     0xC0100000 // sdram heap
#define SDRAM_HEAP_SIZE  0x700000 // 7m
//}}}
//{{{  const
const char* startTaskName = "Start";
const char* dhcpTaskName = "DHCP";
const char* playTaskName = "Play";
const char* loadTaskName = "Load";
const char* uiTaskName = "UI";
//}}}

//{{{
class cInfo {
public:
  //{{{
  cInfo (int width, int height, int lines) : mWidth(width), mHeight(height), mNumLines(lines) {

    mTitle[0] = 0;
    mFooter[0] = 0;
    }
  //}}}
  ~cInfo() {}

  void title (char* str) {
    strcpy (mTitle, str);
    }
  void footer (char* str) {
    //strcpy (mFooter, str);
    }

  void line (int colour, char* str) {
    mLines[mCurLine].mColour = colour;
    strcpy (mLines[mCurLine].mStr, str);
    if (mCurLine < 100)
      mCurLine++;
    }

  void line (char* str) {
    line (LCD_WHITE, str);
    }

  void drawLines() {
    int y = 0;
    if (mTitle[0])
      lcdString (LCD_WHITE, mFontHeight, mTitle, 0, y, mWidth, mLineInc);
    for (auto i = 0; i < mCurLine; i++) {
      y += mLineInc;
      if (y > mHeight - (2*mLineInc))
        break;
      lcdString (mLines[i].mColour, mFontHeight, mLines[i].mStr, 0, y, mWidth, mLineInc);
      }

    //if (mFooter[0])
    //  lcdString (LCD_WHITE, mFontHeight, mFooter, 0, mHeight-mLineInc, mWidth, mLineInc);
    }

private:
  //{{{
  class cLine {
  public:
    cLine() { mStr[0] = 0; }
    ~cLine() {}

    int mColour = 0;
    char mStr[60];
    };
  //}}}

  int mWidth = 0;
  int mHeight = 0;

  int mFontHeight = 20;
  int mLineInc = 30;

  int mCurLine = 0;
  int mNumLines = 0;
  char mTitle[100];
  char mFooter[100];
  cLine mLines[100];
  };
//}}}
//{{{  vars
static struct netif gNetif;
static volatile uint8_t DHCP_state = DHCP_START;

static osSemaphoreId dhcpSem;
static osSemaphoreId audioSem;
static osSemaphoreId loadedSem;

static cInfo* mInfo;
//}}}

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

// threads
//{{{
static void dhcpThread (void const* argument) {

  auto netif = (struct netif*)argument;

  struct ip_addr ipAddrInit;
  IP4_ADDR (&ipAddrInit, 0, 0, 0, 0);

  while (true) {
    switch (DHCP_state) {
      case DHCP_START:
        netif->ip_addr = ipAddrInit;
        netif->netmask = ipAddrInit;
        netif->gw = ipAddrInit;
        dhcp_start (netif);
        DHCP_state = DHCP_WAIT_ADDRESS;
        break;

      case DHCP_WAIT_ADDRESS:
        if (netif->ip_addr.addr) {
          dhcp_stop (netif);
          DHCP_state = DHCP_ADDRESS_ASSIGNED;
          osSemaphoreRelease (dhcpSem);
          }
        else if (netif->dhcp->tries > 4) {
          //  DHCP timeout
          DHCP_state = DHCP_TIMEOUT;
          dhcp_stop (netif);

          // use static address
          struct ip_addr ipAddr, netmask, gateway;
          IP4_ADDR (&ipAddr, IP_ADDR0 ,IP_ADDR1 , IP_ADDR2 , IP_ADDR3 );
          IP4_ADDR (&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
          IP4_ADDR (&gateway, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
          netif_set_addr (netif, &ipAddr , &netmask, &gateway);
          osSemaphoreRelease (dhcpSem);
          }
        break;

      default:
        break;
      }

    osDelay (250); // ms
    }

  osThreadTerminate (NULL);
  }
//}}}
//{{{
static void loadThread (void const* argument) {

  BSP_SD_Init();
  while (BSP_SD_IsDetected() != SD_PRESENT) {
    lcdString (LCD_RED, 20, "no SD card", 0, 30, lcdGetXSize(), 24);
    lcdSendWait();
    }
  char SD_Path[4];
  if (FATFS_LinkDriver (&SD_Driver, SD_Path) != 0) {
    lcdString (LCD_RED, 20, "FatFs SD error", 0, 30, lcdGetXSize(), 24);
    lcdSendWait();
    while(1) {}
    }

  FATFS fatFs;
  f_mount (&fatFs, "", 0);

  FILINFO filInfo;
  filInfo.lfname = (char*)malloc (_MAX_LFN + 1);
  filInfo.lfsize = _MAX_LFN + 1;

  const char* ext ="MP3";

  DIR dir;
  if (f_opendir (&dir, "/") != FR_OK) {
    lcdString (LCD_RED, 20, "openDir error", 0, 30, lcdGetXSize(), 24);
    lcdSendWait();
    while(1) {}
    }

  while (1) {
    if ((f_readdir (&dir, &filInfo) != FR_OK) || filInfo.fname[0] == 0)
      break;
    if (filInfo.fname[0] == '.')
      continue;

    if (!(filInfo.fattrib & AM_DIR)) {
      int i = 0;
      while (filInfo.fname[i++] != '.') {;}
      if ((filInfo.fname[i] == ext[0]) && (filInfo.fname[i+1] == ext[1]) && (filInfo.fname[i+2] == ext[2])) {
        lcdString (LCD_BLUE, 20, filInfo.lfname[0] ? (char*)filInfo.lfname : (char*)&filInfo.fname, 0, 30, lcdGetXSize(), 24);
        osDelay (1000);
        }
      }
    }


  while (true) {
    osDelay (1000);
    osSemaphoreWait (loadedSem, osWaitForever);
    }
  }
//}}}
//{{{
static void playThread (void const* argument) {

  int curVol = 80;
  BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_BOTH, curVol, 48000);
  BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);

  bool audioBufferFull = false;
  int16_t* audioBuf = (int16_t*)malloc (8192);
  memset (audioBuf, 0, 8192);
  BSP_AUDIO_OUT_Play ((uint16_t*)audioBuf, 8192);

  while (true) {
    if (osSemaphoreWait (audioSem, 50) == osOK) {
      int16_t* audioPtr = audioBuf + (audioBufferFull * 2048);
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
static void uiThread (void const* argument) {

  uint32_t frameBufferAddress = SDRAM_FRAME0;

  //  init touch
  TS_StateTypeDef TS_State;
  BSP_TS_Init (lcdGetXSize(), lcdGetYSize());
  uint32_t took = 0;
  while (true) {
    BSP_TS_GetState (&TS_State);

    frameBufferAddress = (frameBufferAddress == SDRAM_FRAME0) ? SDRAM_FRAME1 : SDRAM_FRAME0;
    lcdSetLayer (0, frameBufferAddress);

    lcdFrameSync();
    auto time = osKernelSysTick();
    lcdClear (LCD_BLACK);

    // topLine info
    char str [80];
    sprintf (str, "Mp3 player %s %s", __TIME__, __DATE__);
    lcdString (LCD_WHITE, 20, str, 0, 0, lcdGetXSize(), 24);

    // volume bar
    lcdRect (LCD_YELLOW, lcdGetXSize()-20, 0, 20, (80 * lcdGetYSize()) / 100);

    // centre bar
    lcdRect (LCD_GREY, lcdGetXSize()/2, 0, 1, lcdGetYSize());

    // waveform
    int frames = 0;
    uint8_t* power = nullptr;
    int frame = 0 - lcdGetXSize()/2;
    for (auto x = 0; x < lcdGetXSize(); x++, frame++) {
      if (frames <= 0)
        power = nullptr;
      if (power) {
        uint8_t top = *power++;
        uint8_t ylen = *power++;
        lcdRect (LCD_BLUE, x, top, 1, ylen);
        frames--;
        }
      }

    //sprintf (str, "cpu:%2d%% heapFree:%d screenMs:%02d", osGetCPUUsage(), xPortGetFreeHeapSize(), (int)took);
    //mInfo->footer (str);
    mInfo->drawLines();
    lcdDebug (lcdGetYSize()-48);

    // botLine sysInfo
    sprintf (str, "cpu:%2d%% heapFree:%d screenMs:%02d", osGetCPUUsage(), xPortGetFreeHeapSize(), (int)took);
    lcdString (LCD_WHITE, 20, str, 0, lcdGetYSize()-24, lcdGetXSize(), 24);
    lcdSendWait();
    lcdShowLayer (0, frameBufferAddress, 255);

    for (auto i = 0; i < TS_State.touchDetected; i++) {
      auto x = TS_State.touchX[i];
      auto y = TS_State.touchY[i];
      if (TS_State.touchWeight[i])
        lcdEllipse (LCD_GREEN, x, y, TS_State.touchWeight[i], TS_State.touchWeight[i]);
      }

    took = osKernelSysTick() - time;
    }
  }
//}}}
//{{{
static void startThread (void const* argument) {

  //  first time lcd message, must be in thread for lcdWait semaphore
  lcdClear (LCD_BLACK);

  char str [40];
  sprintf (str, "Mp3 player %s %s first", __TIME__, __DATE__);
  lcdString (LCD_WHITE, 20, str, 0, 0, lcdGetXSize(), 24);
  lcdSendWait();
  lcdDisplayOn();

  if (false) {
    // create tcpIp stack thread
    tcpip_init (NULL, NULL);

    // init LwIP stack
    struct ip_addr ipAddr, netmask, gateway;
    IP4_ADDR (&ipAddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
    IP4_ADDR (&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
    IP4_ADDR (&gateway, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
    netif_add (&gNetif, &ipAddr, &netmask, &gateway, NULL, &ethernetif_init, &tcpip_input);
    netif_set_default (&gNetif);

    if (netif_is_link_up (&gNetif)) {
      //{{{  up
      netif_set_up (&gNetif);
      DHCP_state = DHCP_START;
      //}}}
      lcdString (LCD_BLUE, 20, "ethernet ok", 0, 30, lcdGetXSize(), 24);
      }
    else {
      //{{{  down
      netif_set_down (&gNetif);
      DHCP_state = DHCP_LINK_DOWN;
      //}}}
      lcdString (LCD_RED, 20, "no ethernet", 0, 30, lcdGetXSize(), 24);
      }
    lcdSendWait();

    const osThreadDef_t osThreadDHCP =  { (char*)dhcpTaskName, dhcpThread, osPriorityBelowNormal, 0, 256 };
    osThreadCreate (&osThreadDHCP, &gNetif);

    while (osSemaphoreWait (dhcpSem, 5000) == osOK) {}
    httpServerInit();
    // load
    //const osThreadDef_t osThreadLoad =  { (char*)loadTaskName, loadThread, osPriorityNormal, 0, 15000 };
    //osThreadCreate (&osThreadLoad, NULL);
    // play
    //const osThreadDef_t osThreadPlay =  { (char*)playTaskName, playThread, osPriorityAboveNormal, 0, 2000 };
    //osThreadCreate (&osThreadPlay, NULL);
    }

  // ui
  const osThreadDef_t osThreadUi = { (char*)uiTaskName, uiThread, osPriorityNormal, 0, 2000 };
  osThreadCreate (&osThreadUi, NULL);

  BSP_SD_Init();
  while (BSP_SD_IsDetected() != SD_PRESENT)
    mInfo->line (LCD_RED, "no SD card");
  mInfo->line ("SD card");

  char SD_Path[4];
  if (FATFS_LinkDriver (&SD_Driver, SD_Path) != 0) {
    mInfo->line (LCD_RED, "FatFs SD error");
    while (true) {}
    }
  mInfo->line ("FATSFS ok");

  FATFS fatFs;
  f_mount (&fatFs, "", 0);
  mInfo->line ("mount ok");

  FILINFO filInfo;
  filInfo.lfname = (char*)malloc (_MAX_LFN + 1);
  filInfo.lfsize = _MAX_LFN + 1;

  const char* ext ="MP3";

  DIR dir;
  if (f_opendir (&dir, "/") != FR_OK) {
    mInfo->line (LCD_RED, "openDir error");
    while (true) {}
    }
  mInfo->line ("open dir ok");

  while (true) {
    if ((f_readdir (&dir, &filInfo) != FR_OK) || filInfo.fname[0] == 0)
      break;
    mInfo->line ((char*)&filInfo.fname);

    if (filInfo.fname[0] == '.')
      continue;

    if (!(filInfo.fattrib & AM_DIR)) {
      int i = 0;
      while (filInfo.fname[i++] != '.') {;}
      if ((filInfo.fname[i] == ext[0]) && (filInfo.fname[i+1] == ext[1]) && (filInfo.fname[i+2] == ext[2])) {
        mInfo->line (filInfo.lfname[0] ? (char*)filInfo.lfname : (char*)&filInfo.fname);
        }
      }
    }

  osDelay (10000);
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

  // init cpu caches, clocks and memoryregions
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
  osSemaphoreDef (Loaded);
  loadedSem = osSemaphoreCreate (osSemaphore (Loaded), -1);
  osSemaphoreDef (aud);
  audioSem = osSemaphoreCreate (osSemaphore (aud), -1);

  lcdInit (SDRAM_FRAME0);
  mInfo = new cInfo (lcdGetXSize(), lcdGetYSize(), 100);

  // kick off start thread
  const osThreadDef_t osThreadStart = { (char*)startTaskName, startThread, osPriorityNormal, 0, 4000 };
  osThreadCreate (&osThreadStart, NULL);

  osKernelStart();
  return 0;
  }
//}}}
