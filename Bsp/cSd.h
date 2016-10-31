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

uint8_t BSP_SD_Init();
uint8_t BSP_SD_ITConfig();

bool BSP_SD_present();

HAL_SD_TransferStateTypedef BSP_SD_GetStatus();
void BSP_SD_GetCardInfo (HAL_SD_CardInfoTypedef *CardInfo);
std::string BSP_SD_info();

uint8_t BSP_SD_ReadBlocks (uint32_t *pData, uint64_t ReadAddr, uint32_t blocks);
uint8_t BSP_SD_WriteBlocks (uint32_t *pData, uint64_t WriteAddr, uint32_t blocks);

uint8_t BSP_SD_Erase (uint64_t StartAddr, uint64_t EndAddr);

int8_t BSP_SD_IsReady (uint8_t lun);
int8_t BSP_SD_GetCapacity (uint8_t lun, uint32_t* block_num, uint16_t* block_size);
int8_t BSP_SD_Read (uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blocks);
int8_t BSP_SD_Write (uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blocks);
