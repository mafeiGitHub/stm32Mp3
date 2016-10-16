// stm32f7xx_hal_conf.h  V1.0.2  18-November-2015
#pragma once

// enabled HAL drivers
#define HAL_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_LTDC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_SAI_MODULE_ENABLED
#define HAL_SD_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DSI_MODULE_ENABLED
#define HAL_JPEG_MODULE_ENABLED

// Timeout Configuration
//#define HAL_TIMEOUT_ENABLED          1
#define HAL_ACCURATE_TIMEOUT_ENABLED   0
#define HAL_TIMEOUT_VALUE              0x1FFFFFF

//{{{
#if !defined (HSE_VALUE)
  #define HSE_VALUE    ((uint32_t)25000000) /*!< Value of the External oscillator in Hz */
#endif /* HSE_VALUE */
//}}}
//{{{
#if !defined (HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    ((uint32_t)500)   /*!< Time out for HSE start up, in ms */
#endif /* HSE_STARTUP_TIMEOUT */
//}}}
//{{{
#if !defined (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)16000000) /*!< Value of the Internal oscillator in Hz*/
#endif /* HSI_VALUE */
//}}}
//{{{
#if !defined (LSI_VALUE)
 #define LSI_VALUE  ((uint32_t)32000) // Value of the Internal Low Speed oscillator in Hz
#endif /* LSI_VALUE */
//}}}
//{{{
#if !defined (LSE_VALUE)
 #define LSE_VALUE  ((uint32_t)32768)    /*!< Value of the External Low Speed oscillator in Hz */
#endif /* LSE_VALUE */
//}}}
//{{{
#if !defined (EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE    ((uint32_t)12288000) /*!< Value of the Internal oscillator in Hz*/
#endif /* EXTERNAL_CLOCK_VALUE */
//}}}

#define  VDD_VALUE              ((uint32_t)3300) // Value of VDD in mv
#define  TICK_INT_PRIORITY      ((uint32_t)0x0F) // tick interrupt priority
#define  USE_RTOS               0

#define  ART_ACCLERATOR_ENABLE  1 // To enable instruction cache and prefetch

//{{{  Ethernet peripheral config definesuration
//  - driver buffers size and count
#define ETH_RX_BUF_SIZE ETH_MAX_PACKET_SIZE // buffer size for receive
#define ETH_TX_BUF_SIZE ETH_MAX_PACKET_SIZE // buffer size for transmit
#define ETH_RXBUFNB     ((uint32_t)5)       // 5 Rx buffers of size ETH_RX_BUF_SIZE
#define ETH_TXBUFNB     ((uint32_t)5)       // 5 Tx buffers of size ETH_TX_BUF_SIZE

// Common PHY Registers
#define PHY_BCR                         ((uint16_t)0x00)    /*!< Transceiver Basic Control Register   */
#define PHY_BSR                         ((uint16_t)0x01)    /*!< Transceiver Basic Status Register    */

#define PHY_RESET                       ((uint16_t)0x8000)  /*!< PHY Reset */
#define PHY_LOOPBACK                    ((uint16_t)0x4000)  /*!< Select loop-back mode */
#define PHY_FULLDUPLEX_100M             ((uint16_t)0x2100)  /*!< Set the full-duplex mode at 100 Mb/s */
#define PHY_HALFDUPLEX_100M             ((uint16_t)0x2000)  /*!< Set the half-duplex mode at 100 Mb/s */
#define PHY_FULLDUPLEX_10M              ((uint16_t)0x0100)  /*!< Set the full-duplex mode at 10 Mb/s  */
#define PHY_HALFDUPLEX_10M              ((uint16_t)0x0000)  /*!< Set the half-duplex mode at 10 Mb/s  */
#define PHY_AUTONEGOTIATION             ((uint16_t)0x1000)  /*!< Enable auto-negotiation function     */
#define PHY_RESTART_AUTONEGOTIATION     ((uint16_t)0x0200)  /*!< Restart auto-negotiation function    */
#define PHY_POWERDOWN                   ((uint16_t)0x0800)  /*!< Select the power down mode           */
#define PHY_ISOLATE                     ((uint16_t)0x0400)  /*!< Isolate PHY from MII                 */

#define PHY_AUTONEGO_COMPLETE           ((uint16_t)0x0020)  /*!< Auto-Negotiation process completed   */
#define PHY_LINKED_STATUS               ((uint16_t)0x0004)  /*!< Valid link established               */
#define PHY_JABBER_DETECTION            ((uint16_t)0x0002)  /*!< Jabber condition detected            */

// Extended PHY Registers
#define PHY_SR                          ((uint16_t)0x1F)    /*!< PHY special control/ status register Offset     */
#define PHY_SPEED_STATUS                ((uint16_t)0x0004)  /*!< PHY Speed mask                                  */
#define PHY_DUPLEX_STATUS               ((uint16_t)0x0010)  /*!< PHY Duplex mask                                 */
#define PHY_ISFR                        ((uint16_t)0x1D)    /*!< PHY Interrupt Source Flag register Offset       */
#define PHY_ISFR_INT4                   ((uint16_t)0x0010)  /*!< PHY Link down inturrupt                         */
//}}}
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_eth.h"
#include "stm32f7xx_hal_flash.h"
#include "stm32f7xx_hal_gpio.h"
#include "stm32f7xx_hal_i2c.h"
#include "stm32f7xx_hal_ltdc.h"
#include "stm32f7xx_hal_pwr.h"
#include "stm32f7xx_hal_rcc.h"
#include "stm32f7xx_hal_sai.h"
#include "stm32f7xx_hal_sd.h"
#include "stm32f7xx_hal_tim.h"
#include "stm32f7xx_hal_uart.h"
#include "stm32f7xx_hal_cortex.h"
#include "stm32f7xx_hal_dsi.h"
#include "stm32f7xx_hal_jpeg.h"

//{{{
#ifdef  USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif
//}}}
