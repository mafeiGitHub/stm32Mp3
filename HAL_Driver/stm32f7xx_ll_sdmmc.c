/*{{{*/
/**
  ******************************************************************************
  * @file    stm32f7xx_ll_sdmmc.c
  * @author  MCD Application Team
  * @version V1.1.2
  * @date    23-September-2016
  * @brief   SDMMC Low Layer HAL module driver.
  *
  *          This file provides firmware functions to manage the following
  *          functionalities of the SDMMC peripheral:
  *           + Initialization/de-initialization functions
  *           + I/O operation functions
  *           + Peripheral Control functions
  *           + Peripheral State functions
  *
  @verbatim
  ==============================================================================
                       ##### SDMMC peripheral features #####
  ==============================================================================
    [..] The SD/SDMMC MMC card host interface (SDMMC) provides an interface between the APB2
         peripheral bus and MultiMedia cards (MMCs), SD memory cards, SDMMC cards and CE-ATA
         devices.

    [..] The SDMMC features include the following:
         (+) Full compliance with MultiMedia Card System Specification Version 4.2. Card support
             for three different databus modes: 1-bit (default), 4-bit and 8-bit
         (+) Full compatibility with previous versions of MultiMedia Cards (forward compatibility)
         (+) Full compliance with SD Memory Card Specifications Version 2.0
         (+) Full compliance with SD I/O Card Specification Version 2.0: card support for two
             different data bus modes: 1-bit (default) and 4-bit
         (+) Full support of the CE-ATA features (full compliance with CE-ATA digital protocol
             Rev1.1)
         (+) Data transfer up to 48 MHz for the 8 bit mode
         (+) Data and command output enable signals to control external bidirectional drivers.


                           ##### How to use this driver #####
  ==============================================================================
    [..]
      This driver is a considered as a driver of service for external devices drivers
      that interfaces with the SDMMC peripheral.
      According to the device used (SD card/ MMC card / SDMMC card ...), a set of APIs
      is used in the device's driver to perform SDMMC operations and functionalities.

      This driver is almost transparent for the final user, it is only used to implement other
      functionalities of the external device.

    [..]
      (+) The SDMMC clock (SDMMCCLK = 48 MHz) is coming from a specific output of PLL
          (PLL48CLK). Before start working with SDMMC peripheral make sure that the
          PLL is well configured.
          The SDMMC peripheral uses two clock signals:
          (++) SDMMC adapter clock (SDMMCCLK = 48 MHz)
          (++) APB2 bus clock (PCLK2)

          -@@- PCLK2 and SDMMC_CK clock frequencies must respect the following condition:
               Frequency(PCLK2) >= (3 / 8 x Frequency(SDMMC_CK))

      (+) Enable/Disable peripheral clock using RCC peripheral macros related to SDMMC
          peripheral.

      (+) Enable the Power ON State using the SDMMC_PowerState_ON(SDMMCx)
          function and disable it using the function SDMMC_PowerState_OFF(SDMMCx).

      (+) Enable/Disable the clock using the __SDMMC_ENABLE()/__SDMMC_DISABLE() macros.

      (+) Enable/Disable the peripheral interrupts using the macros __SDMMC_ENABLE_IT(hSDMMC, IT)
          and __SDMMC_DISABLE_IT(hSDMMC, IT) if you need to use interrupt mode.

      (+) When using the DMA mode
          (++) Configure the DMA in the MSP layer of the external device
          (++) Active the needed channel Request
          (++) Enable the DMA using __SDMMC_DMA_ENABLE() macro or Disable it using the macro
               __SDMMC_DMA_DISABLE().

      (+) To control the CPSM (Command Path State Machine) and send
          commands to the card use the SDMMC_SendCommand(SDMMCx),
          SDMMC_GetCommandResponse() and SDMMC_GetResponse() functions. First, user has
          to fill the command structure (pointer to SDMMC_CmdInitTypeDef) according
          to the selected command to be sent.
          The parameters that should be filled are:
           (++) Command Argument
           (++) Command Index
           (++) Command Response type
           (++) Command Wait
           (++) CPSM Status (Enable or Disable).

          -@@- To check if the command is well received, read the SDMMC_CMDRESP
              register using the SDMMC_GetCommandResponse().
              The SDMMC responses registers (SDMMC_RESP1 to SDMMC_RESP2), use the
              SDMMC_GetResponse() function.

      (+) To control the DPSM (Data Path State Machine) and send/receive
           data to/from the card use the SDMMC_DataConfig(), SDMMC_GetDataCounter(),
          SDMMC_ReadFIFO(), DIO_WriteFIFO() and SDMMC_GetFIFOCount() functions.

    *** Read Operations ***
    =======================
    [..]
      (#) First, user has to fill the data structure (pointer to
          SDMMC_DataInitTypeDef) according to the selected data type to be received.
          The parameters that should be filled are:
           (++) Data TimeOut
           (++) Data Length
           (++) Data Block size
           (++) Data Transfer direction: should be from card (To SDMMC)
           (++) Data Transfer mode
           (++) DPSM Status (Enable or Disable)

      (#) Configure the SDMMC resources to receive the data from the card
          according to selected transfer mode (Refer to Step 8, 9 and 10).

      (#) Send the selected Read command (refer to step 11).

      (#) Use the SDMMC flags/interrupts to check the transfer status.

    *** Write Operations ***
    ========================
    [..]
     (#) First, user has to fill the data structure (pointer to
         SDMMC_DataInitTypeDef) according to the selected data type to be received.
         The parameters that should be filled are:
          (++) Data TimeOut
          (++) Data Length
          (++) Data Block size
          (++) Data Transfer direction:  should be to card (To CARD)
          (++) Data Transfer mode
          (++) DPSM Status (Enable or Disable)

     (#) Configure the SDMMC resources to send the data to the card according to
         selected transfer mode.

     (#) Send the selected Write command.

     (#) Use the SDMMC flags/interrupts to check the transfer status.

  @endverbatim
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/*}}}*/
#include "stm32f7xx_hal.h"

