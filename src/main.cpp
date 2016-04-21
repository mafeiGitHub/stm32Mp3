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
#include "../Bsp/clcd.h"

#include "../widgets/cRootContainer.h"
#include "../widgets/cWidget.h"
#include "../widgets/cTextBox.h"
#include "../widgets/cSelectTextBox.h"
#include "../widgets/cValueBox.h"
#include "../widgets/cWaveformWidget.h"

#include "../Bsp/stm32746g_discovery_sd.h"
#include "../fatfs/fatFs.h"

#include "../httpServer/httpServer.h"

#include "cMp3decoder.h"

using namespace std;
//}}}
//{{{  static vars
static osSemaphoreId audSem;
static bool mAudHalf = false;

static float mVolume = 0.8f;
static bool mVolumeChanged = false;
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
static void listDirectory (vector<string>& mp3Files, string directoryName, string indent) {

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
      listDirectory (mp3Files, directoryName + "/" + fileInfo.getName(), indent + "-");

    else if (fileInfo.matchExtension ("MP3")) {
      mp3Files.push_back (directoryName + "/" + fileInfo.getName());
      cLcd::debug (indent + fileInfo.getName());
      cFile file (directoryName + "/" + fileInfo.getName(), FA_OPEN_EXISTING | FA_READ);
      //cLcd::debug ("- filesize " + cLcd::intStr (file.getSize()));
      }
    }
  }
//}}}

// threads
//{{{
static void uiThread (void const* argument) {

  auto lcd = cLcd::get();
  auto root = cRootContainer::get();
  cLcd::debug ("uiThread started");

  //{{{  init touch
  BSP_TS_Init (root->getWidth(),root->getHeight());
  int pressed[5] = {0, 0, 0, 0, 0};
  int16_t x[5];
  int16_t y[5];
  uint8_t z[5];
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
          root->press (pressed[0], x[0], y[0], z[0], xinc, yinc);
        pressed[touch]++;
        }
      else {
        x[touch] = 0;
        y[touch] = 0;
        z[touch] = 0;
        if (!touch && pressed[0])
          root->release();
        pressed[touch] = 0;
        }
      }
    //}}}

    lcd->startDraw();
    root->draw (lcd);
    lcd->endDraw();

    if (mVolumeChanged) {
      //{{{  change volume
      BSP_AUDIO_OUT_SetVolume (int(mVolume * 100));
      mVolumeChanged = false;
      }
      //}}}
    }
  }
