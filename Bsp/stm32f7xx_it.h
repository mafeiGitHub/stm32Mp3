// stm32f7xx_it.h version V1.0.2  18-November-2015
#pragma once

#include "stm32f7xx.h"

void NMI_Handler();
void HardFault_Handler();
void MemManage_Handler();
void BusFault_Handler();
void UsageFault_Handler();
void SVC_Handler();
void DebugMon_Handler();
void PendSV_Handler();
void SysTick_Handler();

void ETH_IRQHandler();

void AUDIO_OUT_SAIx_DMAx_IRQHandler();
void AUDIO_IN_SAIx_DMAx_IRQHandler();

void LTDC_IRQHandler();
void DMA2D_IRQHandler();

void DMA2_Stream3_IRQHandler();
void DMA2_Stream6_IRQHandler();
void SDMMC1_IRQHandler();
