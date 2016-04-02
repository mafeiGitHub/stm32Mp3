/*{{{*/
/**
  ******************************************************************************
  * @file    stm32746g_discovery_sd.c
      - This driver is used to drive the micro SD external card mounted on STM32746G-Discovery
        board.
      - This driver does not need a specific component driver for the micro SD device
        to be included with.
     + Initialization steps:
        o Initialize the micro SD card using the BSP_SD_Init() function. This
          function includes the MSP layer hardware resources initialization and the
          SDIO interface configuration to interface with the external micro SD. It
          also includes the micro SD initialization sequence.
        o To check the SD card presence you can use the function BSP_SD_IsDetected() which
          returns the detection status
        o If SD presence detection interrupt mode is desired, you must configure the
          SD detection interrupt mode by calling the function BSP_SD_ITConfig(). The interrupt
          is generated as an external interrupt whenever the micro SD card is
          plugged/unplugged in/from the board.
        o The function BSP_SD_GetCardInfo() is used to get the micro SD card information
          which is stored in the structure "HAL_SD_CardInfoTypedef".

     + Micro SD card operations
        o The micro SD card can be accessed with read/write block(s) operations once
          it is ready for access. The access can be performed whether using the polling
          mode by calling the functions BSP_SD_ReadBlocks()/BSP_SD_WriteBlocks(), or by DMA
          transfer using the functions BSP_SD_ReadBlocks_DMA()/BSP_SD_WriteBlocks_DMA()
        o The DMA transfer complete is used with interrupt mode. Once the SD transfer
          is complete, the SD interrupt is handled using the function BSP_SD_IRQHandler(),
          the DMA Tx/Rx transfer complete are handled using the functions
          BSP_SD_DMA_Tx_IRQHandler()/BSP_SD_DMA_Rx_IRQHandler(). The corresponding user callbacks
          are implemented by the user at application level.
        o The SD erase block(s) is performed using the function BSP_SD_Erase() with specifying
          the number of blocks to erase.
        o The SD runtime status is returned when calling the function BSP_SD_GetStatus().
*/
/*}}}*/
#include "stm32746g_discovery_sd.h"

static SD_CardInfo uSdCardInfo;
static SD_HandleTypeDef uSdHandle;
static DMA_HandleTypeDef dma_rx_handle;
static DMA_HandleTypeDef dma_tx_handle;

void BSP_SD_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
void BSP_SD_DMA_Tx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmatx); }
void BSP_SD_DMA_Rx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmarx); }
HAL_SD_TransferStateTypedef BSP_SD_GetStatus() { return(HAL_SD_GetStatus(&uSdHandle)); }
void BSP_SD_GetCardInfo (HAL_SD_CardInfoTypedef *CardInfo) { HAL_SD_Get_CardInfo(&uSdHandle, CardInfo); }

