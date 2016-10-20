// ******************************************************************************
//  * @file    stm32f769i_discovery.h
//  * @version V1.1.0
//  * @date    29-August-2016
#pragma once
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}

#include "stm32f7xx_hal.h"

#define LEDn ((uint8_t)4)
typedef enum { LED1 = 0, LED_RED = LED1, LED2 = 1, LED_GREEN = LED2, LED3 = 2, LED4 = 3 } Led_TypeDef;
typedef enum { BUTTON_WAKEUP = 0, } Button_TypeDef;
typedef enum { BUTTON_MODE_GPIO = 0, BUTTON_MODE_EXTI = 1 } ButtonMode_TypeDef;
typedef enum { PB_SET = 0, PB_RESET = !PB_SET } ButtonValue_TypeDef;
typedef enum { DISCO_OK    = 0, DISCO_ERROR = 1 } DISCO_Status_TypeDef;

#define BUTTON_USER BUTTON_WAKEUP

/* 2 Leds are connected to MCU directly on PJ13 and PJ5 */
#define LED1_GPIO_PORT                   ((GPIO_TypeDef*)GPIOJ)
#define LED2_GPIO_PORT                   ((GPIO_TypeDef*)GPIOJ)
#define LED3_GPIO_PORT                   ((GPIO_TypeDef*)GPIOA)
#define LED4_GPIO_PORT                   ((GPIO_TypeDef*)GPIOD)

#define LED1_PIN                         ((uint32_t)GPIO_PIN_13)
#define LED2_PIN                         ((uint32_t)GPIO_PIN_5)
#define LED3_PIN                         ((uint32_t)GPIO_PIN_12)
#define LED4_PIN                         ((uint32_t)GPIO_PIN_4)

#define BUTTONn                          ((uint8_t)1)

#define WAKEUP_BUTTON_PIN                GPIO_PIN_0
#define WAKEUP_BUTTON_GPIO_PORT          GPIOA
#define WAKEUP_BUTTON_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
#define WAKEUP_BUTTON_GPIO_CLK_DISABLE() __HAL_RCC_GPIOA_CLK_DISABLE()
#define WAKEUP_BUTTON_EXTI_IRQn          EXTI0_IRQn

/* Define the USER button as an alias of the Wakeup button */
#define USER_BUTTON_PIN                   WAKEUP_BUTTON_PIN
#define USER_BUTTON_GPIO_PORT             WAKEUP_BUTTON_GPIO_PORT
#define USER_BUTTON_GPIO_CLK_ENABLE()     WAKEUP_BUTTON_GPIO_CLK_ENABLE()
#define USER_BUTTON_GPIO_CLK_DISABLE()    WAKEUP_BUTTON_GPIO_CLK_DISABLE()
#define USER_BUTTON_EXTI_IRQn             WAKEUP_BUTTON_EXTI_IRQn

#define BUTTON_GPIO_CLK_ENABLE()            __HAL_RCC_GPIOA_CLK_ENABLE()

#define OTG_HS_OVER_CURRENT_PIN                  GPIO_PIN_4
#define OTG_HS_OVER_CURRENT_PORT                 GPIOD
#define OTG_HS_OVER_CURRENT_PORT_CLK_ENABLE()    __HAL_RCC_GPIOD_CLK_ENABLE()

#define SD_DETECT_PIN                        ((uint32_t)GPIO_PIN_15)
#define SD_DETECT_GPIO_PORT                  ((GPIO_TypeDef*)GPIOI)
#define SD_DETECT_GPIO_CLK_ENABLE()          __HAL_RCC_GPIOI_CLK_ENABLE()
#define SD_DETECT_GPIO_CLK_DISABLE()         __HAL_RCC_GPIOI_CLK_DISABLE()
#define SD_DETECT_EXTI_IRQn                  EXTI15_10_IRQn

#define TS_INT_PIN                        ((uint32_t)GPIO_PIN_13)
#define TS_INT_GPIO_PORT                  ((GPIO_TypeDef*)GPIOI)
#define TS_INT_GPIO_CLK_ENABLE()          __HAL_RCC_GPIOI_CLK_ENABLE()
#define TS_INT_GPIO_CLK_DISABLE()         __HAL_RCC_GPIOI_CLK_DISABLE()
#define TS_INT_EXTI_IRQn                  EXTI15_10_IRQn
#define TS_I2C_ADDRESS                   ((uint16_t)0x54)

