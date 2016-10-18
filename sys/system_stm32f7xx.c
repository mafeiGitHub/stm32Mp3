// system_stm32f7xx.c
#include "stm32f7xx.h"

//#define VECT_TAB_SRAM
#define VECT_TAB_OFFSET 0x00 // Vector Table base offset field, This value must be a multiple of 0x200

#define HSE_VALUE ((uint32_t)25000000) // Default value of the External oscillator in Hz
#define HSI_VALUE ((uint32_t)16000000) // Value of the Internal oscillator in Hz

uint32_t SystemCoreClock = 16000000;
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8] = {0, 0, 0, 0, 1, 2, 3, 4};

/*{{{*/
void SystemSDram8Mb() {

  register uint32_t tmpreg = 0, timeout = 0xFFFF;
  register __IO uint32_t index;

  /* Enable GPIOC, GPIOD, GPIOE, GPIOF, GPIOG and GPIOH interface clock */
  RCC->AHB1ENR |= 0x000000FC;

  /* Connect PCx pins to FMC Alternate function */
  GPIOC->AFR[0]  = 0x0000C000;
  GPIOC->AFR[1]  = 0x00000000;
  /* Configure PCx pins in Alternate function mode */
  GPIOC->MODER   = 0x00000080;
  /* Configure PCx pins speed to 50 MHz */
  GPIOC->OSPEEDR = 0x00000080;
  /* Configure PCx pins Output type to push-pull */
  GPIOC->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PCx pins */
  GPIOC->PUPDR   = 0x00000040;

  /* Connect PDx pins to FMC Alternate function */
  GPIOD->AFR[0]  = 0x0000C0CC;
  GPIOD->AFR[1]  = 0xCC000CCC;
  /* Configure PDx pins in Alternate function mode */
  GPIOD->MODER   = 0xA02A008A;
  /* Configure PDx pins speed to 50 MHz */
  GPIOD->OSPEEDR = 0xA02A008A;
  /* Configure PDx pins Output type to push-pull */
  GPIOD->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PDx pins */
  GPIOD->PUPDR   = 0x50150045;

  /* Connect PEx pins to FMC Alternate function */
  GPIOE->AFR[0]  = 0xC00000CC;
  GPIOE->AFR[1]  = 0xCCCCCCCC;
  /* Configure PEx pins in Alternate function mode */
  GPIOE->MODER   = 0xAAAA800A;
  /* Configure PEx pins speed to 50 MHz */
  GPIOE->OSPEEDR = 0xAAAA800A;
  /* Configure PEx pins Output type to push-pull */
  GPIOE->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PEx pins */
  GPIOE->PUPDR   = 0x55554005;

  /* Connect PFx pins to FMC Alternate function */
  GPIOF->AFR[0]  = 0x00CCCCCC;
  GPIOF->AFR[1]  = 0xCCCCC000;
  /* Configure PFx pins in Alternate function mode */
  GPIOF->MODER   = 0xAA800AAA;
  /* Configure PFx pins speed to 50 MHz */
  GPIOF->OSPEEDR = 0xAA800AAA;
  /* Configure PFx pins Output type to push-pull */
  GPIOF->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PFx pins */
  GPIOF->PUPDR   = 0x55400555;

  /* Connect PGx pins to FMC Alternate function */
  GPIOG->AFR[0]  = 0x00CC00CC;
  GPIOG->AFR[1]  = 0xC000000C;
  /* Configure PGx pins in Alternate function mode */
  GPIOG->MODER   = 0x80020A0A;
  /* Configure PGx pins speed to 50 MHz */
  GPIOG->OSPEEDR = 0x80020A0A;
  /* Configure PGx pins Output type to push-pull */
  GPIOG->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PGx pins */
  GPIOG->PUPDR   = 0x40010505;

  /* Connect PHx pins to FMC Alternate function */
  GPIOH->AFR[0]  = 0x00C0C000;
  GPIOH->AFR[1]  = 0x00000000;
  /* Configure PHx pins in Alternate function mode */
  GPIOH->MODER   = 0x00000880;
  /* Configure PHx pins speed to 50 MHz */
  GPIOH->OSPEEDR = 0x00000880;
  /* Configure PHx pins Output type to push-pull */
  GPIOH->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PHx pins */
  GPIOH->PUPDR   = 0x00000440;

  /* Enable the FMC interface clock */
  RCC->AHB3ENR |= 0x00000001;

  /* Configure and enable SDRAM bank1 */
  FMC_Bank5_6->SDCR[0]  = 0x00001954;
  FMC_Bank5_6->SDTR[0]  = 0x01115351;

  /* SDRAM initialization sequence */
  /* Clock enable command */
  FMC_Bank5_6->SDCMR = 0x00000011;
  tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
  while((tmpreg != 0) && (timeout-- > 0))
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;

  /* Delay */
  for (index = 0; index<1000; index++);

  /* PALL command */
  FMC_Bank5_6->SDCMR = 0x00000012;
  timeout = 0xFFFF;
  while((tmpreg != 0) && (timeout-- > 0))
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;

  /* Auto refresh command */
  FMC_Bank5_6->SDCMR = 0x000000F3;
  timeout = 0xFFFF;
  while((tmpreg != 0) && (timeout-- > 0))
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;

  /* MRD register program */
  FMC_Bank5_6->SDCMR = 0x00044014;
  timeout = 0xFFFF;
  while((tmpreg != 0) && (timeout-- > 0))
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;

  /* Set refresh count */
  tmpreg = FMC_Bank5_6->SDRTR;
  FMC_Bank5_6->SDRTR = (tmpreg | (0x0000050C<<1));

  /* Disable write protection */
  tmpreg = FMC_Bank5_6->SDCR[0];
  FMC_Bank5_6->SDCR[0] = (tmpreg & 0xFFFFFDFF);
  }
