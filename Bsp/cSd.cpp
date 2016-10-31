// cSd.cpp
//{{{  includes
#include "cSd.h"
#include <string.h>
#include "cmsis_os.h"

#include "cLcd.h"
//}}}
#define SD_PRESENT               ((uint8_t)0x01)
#define SD_NOT_PRESENT           ((uint8_t)0x00)

//{{{  SD DMA config
#define __DMAx_TxRx_CLK_ENABLE  __HAL_RCC_DMA2_CLK_ENABLE

#define SD_DMAx_Rx_CHANNEL      DMA_CHANNEL_4
#define SD_DMAx_Tx_CHANNEL      DMA_CHANNEL_4

#define SD_DMAx_Rx_STREAM       DMA2_Stream3
#define SD_DMAx_Tx_STREAM       DMA2_Stream6

#define SD_DMAx_Rx_IRQn         DMA2_Stream3_IRQn
#define SD_DMAx_Tx_IRQn         DMA2_Stream6_IRQn
//}}}
SD_HandleTypeDef uSdHandle;
//{{{  static vars
static SD_CardInfo uSdCardInfo;
static DMA_HandleTypeDef dma_rx_handle;
static DMA_HandleTypeDef dma_tx_handle;

static const uint32_t sdReadCacheSize = 0x40;
static uint8_t* mSdReadCache = 0;
static uint32_t mSdReadCacheBlock = 0xFFFFFFB0;
static uint32_t sdReads = 0;
static uint32_t sdReadHits = 0;
static uint32_t sdReadMultipleLen = 0;
static uint32_t sdReadBlock = 0xFFFFFFFF;

static uint32_t sdWrites = 0;
static uint32_t sdWriteMultipleLen = 0;
static uint32_t sdWriteBlock = 0xFFFFFFFF;
//}}}

