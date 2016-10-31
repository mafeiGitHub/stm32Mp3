// cSd.h
#pragma once
//{{{  includes
#include <stdbool.h>
#include <string>
#include "stm32746g_discovery.h"
//}}}

#define SD_CardInfo HAL_SD_CardInfoTypedef

#define MSD_OK                        ((uint8_t)0x00)
#define MSD_ERROR                     ((uint8_t)0x01)
#define MSD_ERROR_SD_NOT_PRESENT      ((uint8_t)0x02)

uint8_t SD_Init();
uint8_t SD_ITConfig();

bool SD_present();

HAL_SD_TransferStateTypedef SD_GetStatus();
void SD_GetCardInfo (HAL_SD_CardInfoTypedef *CardInfo);
std::string SD_info();

uint8_t SD_ReadBlocks (uint32_t *pData, uint64_t ReadAddr, uint16_t blocks);
uint8_t SD_WriteBlocks (uint32_t *pData, uint64_t WriteAddr, uint16_t blocks);

uint8_t SD_Erase (uint64_t StartAddr, uint64_t EndAddr);

int8_t SD_IsReady();
int8_t SD_GetCapacity (uint32_t* block_num, uint16_t* block_size);
int8_t SD_Read (uint8_t* buf, uint32_t blk_addr, uint16_t blocks);
int8_t SD_Write (uint8_t* buf, uint32_t blk_addr, uint16_t blocks);
