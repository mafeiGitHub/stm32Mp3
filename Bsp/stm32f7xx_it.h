// stm32f7xx_it.h - BSP HAL glue
#pragma once

#include "stm32f7xx.h"

void NMI_Handler() {}
void DebugMon_Handler() {}

void HardFault_Handler();
void MemManage_Handler();
void BusFault_Handler();
void UsageFault_Handler();
void SVC_Handler();
void PendSV_Handler();
void SysTick_Handler();

void ETH_IRQHandler();

void LTDC_IRQHandler();
void DMA2D_IRQHandler();

void AUDIO_OUT_SAIx_DMAx_IRQHandler();
void AUDIO_IN_SAIx_DMAx_IRQHandler();
void AUDIO_DFSDMx_DMAx_TOP_LEFT_IRQHandler();
void AUDIO_DFSDMx_DMAx_TOP_RIGHT_IRQHandler();

void SDMMC1_IRQHandler();
void DMA2_Stream3_IRQHandler();
void DMA2_Stream6_IRQHandler();

void SDMMC2_IRQHandler();
void DMA2_Stream0_IRQHandler();
void DMA2_Stream5_IRQHandler();