#if defined (HAL_SD_MODULE_ENABLED) || defined(HAL_MMC_MODULE_ENABLED)

/*{{{*/
HAL_StatusTypeDef SDMMC_Init(SDMMC_TypeDef *SDMMCx, SDMMC_InitTypeDef Init)
{
  uint32_t tmpreg = 0;

  /* Check the parameters */
  assert_param(IS_SDMMC_ALL_INSTANCE(SDMMCx));
  assert_param(IS_SDMMC_CLOCK_EDGE(Init.ClockEdge));
  assert_param(IS_SDMMC_CLOCK_BYPASS(Init.ClockBypass));
  assert_param(IS_SDMMC_CLOCK_POWER_SAVE(Init.ClockPowerSave));
  assert_param(IS_SDMMC_BUS_WIDE(Init.BusWide));
  assert_param(IS_SDMMC_HARDWARE_FLOW_CONTROL(Init.HardwareFlowControl));
  assert_param(IS_SDMMC_CLKDIV(Init.ClockDiv));

  /* Set SDMMC configuration parameters */
  tmpreg |= (Init.ClockEdge           |\
             Init.ClockBypass         |\
             Init.ClockPowerSave      |\
             Init.BusWide             |\
             Init.HardwareFlowControl |\
             Init.ClockDiv
             );

  /* Write to SDMMC CLKCR */
  MODIFY_REG(SDMMCx->CLKCR, CLKCR_CLEAR_MASK, tmpreg);

  return HAL_OK;
}
/*}}}*/

uint32_t SDMMC_GetDataCounter(SDMMC_TypeDef *SDMMCx) { return (SDMMCx->DCOUNT); }
uint32_t SDMMC_GetFIFOCount(SDMMC_TypeDef *SDMMCx) { return (SDMMCx->FIFO); }

uint32_t SDMMC_ReadFIFO (SDMMC_TypeDef *SDMMCx) { return (SDMMCx->FIFO); }
/*{{{*/
HAL_StatusTypeDef SDMMC_WriteFIFO (SDMMC_TypeDef *SDMMCx, uint32_t *pWriteData) {
  SDMMCx->FIFO = *pWriteData;
  return HAL_OK;
  }
/*}}}*/

