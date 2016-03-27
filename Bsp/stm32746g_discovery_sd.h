#pragma once
#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32746g_discovery.h"

#define SD_CardInfo HAL_SD_CardInfoTypedef

#define MSD_OK                        ((uint8_t)0x00)
#define MSD_ERROR                     ((uint8_t)0x01)
#define MSD_ERROR_SD_NOT_PRESENT      ((uint8_t)0x02)

#define SD_PRESENT               ((uint8_t)0x01)
#define SD_NOT_PRESENT           ((uint8_t)0x00)
#define SD_DATATIMEOUT           ((uint32_t)100000000)

/* DMA definitions for SD DMA transfer */
#define __DMAx_TxRx_CLK_ENABLE   __HAL_RCC_DMA2_CLK_ENABLE
#define SD_DMAx_Tx_CHANNEL       DMA_CHANNEL_4
#define SD_DMAx_Rx_CHANNEL       DMA_CHANNEL_4
#define SD_DMAx_Tx_STREAM        DMA2_Stream6
#define SD_DMAx_Rx_STREAM        DMA2_Stream3
#define SD_DMAx_Tx_IRQn          DMA2_Stream6_IRQn
#define SD_DMAx_Rx_IRQn          DMA2_Stream3_IRQn
#define SD_DMAx_Tx_IRQHandler    DMA2_Stream6_IRQHandler
#define SD_DMAx_Rx_IRQHandler    DMA2_Stream3_IRQHandler
#define SD_DetectIRQHandler()    HAL_GPIO_EXTI_IRQHandler(SD_DETECT_PIN)

uint8_t BSP_SD_Init();
uint8_t BSP_SD_DeInit();
uint8_t BSP_SD_ITConfig();
void    BSP_SD_DetectIT();
void    BSP_SD_DetectCallback();
uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint64_t ReadAddr, uint32_t BlockSize, uint32_t NumOfBlocks);
uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint64_t WriteAddr, uint32_t BlockSize, uint32_t NumOfBlocks);
uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint64_t ReadAddr, uint32_t BlockSize, uint32_t NumOfBlocks);
uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint64_t WriteAddr, uint32_t BlockSize, uint32_t NumOfBlocks);
uint8_t BSP_SD_Erase(uint64_t StartAddr, uint64_t EndAddr);
void    BSP_SD_IRQHandler();
void    BSP_SD_DMA_Tx_IRQHandler();
void    BSP_SD_DMA_Rx_IRQHandler();
HAL_SD_TransferStateTypedef BSP_SD_GetStatus();
void    BSP_SD_GetCardInfo(HAL_SD_CardInfoTypedef *CardInfo);
uint8_t BSP_SD_IsDetected();

void    BSP_SD_MspInit(SD_HandleTypeDef *hsd, void *Params);
void    BSP_SD_Detect_MspInit(SD_HandleTypeDef *hsd, void *Params);
void    BSP_SD_MspDeInit(SD_HandleTypeDef *hsd, void *Params);

#ifdef __cplusplus
}
#endif
