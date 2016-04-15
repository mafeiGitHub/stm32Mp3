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

#include "../Bsp/stm32746g_discovery_sd.h"
#include "../fatfs/fatFs.h"

#include "../httpServer/httpServer.h"

#include "cMp3decoder.h"

using namespace std;
//}}}
//{{{  static vars
static struct netif gNetif;
static osSemaphoreId dhcpSem;

static float mVolume = 0.8f;

static int mPlayFrame = 0;
static int mPlayBytes = 0;
static int mFileSize = 0;
static bool mSkip = false;
static int mDirNest = 0;

static cMp3Decoder* mMp3Decoder = nullptr;
static float mPower [480*2];

static osSemaphoreId audSem;
static bool audBufHalf = false;
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
static void listDirectory (string directoryName) {

	mDirNest++;
	string indent;
	for (auto i = 0; i < mDirNest; i++)
		indent += " ";

	cLcd::debug ("dir - " + directoryName);

	cDirectory directory;
	auto result = directory.open (directoryName);
	if (result != FR_OK)
		cLcd::debug (LCD_RED, "directory open error:"  + cLcd::intStr (result));
	else {
		cFileInfo fileInfo;
		while ((directory.read (fileInfo) == FR_OK) && !fileInfo.getEmpty()) {
			if (fileInfo.getBack()) {
				//cLcd::debug (fileInfo.getName());
				}
			else if (fileInfo.isDirectory())
				listDirectory (directoryName + "/" + fileInfo.getName());
			else {
				cFile file;
				auto result = file.open (directoryName + "/" + fileInfo.getName(), FA_OPEN_EXISTING | FA_READ);
				if (result == FR_OK) {
					cLcd::debug (indent + cLcd::intStr (file.getSize()/1000) + "k " + fileInfo.getName());
					file.close();
					}
				}
			}
		directory.close();
		}

	mDirNest--;
	}
//}}}
//{{{
static void playFile (string directoryName, string fileName) {

	string fullName = directoryName + "/" + fileName;
	cLcd::instance()->setTitle (fullName);

	mPlayBytes = 0;
	for (auto i = 0; i < 480*2; i++)
		mPower[i] = 0;
	memset ((void*)AUDIO_BUFFER, 0, AUDIO_BUFFER_SIZE);

	cFile file;
	auto result = file.open (fullName, FA_OPEN_EXISTING | FA_READ);
	if (result != FR_OK) {
		cLcd::debug ("- open failed " + cLcd::intStr (result) + " " + fullName);
		return;
		}
	mFileSize = file.getSize();
	cLcd::debug ("playing " + cLcd::intStr (mFileSize) + " " + fullName);

	int chunkSize = 2048;
	auto chunkBuffer = (uint8_t*)pvPortMalloc (chunkSize + 1044);

	// play file from fileBuffer
	BSP_AUDIO_OUT_Play ((uint16_t*)AUDIO_BUFFER, AUDIO_BUFFER_SIZE);

	mPlayFrame = 0;
	mPlayBytes = 0;
	while (!mSkip && (mPlayBytes < mFileSize)) {
		int chunkBytesLeft;
		file.read (chunkBuffer, chunkSize, chunkBytesLeft);
		if (!chunkBytesLeft)
			break;

		auto chunkPtr = chunkBuffer;
		int bytesUsed;
		while (!mSkip && (bytesUsed = mMp3Decoder->findNextHeader (chunkPtr, chunkBytesLeft))) {
			chunkPtr += bytesUsed;
			chunkBytesLeft -= bytesUsed;
			mPlayBytes += bytesUsed;

			if (mMp3Decoder->getFrameBodySize() > chunkBytesLeft) {
				// load rest of frame
				int bytesLoaded;
				file.read (chunkPtr + chunkBytesLeft, mMp3Decoder->getFrameBodySize() - chunkBytesLeft, bytesLoaded);
				if (!bytesLoaded)
					break;
				chunkBytesLeft += bytesLoaded;
				}

			osSemaphoreWait (audSem, 50);
			bytesUsed = mMp3Decoder->decodeFrameBody (chunkPtr, &mPower[(mPlayFrame % 480) * 2], (int16_t*)(audBufHalf ? AUDIO_BUFFER : AUDIO_BUFFER_HALF));
			chunkPtr += bytesUsed;
			chunkBytesLeft -= bytesUsed;
			mPlayBytes += bytesUsed;
			mPlayFrame++;
			}
		}
	file.close();

	if (mSkip) {
		cLcd::debug ("- skip file at " + cLcd::intStr (mPlayBytes) + " of " +  cLcd::intStr (mFileSize));
		mSkip = false;
		}

	vPortFree (chunkBuffer);

	BSP_AUDIO_OUT_Stop (CODEC_PDWN_SW);
	}
