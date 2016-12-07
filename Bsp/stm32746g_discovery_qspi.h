#pragma once
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}

#include "stm32f7xx_hal.h"

// QSPI Error codes
#define QSPI_OK            ((uint8_t)0x00)
#define QSPI_ERROR         ((uint8_t)0x01)
#define QSPI_BUSY          ((uint8_t)0x02)
#define QSPI_NOT_SUPPORTED ((uint8_t)0x04)
#define QSPI_SUSPENDED     ((uint8_t)0x08)

// N25Q128A13EF840E Micron memory
#define QSPI_FLASH_SIZE    23     // Address bus width to access whole memory space
#define QSPI_PAGE_SIZE     256

typedef struct {
  uint32_t FlashSize;          /*!< Size of the flash */
  uint32_t EraseSectorSize;    /*!< Size of sectors for the erase operation */
  uint32_t EraseSectorsNumber; /*!< Number of sectors for the erase operation */
  uint32_t ProgPageSize;       /*!< Size of pages for the program operation */
  uint32_t ProgPagesNumber;    /*!< Number of pages for the program operation */
  } QSPI_Info;

uint8_t BSP_QSPI_Init();
uint8_t BSP_QSPI_DeInit();

uint8_t BSP_QSPI_Read (uint8_t* pData, uint32_t ReadAddr, uint32_t Size);
uint8_t BSP_QSPI_Write (uint8_t* pData, uint32_t WriteAddr, uint32_t Size);

uint8_t BSP_QSPI_Erase_Block (uint32_t BlockAddress);
uint8_t BSP_QSPI_Erase_Chip();

uint8_t BSP_QSPI_GetStatus();
uint8_t BSP_QSPI_GetInfo (QSPI_Info* pInfo);
uint8_t BSP_QSPI_EnableMemoryMappedMode();

// These functions can be modified in case the current settings need to be changed for specific application needs
void BSP_QSPI_MspInit (QSPI_HandleTypeDef *hqspi, void *Params);
void BSP_QSPI_MspDeInit (QSPI_HandleTypeDef *hqspi, void *Params);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
