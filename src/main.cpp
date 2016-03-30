// main.cpp
//{{{  includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

#include "cmsis_os.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_sai.h"

#include "../Bsp/stm32746g_discovery_audio.h"
#include "../Bsp/stm32746g_discovery.h"
#include "../Bsp/stm32746g_discovery_ts.h"
#include "../Bsp/stm32746g_discovery_lcd.h"
#include "../Bsp/ethernetif.h"

#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "ipv4/lwip/ip_addr.h"
#include "lwip/api.h"

#include "../fatfs/ff.h"
#include "../fatfs/ff_gen_drv.h"
#include "../fatfs/drivers/sd_diskio.h"

#include "../sys/cpuUsage.h"
#include "../httpServer/httpServer.h"

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

//#include "cMp3decoder.h"
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

//{{{
static std::string toString (int value) {

  std::ostringstream os;
  os << value;
  return os.str();
  }
//}}}
//{{{
class cInfo {
public:
  //{{{
  cInfo()  {
    mWidth = lcdGetXSize();
    mHeight = lcdGetYSize();
    mStartTime = osKernelSysTick();
    updateDisplayLines();
    }
  //}}}
  ~cInfo() {}

  //{{{
  void setShowTime (bool enable) {
    mShowTime = enable;
    }
  //}}}
  //{{{
  void setLcdDebug (bool enable) {
    mLcdDebug = enable;
    updateDisplayLines();
    }
  //}}}
  //{{{
  void setTitle (std::string title) {
    mTitle = title;
    updateDisplayLines();
    }
  //}}}
  //{{{
  void setFooter (std::string footer) {
    mFooter = footer;
    updateDisplayLines();
    }
  //}}}
  //{{{
  void line (int colour, std::string str) {

    bool tailing = mLastLine == (int)mDisplayFirstLine + mNumDisplayLines - 1;

    auto line = (mLastLine < mMaxLine-1) ? mLastLine+1 : mLastLine;
    mLines[line].mTime = osKernelSysTick();
    mLines[line].mColour = colour;
    mLines[line].mString = str;
    mLastLine = line;

    if (tailing)
      mDisplayFirstLine = mLastLine - mNumDisplayLines + 1;
    }
  //}}}
  //{{{
  void line (std::string str) {
    line (LCD_WHITE, str);
    }
  //}}}

  //{{{
  void setDisplayTail() {

    if (mLastLine > (int)mNumDisplayLines-1)
      mDisplayFirstLine = mLastLine - mNumDisplayLines + 1;
    else
      mDisplayFirstLine = 0;
    }
  //}}}
  //{{{
  void incDisplayLines (int xinc, int yinc) {

    float value = mDisplayFirstLine - (yinc / 4.0f);

    if (value < 0)
      mDisplayFirstLine = 0;
    else if (mLastLine <= (int)mNumDisplayLines-1)
      mDisplayFirstLine = 0;
    else if (value > mLastLine - mNumDisplayLines + 1)
      mDisplayFirstLine = mLastLine - mNumDisplayLines + 1;
    else
      mDisplayFirstLine = value;
    }
  //}}}
  //{{{
  void drawLines() {

    auto y = 0;
    if (!mTitle.empty()) {
      lcdString (LCD_WHITE, mFontHeight, mTitle, 0, y, mWidth, mLineInc);
      y += mLineInc;
      }

    if (mLastLine >= 0) {
      auto yorg = mLineInc + ((int)mDisplayFirstLine * mNumDisplayLines * mLineInc / (mLastLine + 1));
      auto ylen = mNumDisplayLines * mNumDisplayLines * mLineInc / (mLastLine + 1);
      lcdRect (LCD_YELLOW, 0, yorg, 8, ylen);
      }

    auto lastLine = (int)mDisplayFirstLine + mNumDisplayLines - 1;
    if (lastLine > mLastLine)
      lastLine = mLastLine;
    for (auto lineIndex = (int)mDisplayFirstLine; lineIndex <= lastLine; lineIndex++) {
      auto x = 0;
      if (mShowTime) {
        lcdString (LCD_GREEN, mFontHeight,
                   toString ((mLines[lineIndex].mTime-mStartTime)/1000) + "." +
                   toString ((mLines[lineIndex].mTime-mStartTime) % 1000), x, y, mWidth, mLineInc);
        x += mLineInc*3;
        }
      lcdString (mLines[lineIndex].mColour, mFontHeight, mLines[lineIndex].mString, x, y, mWidth, mLineInc);
      y += mLineInc;
      }

    if (mLcdDebug)
      lcdDebug (mHeight - 2 * mLineInc);

    if (!mFooter.empty())
      lcdString (LCD_YELLOW, mFontHeight, mFooter, 0, mHeight-mLineInc-1, mWidth, mLineInc);
    }
  //}}}

private:
  //{{{
  void updateDisplayLines() {

    auto lines = mHeight / mLineInc;

    if (!mTitle.empty())
      lines--;

    if (mLcdDebug)
      lines--;

    if (!mFooter.empty())
      lines--;

    mNumDisplayLines = lines;
    }
  //}}}