//}}}
//{{{
static void playDirectory (string directoryName, const char* extension) {

	cDirectory directory;
	auto result = directory.open (directoryName);
	if (result != FR_OK)
		cLcd::debug (LCD_RED, "directory open error:"  + cLcd::intStr (result));
	else {
		cLcd::debug ("directory opened");

		cFileInfo fileInfo;
		while ((directory.read (fileInfo) == FR_OK) && !fileInfo.getEmpty()) {
			if (fileInfo.getBack()) {
				}
			else if (fileInfo.isDirectory())
				cLcd::debug ("directory - " +  fileInfo.getName());
			else if (!extension || fileInfo.matchExtension (extension))
				playFile (directoryName, fileInfo.getName());
			}
		directory.close();
		}
	}
//}}}

// threads
//{{{
//static void dhcpThread (void const* argument) {

	//cLcd::debug  ("dhcpThread started");
	//auto netif = (struct netif*)argument;

	//struct ip_addr nullIpAddr;
	//IP4_ADDR (&nullIpAddr, 0, 0, 0, 0);
	//netif->ip_addr = nullIpAddr;
	//netif->netmask = nullIpAddr;
	//netif->gw = nullIpAddr;
	//dhcp_start (netif);

	//while (true) {
		//if (netif->ip_addr.addr) {
			//cLcd::debug (LCD_YELLOW, "dhcp allocated " + cLcd::intStr ( (int)(netif->ip_addr.addr & 0xFF)) + "." +
																									 //cLcd::intStr ((int)((netif->ip_addr.addr >> 16) & 0xFF)) + "." +
																									 //cLcd::intStr ((int)((netif->ip_addr.addr >> 8) & 0xFF)) + "." +
																									 //cLcd::intStr ( (int)(netif->ip_addr.addr >> 24)));
			//dhcp_stop (netif);
			//osSemaphoreRelease (dhcpSem);
			//break;
			//}

		//else if (netif->dhcp->tries > 4) {
			//cLcd::debug (LCD_RED, "dhcp timeout");
			//dhcp_stop (netif);

			//// use static address
			//struct ip_addr ipAddr;
			//IP4_ADDR (&ipAddr, 192 ,168 , 1 , 67 );
			//struct ip_addr netmask;
			//IP4_ADDR (&netmask, 255, 255, 255, 0);
			//struct ip_addr gateway;
			//IP4_ADDR (&gateway, 192, 168, 0, 1);
			//netif_set_addr (netif, &ipAddr , &netmask, &gateway);
			//osSemaphoreRelease (dhcpSem);
			//break;
			//}

		//osDelay (250);
		//}

	//osThreadTerminate (NULL);
	//}
