#pragma once
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}

#include "stm32746g_discovery.h"

#define SD_CardInfo HAL_SD_CardInfoTypedef

#define MSD_OK                        ((uint8_t)0x00)
#define MSD_ERROR                     ((uint8_t)0x01)
#define MSD_ERROR_SD_NOT_PRESENT      ((uint8_t)0x02)

#define SD_PRESENT               ((uint8_t)0x01)
#define SD_NOT_PRESENT           ((uint8_t)0x00)
#define SD_DATATIMEOUT           ((uint32_t)100000000)

#define BSP_SDMMC_IRQHandler              SDMMC1_IRQHandler
#define BSP_SDMMC_DMA_Tx_IRQHandler       DMA2_Stream6_IRQHandler
#define BSP_SDMMC_DMA_Rx_IRQHandler       DMA2_Stream3_IRQHandler
#define SD_DetectIRQHandler()             HAL_GPIO_EXTI_IRQHandler(SD_DETECT_PIN)

uint8_t BSP_SD_Init();
uint8_t BSP_SD_ITConfig();

void    BSP_SD_DetectIT();
uint8_t BSP_SD_IsDetected();

HAL_SD_TransferStateTypedef BSP_SD_GetStatus();
void    BSP_SD_GetCardInfo (HAL_SD_CardInfoTypedef *CardInfo);

uint8_t BSP_SD_ReadBlocks_DMA (uint32_t *pData, uint64_t ReadAddr, uint32_t NumOfBlocks);
uint8_t BSP_SD_WriteBlocks_DMA (uint32_t *pData, uint64_t WriteAddr, uint32_t NumOfBlocks);

uint8_t BSP_SD_Erase (uint64_t StartAddr, uint64_t EndAddr);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
