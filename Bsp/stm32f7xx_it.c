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
  __asm volatile
    (
    " tst lr, #4                                                \n"
    " ite eq                                                    \n"
    " mrseq r0, msp                                             \n"
    " mrsne r0, psp                                             \n"
    " ldr r1, [r0, #24]                                         \n"
    " ldr r2, handler2_address_const                            \n"
    " bx r2                                                     \n"
    " handler2_address_const: .word getRegistersFromStack    \n"
    );
  }
/*}}}*/
/*{{{*/
void getRegistersFromStack (uint32_t* faultStackAddress) {
// These are volatile to try and prevent the compiler/linker optimising them
// away as the variables never actually get used.  If the debugger won't show the
// values of the variables, make them global my moving their declaration outside of this function.

  volatile uint32_t r0  = faultStackAddress[0];
  volatile uint32_t r1  = faultStackAddress[1];
  volatile uint32_t r2  = faultStackAddress[2];
  volatile uint32_t r3  = faultStackAddress[3];
  volatile uint32_t r12 = faultStackAddress[4];
  volatile uint32_t lr  = faultStackAddress[5]; // Link register
  volatile uint32_t pc  = faultStackAddress[6]; // Program counter
  volatile uint32_t psr = faultStackAddress[7]; // Program status register

  printf ("\n");
  printf ("R0 = %x\n", r0);
  printf ("R1 = %x\n", r1);
  printf ("R2 = %x\n", r2);
  printf ("R3 = %x\n", r3);

  printf ("R12 = %x\n", r12);
  printf ("LR [R14] = %x  subroutine call return address\n", lr);
  printf ("PC [R15] = %x  program counter\n", pc);
  printf ("PSR = %x\n", psr);

  printf ("BFAR = %x\n", (*((volatile unsigned long *)(0xE000ED38))));
  printf ("CFSR = %x\n", (*((volatile unsigned long *)(0xE000ED28))));
  printf ("HFSR = %x\n", (*((volatile unsigned long *)(0xE000ED2C))));
  printf ("DFSR = %x\n", (*((volatile unsigned long *)(0xE000ED30))));
  printf ("AFSR = %x\n", (*((volatile unsigned long *)(0xE000ED3C))));
  printf ("SCB_SHCSR = %x\n", SCB->SHCSR);

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
