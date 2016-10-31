// cSdPrivate.h - private interface between cLcd and HAL drivers
#pragma once

#ifdef __cplusplus
  extern "C" {
#endif

SD_HandleTypeDef uSdHandle;

#define BSP_SDMMC_IRQHandler              SDMMC1_IRQHandler
#define BSP_SDMMC_DMA_Tx_IRQHandler       DMA2_Stream6_IRQHandler
#define BSP_SDMMC_DMA_Rx_IRQHandler       DMA2_Stream3_IRQHandler
#define SD_DetectIRQHandler()             HAL_GPIO_EXTI_IRQHandler(SD_DETECT_PIN)

#ifdef __cplusplus
   }
#endif
