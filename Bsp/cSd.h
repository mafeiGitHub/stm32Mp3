// cSd.h
#pragma once
//{{{  includes
#include <string>
#include "stm32746g_discovery.h"
//}}}

typedef enum { MSD_OK, MSD_ERROR, MSD_ERROR_SD_NOT_PRESENT } MSD_RESULT;

uint8_t SD_Init();
uint8_t SD_ITConfig();

bool SD_present();
HAL_SD_TransferStateTypedef SD_GetStatus();
void SD_GetCardInfo (HAL_SD_CardInfoTypedef *CardInfo);
std::string SD_info();

uint8_t SD_Read (uint8_t* buf, uint32_t blk_addr, uint16_t blocks);
uint8_t SD_Write (uint8_t* buf, uint32_t blk_addr, uint16_t blocks);

uint8_t SD_Erase (uint64_t StartAddr, uint64_t EndAddr);

int8_t SD_IsReady();
int8_t SD_GetCapacity (uint32_t* block_num, uint16_t* block_size);
int8_t SD_ReadCached (uint8_t* buf, uint32_t blk_addr, uint16_t blocks);
int8_t SD_WriteCached (uint8_t* buf, uint32_t blk_addr, uint16_t blocks);
