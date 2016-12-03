#pragma once
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}
//{{{  includes
#include "stm32f7xx_hal.h"
#include "mx25l512.h"
//}}}
//{{{  defines
/* QSPI Error codes */
#define QSPI_OK            ((uint8_t)0x00)
#define QSPI_ERROR         ((uint8_t)0x01)
#define QSPI_BUSY          ((uint8_t)0x02)
#define QSPI_NOT_SUPPORTED ((uint8_t)0x04)
#define QSPI_SUSPENDED     ((uint8_t)0x08)


/* Definition for QSPI clock resources */
#define QSPI_CLK_ENABLE()          __HAL_RCC_QSPI_CLK_ENABLE()
#define QSPI_CLK_DISABLE()         __HAL_RCC_QSPI_CLK_DISABLE()
#define QSPI_CS_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()
#define QSPI_CLK_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define QSPI_D0_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()
#define QSPI_D1_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()
#define QSPI_D2_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOE_CLK_ENABLE()
#define QSPI_D3_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()

#define QSPI_FORCE_RESET()         __HAL_RCC_QSPI_FORCE_RESET()
#define QSPI_RELEASE_RESET()       __HAL_RCC_QSPI_RELEASE_RESET()

/* Definition for QSPI Pins */
/* QSPI_CS */
#define QSPI_CS_PIN                GPIO_PIN_6
#define QSPI_CS_GPIO_PORT          GPIOB
#define QSPI_CS_PIN_AF             GPIO_AF10_QUADSPI
/* QSPI_CLK */
#define QSPI_CLK_PIN               GPIO_PIN_2
#define QSPI_CLK_GPIO_PORT         GPIOB
#define QSPI_CLK_PIN_AF            GPIO_AF9_QUADSPI
/* QSPI_D0 */
#define QSPI_D0_PIN                GPIO_PIN_9
#define QSPI_D0_GPIO_PORT          GPIOC
#define QSPI_D0_PIN_AF             GPIO_AF9_QUADSPI
/* QSPI_D1 */
#define QSPI_D1_PIN                GPIO_PIN_10
#define QSPI_D1_GPIO_PORT          GPIOC
#define QSPI_D1_PIN_AF             GPIO_AF9_QUADSPI
/* QSPI_D2 */
#define QSPI_D2_PIN                GPIO_PIN_2
#define QSPI_D2_GPIO_PORT          GPIOE
#define QSPI_D2_PIN_AF             GPIO_AF9_QUADSPI
/* QSPI_D3 */
#define QSPI_D3_PIN                GPIO_PIN_13
#define QSPI_D3_GPIO_PORT          GPIOD
#define QSPI_D3_PIN_AF             GPIO_AF9_QUADSPI
//}}}

typedef struct {
  uint32_t FlashSize;          /*!< Size of the flash */
  uint32_t EraseSectorSize;    /*!< Size of sectors for the erase operation */
  uint32_t EraseSectorsNumber; /*!< Number of sectors for the erase operation */
  uint32_t ProgPageSize;       /*!< Size of pages for the program operation */
  uint32_t ProgPagesNumber;    /*!< Number of pages for the program operation */
  } QSPI_Info;

uint8_t BSP_QSPI_Init();
uint8_t BSP_QSPI_DeInit();

uint8_t BSP_QSPI_Read (uint8_t* pData, uint32_t ReadAddr, uint32_t Size);
uint8_t BSP_QSPI_Write (uint8_t* pData, uint32_t WriteAddr, uint32_t Size);

uint8_t BSP_QSPI_Erase_Block(uint32_t BlockAddress);
uint8_t BSP_QSPI_Erase_Chip();

uint8_t BSP_QSPI_GetStatus();
uint8_t BSP_QSPI_GetInfo (QSPI_Info* pInfo);

uint8_t BSP_QSPI_EnableMemoryMappedMode(void);

/* These functions can be modified in case the current settings need to be changed for specific application needs */
void BSP_QSPI_MspInit(QSPI_HandleTypeDef *hqspi, void *Params);
void BSP_QSPI_MspDeInit(QSPI_HandleTypeDef *hqspi, void *Params);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