#define AUDIO_I2C_ADDRESS                ((uint16_t)0x34)

#define DISCOVERY_AUDIO_I2Cx                             I2C4
#define DISCOVERY_AUDIO_I2Cx_CLK_ENABLE()                __HAL_RCC_I2C4_CLK_ENABLE()
#define DISCOVERY_AUDIO_I2Cx_SCL_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOD_CLK_ENABLE()
#define DISCOVERY_AUDIO_I2Cx_SDA_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOB_CLK_ENABLE()

#define DISCOVERY_AUDIO_I2Cx_FORCE_RESET()               __HAL_RCC_I2C4_FORCE_RESET()
#define DISCOVERY_AUDIO_I2Cx_RELEASE_RESET()             __HAL_RCC_I2C4_RELEASE_RESET()

#define DISCOVERY_AUDIO_I2Cx_SCL_PIN                     GPIO_PIN_12 /*!< PD12 */
#define DISCOVERY_AUDIO_I2Cx_SCL_AF                      GPIO_AF4_I2C4
#define DISCOVERY_AUDIO_I2Cx_SCL_GPIO_PORT               GPIOD
#define DISCOVERY_AUDIO_I2Cx_SDA_PIN                     GPIO_PIN_7 /*!< PB7 */
#define DISCOVERY_AUDIO_I2Cx_SDA_AF                      GPIO_AF11_I2C4
#define DISCOVERY_AUDIO_I2Cx_SDA_GPIO_PORT               GPIOB
#define DISCOVERY_AUDIO_I2Cx_EV_IRQn                     I2C4_EV_IRQn
#define DISCOVERY_AUDIO_I2Cx_ER_IRQn                     I2C4_ER_IRQn

#define DISCOVERY_EXT_I2Cx                             I2C1
#define DISCOVERY_EXT_I2Cx_CLK_ENABLE()                __HAL_RCC_I2C1_CLK_ENABLE()
#define DISCOVERY_DMAx_CLK_ENABLE()                    __HAL_RCC_DMA1_CLK_ENABLE()
#define DISCOVERY_EXT_I2Cx_SCL_SDA_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOB_CLK_ENABLE()

#define DISCOVERY_EXT_I2Cx_FORCE_RESET()               __HAL_RCC_I2C1_FORCE_RESET()
#define DISCOVERY_EXT_I2Cx_RELEASE_RESET()             __HAL_RCC_I2C1_RELEASE_RESET()

#define DISCOVERY_EXT_I2Cx_SCL_PIN                     GPIO_PIN_8 /*!< PB8 */
#define DISCOVERY_EXT_I2Cx_SCL_SDA_GPIO_PORT           GPIOB
#define DISCOVERY_EXT_I2Cx_SCL_SDA_AF                  GPIO_AF4_I2C1
#define DISCOVERY_EXT_I2Cx_SDA_PIN                     GPIO_PIN_9 /*!< PB9 */
#define DISCOVERY_EXT_I2Cx_EV_IRQn                     I2C1_EV_IRQn
#define DISCOVERY_EXT_I2Cx_ER_IRQn                     I2C1_ER_IRQn

#ifndef DISCOVERY_I2Cx_TIMING
#define DISCOVERY_I2Cx_TIMING                      ((uint32_t)0x40912732)
#endif /* DISCOVERY_I2Cx_TIMING */

uint32_t         BSP_GetVersion(void);
void             BSP_LED_Init(Led_TypeDef Led);
void             BSP_LED_DeInit(Led_TypeDef Led);
void             BSP_LED_On(Led_TypeDef Led);
void             BSP_LED_Off(Led_TypeDef Led);
void             BSP_LED_Toggle(Led_TypeDef Led);
void             BSP_PB_Init(Button_TypeDef Button, ButtonMode_TypeDef Button_Mode);
void             BSP_PB_DeInit(Button_TypeDef Button);
uint32_t         BSP_PB_GetState(Button_TypeDef Button);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