//{{{
uint8_t SD_Init() {

  // uSD device interface configuration
  uSdHandle.Instance = SDMMC1;
  uSdHandle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
  uSdHandle.Init.ClockBypass         = SDMMC_CLOCK_BYPASS_DISABLE;
  uSdHandle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  uSdHandle.Init.BusWide             = SDMMC_BUS_WIDE_1B;
  uSdHandle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  uSdHandle.Init.ClockDiv            = SDMMC_TRANSFER_CLK_DIV;

  //{{{  sdDetect init
  SD_DETECT_GPIO_CLK_ENABLE();

  GPIO_InitTypeDef  gpio_init_structure;
  gpio_init_structure.Pin       = SD_DETECT_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_INPUT;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  HAL_GPIO_Init (SD_DETECT_GPIO_PORT, &gpio_init_structure);
  //}}}
  if (!SD_present())
    return MSD_ERROR_SD_NOT_PRESENT;

  __HAL_RCC_SDMMC1_CLK_ENABLE();
  __DMAx_TxRx_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  //{{{  gpio init
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  gpio_init_structure.Alternate = GPIO_AF12_SDMMC1;

  gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  HAL_GPIO_Init(GPIOC, &gpio_init_structure);

  gpio_init_structure.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);
  //}}}
  //{{{  DMA Rx parameters
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
  __HAL_LINKDMA (&uSdHandle, hdmarx, dma_rx_handle);
  HAL_DMA_DeInit (&dma_rx_handle);
  HAL_DMA_Init (&dma_rx_handle);
  //}}}
  //{{{  DMA Tx parameters
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
  __HAL_LINKDMA (&uSdHandle, hdmatx, dma_tx_handle);
  HAL_DMA_DeInit (&dma_tx_handle);
  HAL_DMA_Init (&dma_tx_handle);
  //}}}

  // sd interrupt
  HAL_NVIC_SetPriority (SDMMC1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ (SDMMC1_IRQn);

  // sd rx DMA transfer complete interrupt
  HAL_NVIC_SetPriority (SD_DMAx_Rx_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ (SD_DMAx_Rx_IRQn);

  // sd tx DMA transfer complete interrupt
  HAL_NVIC_SetPriority (SD_DMAx_Tx_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ (SD_DMAx_Tx_IRQn);

  // HAL SD initialization
  if (HAL_SD_Init (&uSdHandle, &uSdCardInfo) != SD_OK)
    return MSD_ERROR;
  if (HAL_SD_WideBusOperation_Config (&uSdHandle, SDMMC_BUS_WIDE_4B) != SD_OK)
    return MSD_ERROR;
  if (HAL_SD_HighSpeed (&uSdHandle) != SD_OK)
    return MSD_ERROR;

  mSdReadCache = (uint8_t*)pvPortMalloc (512 * sdReadCacheSize);

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
uint8_t SD_IsDetected() {
  return HAL_GPIO_ReadPin (SD_DETECT_GPIO_PORT, SD_DETECT_PIN) == GPIO_PIN_SET ? SD_NOT_PRESENT : SD_PRESENT;
  }
//}}}
bool SD_present() { return SD_IsDetected() == SD_PRESENT; }

HAL_SD_TransferStateTypedef SD_GetStatus() { return HAL_SD_GetStatus (&uSdHandle); }
void SD_GetCardInfo (HAL_SD_CardInfoTypedef* CardInfo) { HAL_SD_Get_CardInfo (&uSdHandle, CardInfo); }

//{{{
std::string SD_info() {
  return cLcd::dec (sdReads) + ":" + cLcd::dec (sdReadHits) + "  "  +
         cLcd::dec (sdReadBlock + sdReadMultipleLen) + " w:" + cLcd::dec (sdWrites);
  }
//}}}

//{{{
uint8_t SD_ReadBlocks (uint32_t* pData, uint64_t ReadAddr, uint16_t blocks) {

  if (HAL_SD_ReadBlocks_DMA (&uSdHandle, pData, ReadAddr, blocks) != SD_OK)
    return MSD_ERROR;
  SCB_InvalidateDCache_by_Addr ((uint32_t*)((uint32_t)pData & 0xFFFFFFE0), (blocks * 512) + 32);

  return MSD_OK;
  }
//}}}
//{{{
uint8_t SD_WriteBlocks (uint32_t* pData, uint64_t WriteAddr, uint16_t blocks) {

  if (HAL_SD_WriteBlocks_DMA (&uSdHandle, pData, WriteAddr, blocks) != SD_OK)
    return MSD_ERROR;

  //if (HAL_SD_CheckWriteOperation (&uSdHandle, (uint32_t)SD_DATATIMEOUT) != SD_OK)
  //  return MSD_ERROR;
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

//{{{
int8_t SD_IsReady (uint8_t lun) {
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
int8_t SD_Read (uint8_t* buf, uint32_t blk_addr, uint16_t blocks) {

  if (SD_present()) {
    //SD_ReadBlocks ((uint32_t*)buf, blk_addr * 512, blocks);

    if ((blk_addr >= mSdReadCacheBlock) && (blk_addr + blocks <= mSdReadCacheBlock + sdReadCacheSize)) {
      sdReadHits++;
      memcpy (buf, mSdReadCache + ((blk_addr - mSdReadCacheBlock) * 512), blocks * 512);
      }
    else {
      sdReads++;
      SD_ReadBlocks ((uint32_t*)mSdReadCache, blk_addr * 512, sdReadCacheSize);
      memcpy (buf, mSdReadCache, blocks * 512);
      mSdReadCacheBlock = blk_addr;
      }

    //cLcd::debug ("r:" + cLcd::dec (blk_addr) + "::" + cLcd::dec (blk_len));
    if (blk_addr != sdReadBlock + sdReadMultipleLen) {
      if (sdReadMultipleLen) {
        // flush pending multiple
        cLcd::debug ("rm:" + cLcd::dec (sdReadBlock) + "::" + cLcd::dec (sdReadMultipleLen));
        sdReadMultipleLen = 0;
        }
      sdReadBlock = blk_addr;
      }
    sdReadMultipleLen += blocks;

    return 0;
    }

  return -1;
  }
//}}}
//{{{
int8_t SD_Write (uint8_t* buf, uint32_t blk_addr, uint16_t blocks) {

  if (SD_present()) {
    sdWrites++;
    SD_WriteBlocks ((uint32_t*)buf, blk_addr * 512, blocks);

    mSdReadCacheBlock = 0xFFFFFFF0;

    //cLcd::debug ("w " + cLcd::dec (blk_addr) + " " + cLcd::dec (blocks));
    if (blk_addr != sdWriteBlock + sdWriteMultipleLen) {
      if (sdWriteMultipleLen) {
        // flush pending multiple
        cLcd::debug ("wm:" + cLcd::dec (sdWriteBlock) + "::" + cLcd::dec (sdWriteMultipleLen));
        sdWriteMultipleLen = 0;
        }
      sdWriteBlock = blk_addr;
      }
    sdWriteMultipleLen += blocks;

    return 0;
    }

  return -1;
  }
//}}}
