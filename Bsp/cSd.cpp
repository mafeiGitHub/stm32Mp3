// cSd.cpp
//{{{  includes
#include "cSd.h"
#include <string.h>
#include "freertos.h"

#include "utils.h"
#include "cLcd.h"
//}}}

SD_HandleTypeDef uSdHandle;
//{{{  static vars
static HAL_SD_CardInfoTypedef uSdCardInfo;
static DMA_HandleTypeDef dma_rx_handle;
static DMA_HandleTypeDef dma_tx_handle;

static const uint32_t mReadCacheSize = 0x40;
static uint8_t* mReadCache = 0;
static uint32_t mReadCacheBlock = 0xFFFFFFB0;
static uint32_t mReads = 0;
static uint32_t mReadHits = 0;
static uint32_t mReadMultipleLen = 0;
static uint32_t mReadBlock = 0xFFFFFFFF;

static uint32_t mWrites = 0;
static uint32_t mWriteMultipleLen = 0;
static uint32_t mWriteBlock = 0xFFFFFFFF;
//}}}

//osMutexId mSdMutex;
//if ((mSdMutex, 1000) == osOK)) {
//osMutexRelease (mSdMutex);

//{{{
uint8_t SD_Init() {

  //{{{  sdDetect init
  SD_DETECT_GPIO_CLK_ENABLE();

  GPIO_InitTypeDef  gpio_init_structure;
  gpio_init_structure.Pin       = SD_DETECT_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_INPUT;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  HAL_GPIO_Init (SD_DETECT_GPIO_PORT, &gpio_init_structure);
  //}}}
  uSdHandle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
  uSdHandle.Init.ClockBypass         = SDMMC_CLOCK_BYPASS_DISABLE;
  uSdHandle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
   uSdHandle.Init.BusWide             = SDMMC_BUS_WIDE_1B;
  uSdHandle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  uSdHandle.Init.ClockDiv            = SDMMC_TRANSFER_CLK_DIV;

   __HAL_RCC_DMA2_CLK_ENABLE();

  // sd interrupt
  #ifdef STM32F746G_DISCO
    uSdHandle.Instance = SDMMC1;
    __HAL_RCC_SDMMC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    //{{{  gpio init
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Pull      = GPIO_PULLUP;
    gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
    gpio_init_structure.Alternate = GPIO_AF12_SDMMC1;

    gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init (GPIOC, &gpio_init_structure);

    gpio_init_structure.Pin = GPIO_PIN_2;
    HAL_GPIO_Init (GPIOD, &gpio_init_structure);
    //}}}
    //{{{  DMA rx parameters
    dma_rx_handle.Instance                 = DMA2_Stream3;
    dma_rx_handle.Init.Channel             = DMA_CHANNEL_4;
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

    __HAL_LINKDMA (&uSdHandle, hdmarx, dma_rx_handle);
    HAL_DMA_DeInit (&dma_rx_handle);
    HAL_DMA_Init (&dma_rx_handle);
    //}}}
    //{{{  DMA tx parameters
    dma_tx_handle.Instance                 = DMA2_Stream6;
    dma_tx_handle.Init.Channel             = DMA_CHANNEL_4;
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

    __HAL_LINKDMA (&uSdHandle, hdmatx, dma_tx_handle);
    HAL_DMA_DeInit (&dma_tx_handle);
    HAL_DMA_Init (&dma_tx_handle);
    //}}}
    HAL_NVIC_SetPriority (SDMMC1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ (SDMMC1_IRQn);
    HAL_NVIC_SetPriority (DMA2_Stream3_IRQn, 6, 0);  // f for 769
    HAL_NVIC_EnableIRQ (DMA2_Stream3_IRQn);
    HAL_NVIC_SetPriority (DMA2_Stream6_IRQn, 6, 0);  // f for 769
    HAL_NVIC_EnableIRQ (DMA2_Stream6_IRQn);
  #else
    uSdHandle.Instance = SDMMC2;
    __HAL_RCC_SDMMC2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    //{{{  gpio init
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
    gpio_init_structure.Pull      = GPIO_PULLUP;
    gpio_init_structure.Speed     = GPIO_SPEED_HIGH;

    gpio_init_structure.Alternate = GPIO_AF10_SDMMC2;
    gpio_init_structure.Pin = GPIO_PIN_3 | GPIO_PIN_4;
    HAL_GPIO_Init (GPIOB, &gpio_init_structure);

    gpio_init_structure.Alternate = GPIO_AF11_SDMMC2;
    gpio_init_structure.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init (GPIOD, &gpio_init_structure);

    gpio_init_structure.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init (GPIOG, &gpio_init_structure);
    //}}}
    //{{{  DMA rx parameters
    dma_rx_handle.Instance                 = DMA2_Stream0;
    dma_rx_handle.Init.Channel             = DMA_CHANNEL_11;
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

    __HAL_LINKDMA (&uSdHandle, hdmarx, dma_rx_handle);
    HAL_DMA_DeInit (&dma_rx_handle);
    HAL_DMA_Init (&dma_rx_handle);
    //}}}
    //{{{  DMA tx parameters
    dma_tx_handle.Instance                 = DMA2_Stream5;
    dma_tx_handle.Init.Channel             = DMA_CHANNEL_11;
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

    __HAL_LINKDMA (&uSdHandle, hdmatx, dma_tx_handle);
    HAL_DMA_DeInit (&dma_tx_handle);
    HAL_DMA_Init (&dma_tx_handle);
    //}}}
    HAL_NVIC_SetPriority (SDMMC2_IRQn, 0x5, 0);  //e
    HAL_NVIC_EnableIRQ (SDMMC2_IRQn);
    HAL_NVIC_SetPriority (DMA2_Stream0_IRQn, 0x6, 0); //f
    HAL_NVIC_EnableIRQ (DMA2_Stream0_IRQn);
    HAL_NVIC_SetPriority (DMA2_Stream5_IRQn, 0x6, 0);  // f
    HAL_NVIC_EnableIRQ (DMA2_Stream5_IRQn);
  #endif

  // HAL SD initialization
  if (HAL_SD_Init (&uSdHandle, &uSdCardInfo) != SD_OK)
    return MSD_ERROR;
  if (HAL_SD_WideBusOperation_Config (&uSdHandle, SDMMC_BUS_WIDE_4B) != SD_OK)
    return MSD_ERROR;
  if (HAL_SD_HighSpeed (&uSdHandle) != SD_OK)
    return MSD_ERROR;

  //osMutexDef (sdMutex);
  //mSdMutex = osMutexCreate (osMutex (sdMutex));

  mReadCache = (uint8_t*)pvPortMalloc (512 * mReadCacheSize);

  return MSD_OK;
  }
//}}}

//{{{
uint8_t SD_ITConfig() {

  GPIO_InitTypeDef gpio_init_structure;
  gpio_init_structure.Pin = SD_DETECT_PIN;
  gpio_init_structure.Pull = GPIO_PULLUP;
  gpio_init_structure.Speed = GPIO_SPEED_FAST;
  gpio_init_structure.Mode = GPIO_MODE_IT_RISING_FALLING;
  HAL_GPIO_Init (SD_DETECT_GPIO_PORT, &gpio_init_structure);

  // Enable and set SD detect EXTI Interrupt to the lowest priority */
  HAL_NVIC_SetPriority ((IRQn_Type)(SD_DETECT_EXTI_IRQn), 0x0F, 0x00);
  HAL_NVIC_EnableIRQ ((IRQn_Type)(SD_DETECT_EXTI_IRQn));

  return MSD_OK;
  }
//}}}

//{{{
bool SD_present() {
  return !(SD_DETECT_GPIO_PORT->IDR & SD_DETECT_PIN);
  }
//}}}
//{{{
HAL_SD_TransferStateTypedef SD_GetStatus() {
  return HAL_SD_GetStatus (&uSdHandle);
  }
//}}}
//{{{
void SD_GetCardInfo (HAL_SD_CardInfoTypedef* CardInfo) {
  HAL_SD_Get_CardInfo (&uSdHandle, CardInfo);
  }
//}}}
//{{{
std::string SD_info() {
  return "r:" + dec (mReadHits) + ":" + dec (mReads) + ":"  + dec (mReadBlock + mReadMultipleLen) +
         " w:" + dec (mWrites);
  }
//}}}

//{{{
uint8_t SD_Read (uint8_t* buf, uint32_t blk_addr, uint16_t blocks) {

  if (HAL_SD_ReadBlocks (&uSdHandle, (uint32_t*)buf, blk_addr * 512, blocks) != SD_OK)
    return MSD_ERROR;
  SCB_InvalidateDCache_by_Addr ((uint32_t*)((uint32_t)buf & 0xFFFFFFE0), (blocks * 512) + 32);

  return MSD_OK;
  }
//}}}
//{{{
uint8_t SD_Write (uint8_t* buf, uint32_t blk_addr, uint16_t blocks) {

  if (HAL_SD_WriteBlocks (&uSdHandle, (uint32_t*)buf, blk_addr * 512, blocks) != SD_OK)
    return MSD_ERROR;
  //can't remove ?
  HAL_SD_CheckWriteOperation (&uSdHandle, 0xFFFFFFFF);

  return MSD_OK;
  }
//}}}

//{{{
uint8_t SD_Erase (uint64_t StartAddr, uint64_t EndAddr) {
  if (HAL_SD_Erase (&uSdHandle, StartAddr, EndAddr) != SD_OK)
    return MSD_ERROR;
  else
    return MSD_OK;
  }
//}}}

// usb MSC interface
//{{{
int8_t SD_IsReady() {
  return (SD_present() && (HAL_SD_GetStatus (&uSdHandle) == SD_TRANSFER_OK)) ? 0 : -1;
  }
//}}}
//{{{
int8_t SD_GetCapacity (uint32_t* block_num, uint16_t* block_size) {

  if (SD_present()) {
    HAL_SD_CardInfoTypedef info;
    HAL_SD_Get_CardInfo (&uSdHandle, &info);
    *block_num = (info.CardCapacity) / 512 - 1;
    *block_size = 512;
    return 0;
    }

  return -1;
  }
//}}}
//{{{
int8_t SD_ReadCached (uint8_t* buf, uint32_t blk_addr, uint16_t blocks) {

  if (SD_present()) {
    if ((blk_addr >= mReadCacheBlock) && (blk_addr + blocks <= mReadCacheBlock + mReadCacheSize)) {
      mReadHits++;
      memcpy (buf, mReadCache + ((blk_addr - mReadCacheBlock) * 512), blocks * 512);
      }
    else {
      mReads++;
      SD_Read (mReadCache, blk_addr, mReadCacheSize);
      memcpy (buf, mReadCache, blocks * 512);
      mReadCacheBlock = blk_addr;
      }

    if (blk_addr != mReadBlock + mReadMultipleLen) {
      if (mReadMultipleLen) {
        // flush pending multiple
        //cLcd::debug ("rm:" + dec (mReadBlock) + "::" + dec (mReadMultipleLen));
        mReadMultipleLen = 0;
        }
      mReadBlock = blk_addr;
      }
    mReadMultipleLen += blocks;
    return 0;
    }

  return -1;
  }
//}}}
//{{{
int8_t SD_WriteCached (uint8_t* buf, uint32_t blk_addr, uint16_t blocks) {

  if (SD_present()) {
    mWrites++;
    SD_Write (buf, blk_addr, blocks);

    mReadCacheBlock = 0xFFFFFFF0;
    if (blk_addr != mWriteBlock + mWriteMultipleLen) {
      if (mWriteMultipleLen) {
        // flush pending multiple
        //cLcd::debug ("wm:" + dec (mWriteBlock) + "::" + dec (mWriteMultipleLen));
        mWriteMultipleLen = 0;
        }
      mWriteBlock = blk_addr;
      }
    mWriteMultipleLen += blocks;

    return 0;
    }

  return -1;
  }
//}}}
