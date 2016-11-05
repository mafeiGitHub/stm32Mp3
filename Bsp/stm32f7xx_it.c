// stm32f7xx_it.c - BSP handler IRQ to HAL glue
/*{{{  includes*/
#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"

#include "cmsis_os.h"
#include "os/ethernetif.h"
#include "cLcdPrivate.h"
#include "cSdPrivate.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery_audio.h"
#else
  #include "stm32f769i_discovery_audio.h"
#endif
/*}}}*/

void HardFault_Handler()  { while (1) {} }
void MemManage_Handler()  { while (1) {} }
void BusFault_Handler()   { while (1) {} }
void UsageFault_Handler() { while (1) {} }

void SysTick_Handler() { HAL_IncTick(); osSystickHandler(); }

// usb
extern PCD_HandleTypeDef hpcd;
void OTG_FS_IRQHandler() { HAL_PCD_IRQHandler (&hpcd); }
void OTG_HS_IRQHandler() { HAL_PCD_IRQHandler (&hpcd); }

// ethernet
extern ETH_HandleTypeDef EthHandle;
void ETH_IRQHandler() { HAL_ETH_IRQHandler (&EthHandle); }

// QSPI
extern QSPI_HandleTypeDef QSPIHandle;
void QUADSPI_IRQHandler() { HAL_QSPI_IRQHandler (&QSPIHandle); }
// lcd
void LTDC_IRQHandler() { LCD_LTDC_IRQHandler(); }
void DMA2D_IRQHandler() { LCD_DMA2D_IRQHandler(); }

// audio
extern SAI_HandleTypeDef haudio_out_sai;
void AUDIO_OUT_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_out_sai.hdmatx); }

// audio in
#ifdef STM32F746G_DISCO
  void SDMMC1_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
  void BSP_SDMMC_DMA_Rx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmarx); }
  void BSP_SDMMC_DMA_Tx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmatx); }
  //extern SAI_HandleTypeDef haudio_in_sai;
  //void AUDIO_IN_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_in_sai.hdmarx); }
#else
  void SDMMC2_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
  void BSP_SDMMC_DMA_Rx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmarx); }
  void BSP_SDMMC_DMA_Tx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmatx); }
  // audio in - reuses SD dma channels?
  //extern DFSDM_Filter_HandleTypeDef hAudioInTopLeftFilter;
  //extern DFSDM_Filter_HandleTypeDef hAudioInTopRightFilter;
  //void AUDIO_DFSDMx_DMAx_TOP_LEFT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopLeftFilter.hdmaReg); }
  //void AUDIO_DFSDMx_DMAx_TOP_RIGHT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopRightFilter.hdmaReg); }
#endif
