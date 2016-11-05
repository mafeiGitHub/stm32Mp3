// cSdPrivate.h - private interface between cLcd and HAL drivers
#pragma once

#ifdef __cplusplus
  extern "C" {
#endif

SD_HandleTypeDef uSdHandle;

#ifdef STM32F746G_DISCO
  #define BSP_SDMMC_IRQHandler              SDMMC1_IRQHandler
  #define BSP_SDMMC_DMA_Rx_IRQHandler       DMA2_Stream3_IRQHandler
  #define BSP_SDMMC_DMA_Tx_IRQHandler       DMA2_Stream6_IRQHandler
#else
  #define BSP_SDMMC_IRQHandler              SDMMC1_IRQHandler
  #define BSP_SDMMC_DMA_Rx_IRQHandler       DMA2_Stream0_IRQHandler
  #define BSP_SDMMC_DMA_Tx_IRQHandler       DMA2_Stream5_IRQHandler
#endif

#ifdef __cplusplus
   }
#endif