  int mWidth = 0;
  int mHeight = 0;
  int mStartTime = 0;

  float mDisplayFirstLine = 0;
  int mNumDisplayLines = 0;
  int mFontHeight = 16;
  int mLineInc = 18;

  bool mShowTime = true;
  bool mLcdDebug = false;

  std::string mTitle;
  std::string mFooter;

  int mLastLine = -1;
  int mMaxLine = 500;

  //{{{
  class cLine {
  public:
    cLine() {}
    ~cLine() {}

    int mTime = 0;
    int mColour = LCD_WHITE;
    std::string mString;
    };
  //}}}
  cLine mLines[500];
  };
//}}}
//{{{  vars
static struct netif gNetif;
static volatile uint8_t DHCP_state = DHCP_START;

static osSemaphoreId dhcpSem;
static osSemaphoreId audioSem;
static osSemaphoreId loadedSem;

static cInfo mInfo;
static float mVolume = 0.8f;
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
static void uiThread (void const* argument) {

  const int kInfo = 150;
  const int kVolume = 440;

  mInfo.line ("UIThread started");

  bool lastValid [5] = {false, false, false, false, false};
  int lastx [5];
  int lasty [5];
  uint32_t frameBufferAddress = SDRAM_FRAME0;

  //  init touch
  TS_StateTypeDef tsState;
  BSP_TS_Init (lcdGetXSize(), lcdGetYSize());
  uint32_t took = 0;
  while (true) {
    BSP_TS_GetState (&tsState);

    frameBufferAddress = (frameBufferAddress == SDRAM_FRAME0) ? SDRAM_FRAME1 : SDRAM_FRAME0;
    lcdSetLayer (0, frameBufferAddress);
    lcdFrameSync();
    auto time = osKernelSysTick();
    lcdClear (LCD_BLACK);

    //  touchscreen
    for (auto i = 0; i < 5; i++) {
      if ((i < tsState.touchDetected) && tsState.touchWeight[i]) {
        auto x = tsState.touchX[i];
        auto y = tsState.touchY[i];

        if ((x >= kInfo) && (x < kVolume))
          mInfo.line (toString(x) + "," + toString (y) + "," + toString (tsState.touchWeight[i]) + " " + toString(i));

        lcdEllipse (x < kInfo ? LCD_GREEN : x < 440 ? LCD_MAGENTA : LCD_YELLOW, x, y, tsState.touchWeight[i], tsState.touchWeight[i]);
        if (x < kInfo) {
          //{{{  adjust mInfo
          if (y > 260)
            mInfo.setDisplayTail();
          else if (lastValid[i])
            mInfo.incDisplayLines (x-lastx[i], y - lasty[i]);
          }
          //}}}
        else if (x >= kVolume){
          //{{{  adjust volume
          auto volume = lastValid[i] ? mVolume + float(y - lasty[i]) / lcdGetYSize() : float(y) / lcdGetYSize();

          if (volume < 0)
            volume = 0;
          else if (volume > 1.0f)
            volume = 1.0f;

          if (volume != mVolume) {
            mInfo.line ("vol " + toString(int(volume*100)) + "%");
            mVolume = volume;
            }
          }
          //}}}
        lastx[i] = x;
        lasty[i] = y;
        lastValid[i] = true;
        }
      else
        lastValid[i] = false;
      }
    //{{{  volume bar
    lcdRect (LCD_YELLOW, lcdGetXSize()-20, 0, 20, int(mVolume * lcdGetYSize()));
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

    mInfo.setFooter (toString (osGetCPUUsage()) + "% " + toString (xPortGetFreeHeapSize()) + "free " + toString (took) + "ms");
    mInfo.drawLines();
    lcdSendWait();
    lcdShowLayer (0, frameBufferAddress, 255);

    took = osKernelSysTick() - time;
    }
  }
