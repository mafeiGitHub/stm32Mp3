// stm32f7xx_it.c  18 November-2015
#include "stm32f7xx_hal.h"
#include "stm32f7xx_it.h"

#include "cmsis_os.h"
#include "ethernetif.h"

#include "stm32746g_discovery_lcd.h"
extern LTDC_HandleTypeDef hLtdc;

#include "stm32746g_discovery_audio.h"
extern SAI_HandleTypeDef haudio_in_sai;
extern SAI_HandleTypeDef haudio_out_sai;


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

void LTDC_IRQHandler() { LCD_LTDC_IRQHandler(); }
void DMA2D_IRQHandler() { LCD_DMA2D_IRQHandler(); }

void DMA2_Stream3_IRQHandler() { BSP_SD_DMA_Rx_IRQHandler(); }
void DMA2_Stream6_IRQHandler() { BSP_SD_DMA_Tx_IRQHandler(); }
void SDMMC1_IRQHandler() { BSP_SD_IRQHandler(); }