/*{{{*/
uint32_t SDMMC_GetPowerState(SDMMC_TypeDef *SDMMCx)
{
  return (SDMMCx->POWER & SDMMC_POWER_PWRCTRL);
}
/*}}}*/
/*{{{*/
HAL_StatusTypeDef SDMMC_PowerState_ON(SDMMC_TypeDef *SDMMCx)
{
  /* Set power state to ON */
  SDMMCx->POWER = SDMMC_POWER_PWRCTRL;
  return HAL_OK;
}
/*}}}*/
/*{{{*/
HAL_StatusTypeDef SDMMC_PowerState_OFF(SDMMC_TypeDef *SDMMCx)
{
  /* Set power state to OFF */
  SDMMCx->POWER = (uint32_t)0x00000000;
  return HAL_OK;
}
/*}}}*/

/*{{{*/
HAL_StatusTypeDef SDMMC_SendCommand(SDMMC_TypeDef *SDMMCx, SDMMC_CmdInitTypeDef *Command)
{
  uint32_t tmpreg = 0;

  /* Check the parameters */
  assert_param(IS_SDMMC_CMD_INDEX(Command->CmdIndex));
  assert_param(IS_SDMMC_RESPONSE(Command->Response));
  assert_param(IS_SDMMC_WAIT(Command->WaitForInterrupt));
  assert_param(IS_SDMMC_CPSM(Command->CPSM));

  /* Set the SDMMC Argument value */
  SDMMCx->ARG = Command->Argument;

  /* Set SDMMC command parameters */
  tmpreg |= (uint32_t)(Command->CmdIndex | Command->Response | Command->WaitForInterrupt | Command->CPSM);

  /* Write to SDMMC CMD register */
  MODIFY_REG(SDMMCx->CMD, CMD_CLEAR_MASK, tmpreg);

  return HAL_OK;
}
/*}}}*/

uint8_t SDMMC_GetCommandResponse(SDMMC_TypeDef *SDMMCx) { return (uint8_t)(SDMMCx->RESPCMD); }
/*{{{*/
uint32_t SDMMC_GetResponse(SDMMC_TypeDef *SDMMCx, uint32_t Response)
{
  __IO uint32_t tmp = 0;

  /* Check the parameters */
  assert_param(IS_SDMMC_RESP(Response));

  /* Get the response */
  tmp = (uint32_t)&(SDMMCx->RESP1) + Response;

  return (*(__IO uint32_t *) tmp);
}
/*}}}*/

/*{{{*/
HAL_StatusTypeDef SDMMC_DataConfig(SDMMC_TypeDef *SDMMCx, SDMMC_DataInitTypeDef* Data)
{
  uint32_t tmpreg = 0;

  /* Check the parameters */
  assert_param(IS_SDMMC_DATA_LENGTH(Data->DataLength));
  assert_param(IS_SDMMC_BLOCK_SIZE(Data->DataBlockSize));
  assert_param(IS_SDMMC_TRANSFER_DIR(Data->TransferDir));
  assert_param(IS_SDMMC_TRANSFER_MODE(Data->TransferMode));
  assert_param(IS_SDMMC_DPSM(Data->DPSM));

  SDMMCx->DTIMER = Data->DataTimeOut;
  SDMMCx->DLEN = Data->DataLength;

  /* Set the SDMMC data configuration parameters */
  tmpreg |= (uint32_t)(Data->DataBlockSize | Data->TransferDir   | Data->TransferMode  | Data->DPSM);

  /* Write to SDMMC DCTRL */
  MODIFY_REG(SDMMCx->DCTRL, DCTRL_CLEAR_MASK, tmpreg);

  return HAL_OK;

}
/*}}}*/

/*{{{*/
HAL_StatusTypeDef SDMMC_SetSDMMCReadWaitMode(SDMMC_TypeDef *SDMMCx, uint32_t SDMMC_ReadWaitMode)
{
  /* Check the parameters */
  assert_param(IS_SDMMC_READWAIT_MODE(SDMMC_ReadWaitMode));

  /* Set SDMMC read wait mode */
  MODIFY_REG(SDMMCx->DCTRL, SDMMC_DCTRL_RWMOD, SDMMC_ReadWaitMode);

  return HAL_OK;
}
/*}}}*/
#endif
