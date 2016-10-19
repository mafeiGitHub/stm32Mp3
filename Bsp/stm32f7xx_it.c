// stm32f7xx_it.c - BSP handler IRQ to HAL glue
#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"

#include "cmsis_os.h"
#include "ethernetif.h"
#include "cLcdPrivate.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery_sd.h"
  #include "stm32746g_discovery_audio.h"
#else
  #include "stm32f769i_discovery_sd.h"
  #include "stm32f769i_discovery_audio.h"
#endif

void HardFault_Handler()  { while (1) {} }
void MemManage_Handler()  { while (1) {} }
void BusFault_Handler()   { while (1) {} }
void UsageFault_Handler() { while (1) {} }

void SysTick_Handler() { HAL_IncTick(); osSystickHandler(); }

void ETH_IRQHandler() { ETHERNET_IRQHandler(); }

// lcd
void LTDC_IRQHandler() { LCD_LTDC_IRQHandler(); }
void DMA2D_IRQHandler() { LCD_DMA2D_IRQHandler(); }

extern SAI_HandleTypeDef haudio_out_sai;
void AUDIO_OUT_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_out_sai.hdmatx); }

extern SD_HandleTypeDef uSdHandle;
extern DMA_HandleTypeDef dma_rx_handle;
extern DMA_HandleTypeDef dma_tx_handle;
#ifdef STM32F746G_DISCO
  extern SAI_HandleTypeDef haudio_in_sai;
  void AUDIO_IN_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_in_sai.hdmarx); }
  void SDMMC1_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
  void DMA2_Stream3_IRQHandler() { HAL_DMA_IRQHandler (&dma_rx_handle); }
  void DMA2_Stream6_IRQHandler() { HAL_DMA_IRQHandler (&dma_tx_handle); }
#else
  // reuses SD dma channels?
  extern DFSDM_Filter_HandleTypeDef hAudioInTopLeftFilter;
  extern DFSDM_Filter_HandleTypeDef hAudioInTopRightFilter;
  //void AUDIO_DFSDMx_DMAx_TOP_LEFT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopLeftFilter.hdmaReg); }
  //void AUDIO_DFSDMx_DMAx_TOP_RIGHT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopRightFilter.hdmaReg); }
  void SDMMC2_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
  void DMA2_Stream0_IRQHandler() { HAL_DMA_IRQHandler (&dma_rx_handle); }
  void DMA2_Stream5_IRQHandler() { HAL_DMA_IRQHandler (&dma_tx_handle); }
#endif