/*{{{*/
uint8_t BSP_SD_Init() {

  uint8_t sd_state = MSD_OK;

  /* uSD device interface configuration */
  uSdHandle.Instance = SDMMC1;

  uSdHandle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
  uSdHandle.Init.ClockBypass         = SDMMC_CLOCK_BYPASS_DISABLE;
  uSdHandle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  uSdHandle.Init.BusWide             = SDMMC_BUS_WIDE_1B;
  uSdHandle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  uSdHandle.Init.ClockDiv            = SDMMC_TRANSFER_CLK_DIV;

  /* Msp SD Detect pin initialization */
  BSP_SD_Detect_MspInit (&uSdHandle, NULL);
  if(BSP_SD_IsDetected() != SD_PRESENT)   /* Check if SD card is present */
    return MSD_ERROR_SD_NOT_PRESENT;

  /* Msp SD initialization */
  BSP_SD_MspInit (&uSdHandle, NULL);

  /* HAL SD initialization */
  if (HAL_SD_Init(&uSdHandle, &uSdCardInfo) != SD_OK)
    sd_state = MSD_ERROR;

  /* Configure SD Bus width */
  if (sd_state == MSD_OK) {
    /* Enable wide operation */
    if (HAL_SD_WideBusOperation_Config(&uSdHandle, SDMMC_BUS_WIDE_4B) != SD_OK)
      sd_state = MSD_ERROR;
    else
      sd_state = MSD_OK;
    }

  return  sd_state;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_SD_DeInit() {

  uint8_t sd_state = MSD_OK;

  uSdHandle.Instance = SDMMC1;

  /* HAL SD deinitialization */
  if(HAL_SD_DeInit(&uSdHandle) != HAL_OK)
    sd_state = MSD_ERROR;

  /* Msp SD deinitialization */
  uSdHandle.Instance = SDMMC1;
  BSP_SD_MspDeInit(&uSdHandle, NULL);
  return  sd_state;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_SD_ITConfig()
{
  GPIO_InitTypeDef gpio_init_structure;

  /* Configure Interrupt mode for SD detection pin */
  gpio_init_structure.Pin = SD_DETECT_PIN;
  gpio_init_structure.Pull = GPIO_PULLUP;
  gpio_init_structure.Speed = GPIO_SPEED_FAST;
  gpio_init_structure.Mode = GPIO_MODE_IT_RISING_FALLING;
  HAL_GPIO_Init (SD_DETECT_GPIO_PORT, &gpio_init_structure);

  /* Enable and set SD detect EXTI Interrupt to the lowest priority */
  HAL_NVIC_SetPriority ((IRQn_Type)(SD_DETECT_EXTI_IRQn), 0x0F, 0x00);
  HAL_NVIC_EnableIRQ ((IRQn_Type)(SD_DETECT_EXTI_IRQn));

  return MSD_OK;
}
/*}}}*/
/*{{{*/
uint8_t BSP_SD_IsDetected() {

  /* Check SD card detect pin */
  return HAL_GPIO_ReadPin(SD_DETECT_GPIO_PORT, SD_DETECT_PIN) == GPIO_PIN_SET ? SD_NOT_PRESENT : SD_PRESENT;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_SD_ReadBlocks (uint32_t* pData, uint64_t ReadAddr, uint32_t BlockSize, uint32_t NumOfBlocks) {

  return HAL_SD_ReadBlocks (&uSdHandle, pData, ReadAddr, BlockSize, NumOfBlocks) == SD_OK ? MSD_OK : MSD_ERROR;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_SD_WriteBlocks (uint32_t* pData, uint64_t WriteAddr, uint32_t BlockSize, uint32_t NumOfBlocks) {

  return HAL_SD_WriteBlocks(&uSdHandle, pData, WriteAddr, BlockSize, NumOfBlocks) != SD_OK ? MSD_ERROR : MSD_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_SD_ReadBlocks_DMA (uint32_t* pData, uint64_t ReadAddr, uint32_t BlockSize, uint32_t NumOfBlocks) {

  if (HAL_SD_ReadBlocks_DMA (&uSdHandle, pData, ReadAddr, BlockSize, NumOfBlocks) != SD_OK)
    return MSD_ERROR;
  else
    return HAL_SD_CheckReadOperation (&uSdHandle, (uint32_t)SD_DATATIMEOUT) == SD_OK ? MSD_OK : MSD_ERROR;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_SD_WriteBlocks_DMA (uint32_t* pData, uint64_t WriteAddr, uint32_t BlockSize, uint32_t NumOfBlocks) {

  if (HAL_SD_WriteBlocks_DMA(&uSdHandle, pData, WriteAddr, BlockSize, NumOfBlocks) != SD_OK)
    return MSD_ERROR;
  else
    return HAL_SD_CheckWriteOperation(&uSdHandle, (uint32_t)SD_DATATIMEOUT) == SD_OK ? MSD_OK : MSD_ERROR;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_SD_Erase (uint64_t StartAddr, uint64_t EndAddr) {

  return HAL_SD_Erase(&uSdHandle, StartAddr, EndAddr) == SD_OK ? MSD_OK : MSD_ERROR;
  }
/*}}}*/

/*{{{*/
__weak void BSP_SD_MspInit (SD_HandleTypeDef* hsd, void* Params) {

  GPIO_InitTypeDef gpio_init_structure;

  /* Enable SDIO clock */
  __HAL_RCC_SDMMC1_CLK_ENABLE();

  /* Enable DMA2 clocks */
  __DMAx_TxRx_CLK_ENABLE();

  /* Enable GPIOs clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* Common GPIO configuration */
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  gpio_init_structure.Alternate = GPIO_AF12_SDMMC1;

  /* GPIOC configuration */
  gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  HAL_GPIO_Init(GPIOC, &gpio_init_structure);

  /* GPIOD configuration */
  gpio_init_structure.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);

  /* NVIC configuration for SDIO interrupts */
  HAL_NVIC_SetPriority(SDMMC1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(SDMMC1_IRQn);

  /* Configure DMA Rx parameters */
  dma_rx_handle.Init.Channel             = SD_DMAx_Rx_CHANNEL;
  dma_rx_handle.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  dma_rx_handle.Init.PeriphInc           = DMA_PINC_DISABLE;
  dma_rx_handle.Init.MemInc              = DMA_MINC_ENABLE;
  dma_rx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_rx_handle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  dma_rx_handle.Init.Mode                = DMA_PFCTRL;
  dma_rx_handle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  dma_rx_handle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  dma_rx_handle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  dma_rx_handle.Init.MemBurst            = DMA_MBURST_INC4;
  dma_rx_handle.Init.PeriphBurst         = DMA_PBURST_INC4;

  dma_rx_handle.Instance = SD_DMAx_Rx_STREAM;

  /* Associate the DMA handle */
  __HAL_LINKDMA(hsd, hdmarx, dma_rx_handle);

  /* Deinitialize the stream for new transfer */
  HAL_DMA_DeInit(&dma_rx_handle);

  /* Configure the DMA stream */
  HAL_DMA_Init(&dma_rx_handle);

  /* Configure DMA Tx parameters */
  dma_tx_handle.Init.Channel             = SD_DMAx_Tx_CHANNEL;
  dma_tx_handle.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  dma_tx_handle.Init.PeriphInc           = DMA_PINC_DISABLE;
  dma_tx_handle.Init.MemInc              = DMA_MINC_ENABLE;
  dma_tx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_tx_handle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  dma_tx_handle.Init.Mode                = DMA_PFCTRL;
  dma_tx_handle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  dma_tx_handle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  dma_tx_handle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  dma_tx_handle.Init.MemBurst            = DMA_MBURST_INC4;
  dma_tx_handle.Init.PeriphBurst         = DMA_PBURST_INC4;

  dma_tx_handle.Instance = SD_DMAx_Tx_STREAM;

  /* Associate the DMA handle */
  __HAL_LINKDMA(hsd, hdmatx, dma_tx_handle);

  /* Deinitialize the stream for new transfer */
  HAL_DMA_DeInit(&dma_tx_handle);

  /* Configure the DMA stream */
  HAL_DMA_Init(&dma_tx_handle);

  /* NVIC configuration for DMA transfer complete interrupt */
  HAL_NVIC_SetPriority(SD_DMAx_Rx_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SD_DMAx_Rx_IRQn);

  /* NVIC configuration for DMA transfer complete interrupt */
  HAL_NVIC_SetPriority(SD_DMAx_Tx_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SD_DMAx_Tx_IRQn);
}
/*}}}*/
/*{{{*/
__weak void BSP_SD_Detect_MspInit (SD_HandleTypeDef* hsd, void* Params) {

  GPIO_InitTypeDef  gpio_init_structure;

  SD_DETECT_GPIO_CLK_ENABLE();

  /* GPIO configuration in input for uSD_Detect signal */
  gpio_init_structure.Pin       = SD_DETECT_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_INPUT;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &gpio_init_structure);
  }
/*}}}*/
/*{{{*/
__weak void BSP_SD_MspDeInit (SD_HandleTypeDef* hsd, void* Params) {

  /* Disable NVIC for DMA transfer complete interrupts */
  HAL_NVIC_DisableIRQ(SD_DMAx_Rx_IRQn);
  HAL_NVIC_DisableIRQ(SD_DMAx_Tx_IRQn);

  /* Deinitialize the stream for new transfer */
  dma_rx_handle.Instance = SD_DMAx_Rx_STREAM;
  HAL_DMA_DeInit(&dma_rx_handle);

  /* Deinitialize the stream for new transfer */
  dma_tx_handle.Instance = SD_DMAx_Tx_STREAM;
  HAL_DMA_DeInit(&dma_tx_handle);

  /* Disable NVIC for SDIO interrupts */
  HAL_NVIC_DisableIRQ(SDMMC1_IRQn);

  /* DeInit GPIO pins can be done in the application
     (by surcharging this __weak function) */

  /* Disable SDMMC1 clock */
  __HAL_RCC_SDMMC1_CLK_DISABLE();

  /* GPIO pins clock and DMA clocks can be shut down in the application
     by surcharging this __weak function */
}
/*}}}*/