/*}}}*/
/*{{{*/
void SystemSDram16Mb() {

  register uint32_t tmpreg = 0, timeout = 0xFFFF;
  register __IO uint32_t index;

  /* Enable GPIOD, GPIOE, GPIOF, GPIOG, GPIOH and GPIOI interface clock */
  RCC->AHB1ENR |= 0x000001F8;

  /* Connect PDx pins to FMC Alternate function */
  GPIOD->AFR[0]  = 0x000000CC;
  GPIOD->AFR[1]  = 0xCC000CCC;
  /* Configure PDx pins in Alternate function mode */
  GPIOD->MODER   = 0xA02A000A;
  /* Configure PDx pins speed to 100 MHz */
  GPIOD->OSPEEDR = 0xF03F000F;
  /* Configure PDx pins Output type to push-pull */
  GPIOD->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PDx pins */
  GPIOD->PUPDR   = 0x50150005;

  /* Connect PEx pins to FMC Alternate function */
  GPIOE->AFR[0]  = 0xC00000CC;
  GPIOE->AFR[1]  = 0xCCCCCCCC;
  /* Configure PEx pins in Alternate function mode */
  GPIOE->MODER   = 0xAAAA800A;
  /* Configure PEx pins speed to 100 MHz */
  GPIOE->OSPEEDR = 0xFFFFC00F;
  /* Configure PEx pins Output type to push-pull */
  GPIOE->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PEx pins */
  GPIOE->PUPDR   = 0x55554005;

  /* Connect PFx pins to FMC Alternate function */
  GPIOF->AFR[0]  = 0x00CCCCCC;
  GPIOF->AFR[1]  = 0xCCCCC000;
  /* Configure PFx pins in Alternate function mode */
  GPIOF->MODER   = 0xAA800AAA;
  /* Configure PFx pins speed to 100 MHz */
  GPIOF->OSPEEDR = 0xFFC00FFF;
  /* Configure PFx pins Output type to push-pull */
  GPIOF->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PFx pins */
  GPIOF->PUPDR   = 0x55400555;

  /* Connect PGx pins to FMC Alternate function */
  GPIOG->AFR[0]  = 0x00CC0CCC;
  GPIOG->AFR[1]  = 0xC000000C;
  /* Configure PGx pins in Alternate function mode */
  GPIOG->MODER   = 0x80020A2A;
  /* Configure PGx pins speed to 100 MHz */
  GPIOG->OSPEEDR = 0xC0030F3F;
  /* Configure PGx pins Output type to push-pull */
  GPIOG->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PGx pins */
  GPIOG->PUPDR   = 0x40010515;

  /* Connect PHx pins to FMC Alternate function */
  GPIOH->AFR[0]  = 0x00C0CC00;
  GPIOH->AFR[1]  = 0xCCCCCCCC;
  /* Configure PHx pins in Alternate function mode */
  GPIOH->MODER   = 0xAAAA08A0;
  /* Configure PHx pins speed to 100 MHz */
  GPIOH->OSPEEDR = 0xFFFF0CF0;
  /* Configure PHx pins Output type to push-pull */
  GPIOH->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PHx pins */
  GPIOH->PUPDR   = 0x55550450;

  /* Connect PIx pins to FMC Alternate function */
  GPIOI->AFR[0]  = 0xCCCCCCCC;
  GPIOI->AFR[1]  = 0x00000CC0;
  /* Configure PIx pins in Alternate function mode */
  GPIOI->MODER   = 0x0028AAAA;
  /* Configure PIx pins speed to 100 MHz */
  GPIOI->OSPEEDR = 0x003CFFFF;
  /* Configure PIx pins Output type to push-pull */
  GPIOI->OTYPER  = 0x00000000;
  /* No pull-up, pull-down for PIx pins */
  GPIOI->PUPDR   = 0x00145555;

  /* Enable the FMC interface clock */
  RCC->AHB3ENR |= 0x00000001;

  /* Configure and enable SDRAM bank1 */
  FMC_Bank5_6->SDCR[0]  = 0x000019E4;
  FMC_Bank5_6->SDTR[0]  = 0x01116361;

  /* SDRAM initialization sequence */
  /* Clock enable command */
  FMC_Bank5_6->SDCMR = 0x00000011;
  tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
  while((tmpreg != 0) && (timeout-- > 0))
  {
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
  }

  /* Delay */
  for (index = 0; index<1000; index++);

  /* PALL command */
  FMC_Bank5_6->SDCMR = 0x00000012;
  timeout = 0xFFFF;
  while((tmpreg != 0) && (timeout-- > 0))
  {
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
  }

  /* Auto refresh command */
  FMC_Bank5_6->SDCMR = 0x000000F3;
  timeout = 0xFFFF;
  while((tmpreg != 0) && (timeout-- > 0))
  {
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
  }

  /* MRD register program */
  FMC_Bank5_6->SDCMR = 0x00046014;
  timeout = 0xFFFF;
  while((tmpreg != 0) && (timeout-- > 0))
  {
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
  }

  /* Set refresh count */
  tmpreg = FMC_Bank5_6->SDRTR;
  FMC_Bank5_6->SDRTR = (tmpreg | (0x00000603<<1));

  /* Disable write protection */
  tmpreg = FMC_Bank5_6->SDCR[0];
  FMC_Bank5_6->SDCR[0] = (tmpreg & 0xFFFFFDFF);

  /*
   * Disable the FMC bank1 (enabled after reset).
   * This, prevents CPU speculation access on this bank which blocks the use of FMC during
   * 24us. During this time the others FMC master (such as LTDC) cannot use it!
   */
  FMC_Bank1->BTCR[0] = 0x000030d2;
}
/*}}}*/

