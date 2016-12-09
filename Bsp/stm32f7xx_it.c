// stm32f7xx_it.c - BSP handler IRQ to HAL glue
/*{{{  includes*/
#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"

#include "FreeRtos.h"
#include "task.h"

#include "os/ethernetif.h"
#include "cLcdPrivate.h"
#include "cSdPrivate.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery_audio.h"
#else
  #include "stm32f769i_discovery_audio.h"
#endif
/*}}}*/

/*{{{*/
void HardFault_Handler() __attribute__( ( naked ) );
// The prototype shows it is a naked function - in effect this is just an assembly function.

void HardFault_Handler() {
  __asm volatile (
    " tst lr, #4\n"
    " ite eq\n"
    " mrseq r0, msp\n"
    " mrsne r0, psp\n"
    " ldr r1, [r0, #24]\n"
    " ldr r2, handler2_address_const\n"
    " bx r2\n"
    " handler2_address_const: .word getStackRegs\n"
    );
  }
/*}}}*/
/*{{{*/
void getStackRegs (uint32_t* faultStackAddress) {

  volatile uint32_t r0  = faultStackAddress[0];
  volatile uint32_t r1  = faultStackAddress[1];
  volatile uint32_t r2  = faultStackAddress[2];
  volatile uint32_t r3  = faultStackAddress[3];
  volatile uint32_t r12 = faultStackAddress[4];
  volatile uint32_t linkReg = faultStackAddress[5];
  volatile uint32_t programCounter = faultStackAddress[6];
  volatile uint32_t programStatusReg = faultStackAddress[7];

  printf ("\n*** HardFault ***\n");
  printf ("r0   %08lx\n", r0);
  printf ("r1   %08lx\n", r1);
  printf ("r2   %08lx\n", r2);
  printf ("r3   %08lx\n", r3);
  printf ("r12  %08lx\n", r12);
  printf ("r14  %08lx subRetAdd\n", linkReg);
  printf ("pc   %08lx\n", programCounter);
  printf ("psr  %08lx\n", programStatusReg);
  printf ("\n");
  printf ("bfar %08lx\n", SCB->BFAR);
  printf ("cfsr %08lx\n", SCB->CFSR);
  printf ("hfsr %08lx\n", SCB->HFSR);
  printf ("dfsr %08lx\n", SCB->DFSR);
  printf ("afsr %08lx\n", SCB->AFSR);
  printf ("hcsr %08lx\n", SCB->SHCSR);
  printf ("*** breakpoint #0 ***\n");

  __asm("BKPT #0\n") ;
  }
/*}}}*/

//void HardFault_Handler()  { while (1) {} }
void MemManage_Handler()  { while (1) {} }
void BusFault_Handler()   { while (1) {} }
void UsageFault_Handler() { while (1) {} }

extern void xPortSysTickHandler();
void SysTick_Handler() {
  HAL_IncTick();
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    xPortSysTickHandler();
  }

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

// audio out
extern SAI_HandleTypeDef haudio_out_sai;
void AUDIO_OUT_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_out_sai.hdmatx); }

#ifdef STM32F746G_DISCO
  // sd irqs
  void SDMMC1_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
  void DMA2_Stream3_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmarx); }
  void DMA2_Stream6_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmatx); }
  // audio in
  //extern SAI_HandleTypeDef haudio_in_sai;
  //void AUDIO_IN_SAIx_DMAx_IRQHandler() { HAL_DMA_IRQHandler (haudio_in_sai.hdmarx); }
#else
  // uart
  extern UART_HandleTypeDef UartHandle;
  void UART5_IRQHandler();
  void DMA1_Stream0_IRQHandler();
  void DMA1_Stream7_IRQHandler();

  void UART5_IRQHandler() { HAL_UART_IRQHandler (&UartHandle); }
  void DMA1_Stream0_IRQHandler() { HAL_DMA_IRQHandler (UartHandle.hdmarx); }
  void DMA1_Stream7_IRQHandler() { HAL_DMA_IRQHandler (UartHandle.hdmatx); }

  // sd irqs
  void SDMMC2_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
  void DMA2_Stream0_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmarx); }
  void DMA2_Stream5_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmatx); }

  // audio in reuses SD dma channels?
  //extern DFSDM_Filter_HandleTypeDef hAudioInTopLeftFilter;
  //extern DFSDM_Filter_HandleTypeDef hAudioInTopRightFilter;
  //void AUDIO_DFSDMx_DMAx_TOP_LEFT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopLeftFilter.hdmaReg); }
  //void AUDIO_DFSDMx_DMAx_TOP_RIGHT_IRQHandler() { HAL_DMA_IRQHandler (hAudioInTopRightFilter.hdmaReg); }
#endif