//}}}
//{{{
static void uiThread (void const* argument) {

	cLcd::debug ("uiThread started");
	auto lcd = cLcd::instance();

	const int kInfo = 150;
	const int kVolume = 440;

	// init touch
	BSP_TS_Init (cLcd::getWidth(), cLcd::getHeight());
	int pressed [5] = {0, 0, 0, 0, 0};
	int lastx [5];
	int lasty [5];
	int lastz [5];

	while (true) {
		//{{{  read touch and use it
		TS_StateTypeDef tsState;
		BSP_TS_GetState (&tsState);
		for (auto touch = 0; touch < 5; touch++) {
			if ((touch < tsState.touchDetected) && tsState.touchWeight[touch]) {
				auto x = tsState.touchX[touch];
				auto y = tsState.touchY[touch];
				auto z = tsState.touchWeight[touch];

				if (touch == 0) {
					if (x < kInfo)
						//{{{  pressed lcd info
						lcd->pressed (pressed[touch], x, y, pressed[touch] ? x - lastx[touch] : 0, pressed[touch] ? y - lasty[touch] : 0);
						//}}}
					else if (x < kVolume) {
						//{{{  pressed middle
						//cLcd::debug (cLcd::intStr (x) + "," + cLcd::intStr (y) + "," + cLcd::intStr (tsState.touchWeight[touch]));
						if (y < 20)
							mSkip = true;
						}
						//}}}
					else {
						//{{{  pressed volume
						auto volume = pressed[touch] ? mVolume + float(y - lasty[touch]) / cLcd::getHeight(): float(y) / cLcd::getHeight();

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

		lcd->startDraw();
		//{{{  draw touch
		for (auto touch = 0; (touch < 5) && pressed[touch]; touch++)
			lcd->ellipse (touch > 0 ? LCD_LIGHTGREY : lastx[0] < kInfo ? LCD_GREEN : lastx[0] < kVolume ? LCD_MAGENTA : LCD_YELLOW,
										lastx[touch], lasty[touch], lastz[touch], lastz[touch]);
		//}}}
		//{{{  draw yellow volume
		lcd->rect (LCD_YELLOW, cLcd::getWidth()-20, 0, 20, int(mVolume * cLcd::getHeight()));
		//}}}
		//{{{  draw blue play progress
		lcd->rect (LCD_BLUE, 0, 0, ((mPlayBytes >> 10) * cLcd::getWidth()) / (mFileSize >> 10), 2);
		//}}}
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
		lcd->endDraw();
		}

	}
//}}}
//{{{
static void loadThread (void const* argument) {

	cLcd::debug ("loadThread started");

	BSP_AUDIO_OUT_Init (OUTPUT_DEVICE_SPEAKER, int(mVolume * 100), 44100);  // OUTPUT_DEVICE_HEADPHONE
	BSP_AUDIO_OUT_SetAudioFrameSlot (CODEC_AUDIOFRAME_SLOT_13);             // CODEC_AUDIOFRAME_SLOT_02

	BSP_SD_Init();
	while (BSP_SD_IsDetected() != SD_PRESENT) {
		cLcd::debug (LCD_RED, "no SD card");
		osDelay (1000);
		}
	cLcd::debug ("SD card found");

	cFatFs* fatFs = cFatFs::create();
	if (fatFs->mount() == FR_OK) {
		cLcd::debug (fatFs->getLabel() +
								 " vsn:" + cLcd::hexStr (fatFs->getVolumeSerialNumber()) +
								 " freeSectors:" + cLcd::intStr (fatFs->getFreeSectors()));
		listDirectory ("");
		playDirectory ("sub", "MP3");
		}
	else
		cLcd::debug ("fatFs mount problem");

	int tick = 0;
	while (true) {
		cLcd::debug ("load tick" + cLcd::intStr (tick++));
		osDelay (10000);
		}
	}
//}}}
//{{{
static void startThread (void const* argument) {

	cLcd::create (__TIME__ __DATE__);

	const osThreadDef_t osThreadUi = { (char*)"UI", uiThread, osPriorityNormal, 0, 2000 };
	osThreadCreate (&osThreadUi, NULL);

	const osThreadDef_t osThreadLoad =  { (char*)"Load", loadThread, osPriorityNormal, 0, 10000 };
	osThreadCreate (&osThreadLoad, NULL);

	cLcd::debug ("mp3Decoder create");
	mMp3Decoder = new cMp3Decoder;
	cLcd::debug ("mp3Decoder created");

	if (true) {
		tcpip_init (NULL, NULL);
		cLcd::debug ("configuring ethernet");

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
			cLcd::debug (LCD_YELLOW, "ethernet static ip " + cLcd::intStr ((int) (gNetif.ip_addr.addr & 0xFF)) + "." +
																											 cLcd::intStr ((int)((gNetif.ip_addr.addr >> 16) & 0xFF)) + "." +
																											 cLcd::intStr ((int)((gNetif.ip_addr.addr >> 8) & 0xFF)) + "." +
																											 cLcd::intStr ((int) (gNetif.ip_addr.addr >> 24)));

			//const osThreadDef_t osThreadDHCP =  { (char*)"DHCP", dhcpThread, osPriorityBelowNormal, 0, 1024 };
			//osThreadCreate (&osThreadDHCP, &gNetif);
			//while (osSemaphoreWait (dhcpSem, 1000) == osOK)
			//  osDelay (1000);

			httpServerInit();
			cLcd::debug ("httpServer started");
			}
		else {
			// no ethernet
			netif_set_down (&gNetif);
			cLcd::debug (LCD_RED, "no ethernet");
			}
		}

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

	// init semaphores
	osSemaphoreDef (dhcp);
	dhcpSem = osSemaphoreCreate (osSemaphore (dhcp), -1);

	osSemaphoreDef (aud);
	audSem = osSemaphoreCreate (osSemaphore (aud), -1);

	// launch startThread
	const osThreadDef_t osThreadStart = { (char*)"Start", startThread, osPriorityNormal, 0, 4000 };
	osThreadCreate (&osThreadStart, NULL);

	osKernelStart();

	return 0;
	}
//}}}
