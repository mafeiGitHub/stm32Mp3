// stm32f7xx_it.c - BSP HAL glue
#include "stm32f7xx_it.h"

#include "stm32f7xx_hal.h"
#include "stm32746g_discovery_sd.h"
#include "stm32746g_discovery_audio.h"

#include "cmsis_os.h"
#include "ethernetif.h"

#include "cLcdPrivate.h"

extern SAI_HandleTypeDef haudio_in_sai;
extern SAI_HandleTypeDef haudio_out_sai;
extern DFSDM_Filter_HandleTypeDef hAudioInTopLeftFilter;
extern DFSDM_Filter_HandleTypeDef hAudioInTopRightFilter;

extern SD_HandleTypeDef uSdHandle;
extern DMA_HandleTypeDef dma_rx_handle;
extern DMA_HandleTypeDef dma_tx_handle;

void NMI_Handler() {}
void DebugMon_Handler() {}

void HardFault_Handler()  { while (1) {} }
void MemManage_Handler()  { while (1) {} }
void BusFault_Handler()   { while (1) {} }
void UsageFault_Handler() { while (1) {} }

void SysTick_Handler() { HAL_IncTick(); osSystickHandler(); }

void ETH_IRQHandler() { ETHERNET_IRQHandler(); }

void AUDIO_IN_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_in_sai.hdmarx); }
void AUDIO_OUT_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_out_sai.hdmatx); }
void AUDIO_DFSDMx_DMAx_TOP_LEFT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopLeftFilter.hdmaReg); }
void AUDIO_DFSDMx_DMAx_TOP_RIGHT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopRightFilter.hdmaReg); }


void LTDC_IRQHandler() { LCD_LTDC_IRQHandler(); }
void DMA2D_IRQHandler() { LCD_DMA2D_IRQHandler(); }

// 746g disco sd
void DMA2_Stream3_IRQHandler() { HAL_DMA_IRQHandler (&dma_rx_handle); }
void DMA2_Stream6_IRQHandler() { HAL_DMA_IRQHandler (&dma_tx_handle); }
void SDMMC1_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }

// 769i disco sd
void DMA2_Stream0_IRQHandler() { HAL_DMA_IRQHandler (&dma_rx_handle); }
void DMA2_Stream5_IRQHandler() { HAL_DMA_IRQHandler (&dma_tx_handle); }
void SDMMC2_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