//}}}
//{{{
static void loadThread (void const* argument) {

  mInfo.line ("loadThread started");
  BSP_SD_Init();
  while (BSP_SD_IsDetected() != SD_PRESENT) {
    mInfo.line (LCD_RED, "no SD card");
    osDelay (1000);
    }
  mInfo.line ("SD card found");

  char SD_Path[4];
  if (FATFS_LinkDriver (&SD_Driver, SD_Path) != 0)
    mInfo.line (LCD_RED, "SD driver error");
  else {
    mInfo.line ("SD driver found");

    FATFS fatFs;
    f_mount (&fatFs, "", 0);
    mInfo.line ("FAT fileSystem mounted");

    DIR dir;
    if (f_opendir (&dir, "/") != FR_OK)
      mInfo.line (LCD_RED, "directory open error");
    else {
      mInfo.line ("directory opened");

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
            mInfo.line (filInfo.lfname[0] ? (char*)filInfo.lfname : (char*)&filInfo.fname);
            }
          }
        }
      }
    }

  int tick = 0;
  while (true) {
    osDelay (5000);
    mInfo.line ("load tick" + toString (tick++));
    //osSemaphoreWait (loadedSem, osWaitForever);
    }
  }
//}}}
//{{{
static void playThread (void const* argument) {

  mInfo.line ("playThread started");

  int curVol = 80;
  BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_BOTH, curVol, 48000);
  BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_02);

  bool audioBufferFull = false;
  int16_t* audioBuf = (int16_t*)malloc (8192);
  memset (audioBuf, 0, 8192);
  BSP_AUDIO_OUT_Play ((uint16_t*)audioBuf, 8192);

  mInfo.line ("playThread loop");
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

  mInfo.line ("dhcpThread started");
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
static void startThread (void const* argument) {

  mInfo.line ("startThread started");

  // clear LCD and turn it on
  lcdClear (LCD_BLACK);
  lcdSendWait();
  lcdDisplayOn();

  // ui
  const char* uiTaskName = "UI";
  const osThreadDef_t osThreadUi = { (char*)uiTaskName, uiThread, osPriorityNormal, 0, 2000 };
  osThreadCreate (&osThreadUi, NULL);

  // load
  const char* loadTaskName = "Load";
  const osThreadDef_t osThreadLoad =  { (char*)loadTaskName, loadThread, osPriorityNormal, 0, 8000 };
  osThreadCreate (&osThreadLoad, NULL);

  // play
  const char* playTaskName = "Play";
  const osThreadDef_t osThreadPlay =  { (char*)playTaskName, playThread, osPriorityAboveNormal, 0, 2000 };
  osThreadCreate (&osThreadPlay, NULL);

  // create tcpIp stack thread
  tcpip_init (NULL, NULL);
  mInfo.line ("tcpip_init ok");

  // init LwIP stack
  struct ip_addr ipAddr, netmask, gateway;
  IP4_ADDR (&ipAddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
  IP4_ADDR (&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
  IP4_ADDR (&gateway, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
  netif_add (&gNetif, &ipAddr, &netmask, &gateway, NULL, &ethernetif_init, &tcpip_input);
  netif_set_default (&gNetif);

  if (netif_is_link_up (&gNetif)) {
    // ethernet ok
    netif_set_up (&gNetif);
    mInfo.line ("ethernet connected");

    DHCP_state = DHCP_START;
    const char* dhcpTaskName = "DHCP";
    const osThreadDef_t osThreadDHCP =  { (char*)dhcpTaskName, dhcpThread, osPriorityBelowNormal, 0, 256 };
    osThreadCreate (&osThreadDHCP, &gNetif);

    while (osSemaphoreWait (dhcpSem, 1000) == osOK)
      osDelay (1000);

    mInfo.line ("dhcp address allocated");
    httpServerInit();
    mInfo.line ("httpServer started");
    }
  else {
    //{{{  no ethernet
    netif_set_down (&gNetif);
    DHCP_state = DHCP_LINK_DOWN;
    mInfo.line (LCD_RED, "no ethernet");
    }
    //}}}

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
  osSemaphoreDef (Loaded);
  loadedSem = osSemaphoreCreate (osSemaphore (Loaded), -1);
  osSemaphoreDef (aud);
  audioSem = osSemaphoreCreate (osSemaphore (aud), -1);

  lcdInit (SDRAM_FRAME0);
  char str [40];
  sprintf (str, "Mp3 %s %s", __TIME__, __DATE__);
  mInfo.setTitle (str);

  // kick off start thread
  const char* startTaskName = "Start";
  const osThreadDef_t osThreadStart = { (char*)startTaskName, startThread, osPriorityNormal, 0, 4000 };
  osThreadCreate (&osThreadStart, NULL);

  osKernelStart();
  return 0;
  }
//}}}