//}}}
//{{{
static void loadThread (void const* argument) {

  auto lcd = cLcd::get();
  auto root = cRootContainer::get();
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
  cLcd::debug (fatFs->getLabel() + " vsn:" + cLcd::hexStr (fatFs->getVolumeSerialNumber()) +
               " freeSectors:" + cLcd::intStr (fatFs->getFreeSectors()));
  lcd->setShowDebug (false, false, false, false);

  vector <string> mp3Files;
  listDirectory (mp3Files, "", "");
  string selectedFileName;
  bool selectedFileChanged = false;
  for (unsigned int i = 0; i < 14 && i < mp3Files.size(); i++)
    root->addNextBelow (new cSelectTextBox (
      mp3Files[i], selectedFileName, selectedFileChanged, root->getWidth() - cWidget::kBoxHeight));
  //{{{  create volume widget
  root->addTopRight (new cValueBox (mVolume, mVolumeChanged, LCD_YELLOW, cWidget::kBoxHeight-1, root->getHeight()-6));
  //}}}
  //{{{  create position widget
  float position = 0.0f;
  bool positionChanged = false;;
  root->addBottomLeft (new cValueBox (position, positionChanged, LCD_BLUE, root->getWidth(), 6));
  //}}}
  //{{{  create waveform widget
  auto playFrame = 0;
  auto waveform = (float*) pvPortMalloc (480*2*4);
  root->addTopLeft (new cWaveformWidget (playFrame, waveform, root->getWidth(), root->getHeight()));
  //}}}

  auto mp3Decoder = new cMp3Decoder;
  cLcd::debug ("mp3Decoder created");

  osSemaphoreDef (aud);
  audSem = osSemaphoreCreate (osSemaphore (aud), -1);
  BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_SPEAKER, int(mVolume * 100), 44100);  // OUTPUT_DEVICE_HEADPHONE
  BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_13);             // CODEC_AUDIOFRAME_SLOT_02

  //{{{  chunkSize and buffer
  auto chunkSize = 4096;
  auto fullChunkSize = 2048 + chunkSize;
  auto chunkBuffer = (uint8_t*)pvPortMalloc (fullChunkSize);
  //}}}
  unsigned int fileNum = 0;
  selectedFileName = mp3Files[fileNum];
  while (fileNum < mp3Files.size()) {
    lcd->setTitle (selectedFileName);
    //{{{  play selectedFileName
    cFile file (selectedFileName, FA_OPEN_EXISTING | FA_READ);
    if (!file.isOk()) {
      //{{{  error, return
      cLcd::debug ("- open failed " + cLcd::intStr (file.getResult()) + " " + selectedFileName);
      return;
      }
      //}}}
    cLcd::debug ("play " + selectedFileName + " " + cLcd::intStr (file.getSize()));

    //{{{  clear waveform
    for (auto i = 0; i < 480*2; i++)
      waveform[i] = 0.0f;
    //}}}
    //{{{  init BSP_play
    memset ((void*)AUDIO_BUFFER, 0, AUDIO_BUFFER_SIZE);
    BSP_AUDIO_OUT_Play ((uint16_t*)AUDIO_BUFFER, AUDIO_BUFFER_SIZE);
    //}}}

    position = 0.0f;
    playFrame = 0;
    int bytesLeft;
    do {
      file.read (chunkBuffer, fullChunkSize, bytesLeft);
      if (bytesLeft) {
        auto chunkPtr = chunkBuffer;
        int headerBytes;
        do {
          headerBytes = mp3Decoder->findNextHeader (chunkPtr, bytesLeft);
          if (headerBytes) {
            chunkPtr += headerBytes;
            bytesLeft -= headerBytes;
            if (bytesLeft < mp3Decoder->getFrameBodySize() + 4) { // not enough for frameBody and next header
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
            if (bytesLeft >= mp3Decoder->getFrameBodySize()) {
              osSemaphoreWait (audSem, 100);
              auto frameBytes = mp3Decoder->decodeFrameBody (chunkPtr, &waveform[(playFrame % 480) * 2], (int16_t*)(mAudHalf ? AUDIO_BUFFER : AUDIO_BUFFER_HALF));
              if (frameBytes) {
                chunkPtr += frameBytes;
                bytesLeft -= frameBytes;
                playFrame++;
                }
              else
                bytesLeft = 0;
              }
            }
          if (selectedFileChanged)
            bytesLeft = 0;
          else if (positionChanged) {
            //{{{  skip
            file.seek (int(position * file.getSize()) & 0xFFFFFFE0);
            positionChanged = false;
            headerBytes = 0;
            }
            //}}}
          else
            position = (float)file.getPosition() / (float)file.getSize();
          } while (headerBytes && (bytesLeft > 0));
        }
      } while (bytesLeft > 0);

    BSP_AUDIO_OUT_Stop (CODEC_PDWN_SW);
    //}}}
    if (!selectedFileChanged) {
      fileNum++;
      selectedFileName = mp3Files[fileNum];
      }
    selectedFileChanged = false;
    }

  vPortFree (waveform);
  vPortFree (chunkBuffer);
  }
//}}}
//{{{
static void networkThread (void const* argument) {

  static bool kStaticIp = true;

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
      cLcd::debug (LCD_YELLOW, "ethernet static ip " + cLcd::intStr ((int) (netIf.ip_addr.addr & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netIf.ip_addr.addr >> 16) & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netIf.ip_addr.addr >> 8) & 0xFF)) + "." +
                                                       cLcd::intStr ((int) (netIf.ip_addr.addr >> 24)));
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
          cLcd::debug (LCD_YELLOW, "dhcp allocated " + cLcd::intStr ( (int)(netIf.ip_addr.addr & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netIf.ip_addr.addr >> 16) & 0xFF)) + "." +
                                                       cLcd::intStr ((int)((netIf.ip_addr.addr >> 8) & 0xFF)) + "." +
                                                       cLcd::intStr ( (int)(netIf.ip_addr.addr >> 24)));
          dhcp_stop (&netIf);
          break;
          }
          //}}}
        else if (netIf.dhcp->tries > 4) {
          //{{{  dhcp timeout
          cLcd::debug (LCD_RED, "dhcp timeout");
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
    cLcd::debug (LCD_RED, "no ethernet");
    }
    //}}}

  osThreadTerminate (NULL);
  }
//}}}
//{{{
static void startThread (void const* argument) {

  cLcd::create ("mp3 player built at " + string(__TIME__) + " on " + string(__DATE__));
  cRootContainer rootContainer (cLcd::getWidth(), cLcd::getHeight());

  const osThreadDef_t osThreadUi = { (char*)"UI", uiThread, osPriorityNormal, 0, 4000 }; // 1000
  osThreadCreate (&osThreadUi, NULL);

  const osThreadDef_t osThreadLoad =  { (char*)"Load", loadThread, osPriorityNormal, 0, 16000 }; // 10000
  osThreadCreate (&osThreadLoad, NULL);

  const osThreadDef_t osThreadNetwork =  { (char*)"net", networkThread, osPriorityBelowNormal, 0, 1024 };
  osThreadCreate (&osThreadNetwork, NULL);

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