/*{{{*/
void SystemCoreClockUpdate() {
/*{{{*/
/**
   * @brief  Update SystemCoreClock variable according to Clock Register Values.
  *         The SystemCoreClock variable contains the core clock (HCLK), it can
  *         be used by the user application to setup the SysTick timer or configure
  *         other parameters.
  *
  * @note   Each time the core clock (HCLK) changes, this function must be called
  *         to update SystemCoreClock variable value. Otherwise, any configuration
  *         based on this variable will be incorrect.
  *
  * @note   - The system frequency computed by this function is not the real
  *           frequency in the chip. It is calculated based on the predefined
  *           constant and the selected clock source:
  *
  *           - If SYSCLK source is HSI, SystemCoreClock will contain the HSI_VALUE(*)
  *
  *           - If SYSCLK source is HSE, SystemCoreClock will contain the HSE_VALUE(**)
  *
  *           - If SYSCLK source is PLL, SystemCoreClock will contain the HSE_VALUE(**)
  *             or HSI_VALUE(*) multiplied/divided by the PLL factors.
  *
  *         (*) HSI_VALUE is a constant defined in stm32f7xx.h file (default value
  *             16 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.
  *
  *         (**) HSE_VALUE is a constant defined in stm32f7xx.h file (default value
  *              25 MHz), user has to ensure that HSE_VALUE is same as the real
  *              frequency of the crystal used. Otherwise, this function may
  *              have wrong result.
  *
  *         - The result of this function could be not correct when using fractional
  *           value for HSE crystal.
  *
  * @param  None
  * @retval None
  */
/*}}}*/

  // Get SYSCLK source
  uint32_t tmp = RCC->CFGR & RCC_CFGR_SWS;

  switch (tmp) {
    case 0x00:  // HSI used as system clock source
      SystemCoreClock = HSI_VALUE;
      break;

    case 0x04:  // HSE used as system clock source
      SystemCoreClock = HSE_VALUE;
      break;

    case 0x08: { // PLL used as system clock source
      // PLL_VCO = (HSE_VALUE or HSI_VALUE / PLL_M) * PLL_N
      // SYSCLK = PLL_VCO / PLL_P
      uint32_t pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 22;
      uint32_t pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;

      uint32_t pllvco;
      if (pllsource != 0) // HSE used as PLL clock source
        pllvco = (HSE_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
      else // HSI used as PLL clock source */
        pllvco = (HSI_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);

      uint32_t pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >>16) + 1 ) *2;
      SystemCoreClock = pllvco / pllp;
      break;
      }

    default:
      SystemCoreClock = HSI_VALUE;
      break;
    }

  // Compute HCLK frequency. Get HCLK prescaler
  tmp = AHBPrescTable [((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];

  // HCLK frequency
  SystemCoreClock >>= tmp;
  }
/*}}}*/
/*{{{*/
void SystemInit() {

  // FPU settings - set CP10 and CP11 Full Access
  SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));

  // Reset the RCC clock configuration to the default reset state - Set HSION bit
  RCC->CR |= (uint32_t)0x00000001;

  // - Reset CFGR register
  RCC->CFGR = 0x00000000;

  // - Reset HSEON, CSSON and PLLON bits
  RCC->CR &= (uint32_t)0xFEF6FFFF;

  // - Reset PLLCFGR register
  RCC->PLLCFGR = 0x24003010;

  // - Reset HSEBYP bit
  RCC->CR &= (uint32_t)0xFFFBFFFF;

  // Disable all interrupts
  RCC->CIR = 0x00000000;

  #ifdef STM32F746G_DISCO
    SystemSDram8Mb();
  #endif
  #ifdef STM32F769I_DISCO
    SystemSDram16Mb();
  #endif

  // Configure the Vector Table location add offset address
#ifdef VECT_TAB_SRAM
  SCB->VTOR = SRAM1_BASE | VECT_TAB_OFFSET; // Vector Table Relocation in Internal SRAM
#else
  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; // Vector Table Relocation in Internal FLASH
#endif
}
/*}}}*/
