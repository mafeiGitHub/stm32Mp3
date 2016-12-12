#ifdef STM32F769I_DISCO
#include "stm32f7_discovery_qspi.h"
/*{{{  MX25L512 defines*/
#define MX25L512_FLASH_SIZE                  0x4000000 // 512 MBits => 64MBytes
#define MX25L512_SECTOR_SIZE                 0x10000   // 1024 sectors of 64KBytes
#define MX25L512_SUBSECTOR_SIZE              0x1000    // 16384 subsectors of 4kBytes
#define MX25L512_PAGE_SIZE                   0x100     // 262144 pages of 256 bytes

#define MX25L512_DUMMY_CYCLES_READ_QUAD      3
#define MX25L512_DUMMY_CYCLES_READ           8
#define MX25L512_DUMMY_CYCLES_READ_QUAD_IO   10
#define MX25L512_DUMMY_CYCLES_READ_DTR       6
#define MX25L512_DUMMY_CYCLES_READ_QUAD_DTR  8

#define MX25L512_BULK_ERASE_MAX_TIME         600000
#define MX25L512_SECTOR_ERASE_MAX_TIME       2000
#define MX25L512_SUBSECTOR_ERASE_MAX_TIME    800

// Status Register
#define MX25L512_SR_WIP                  ((uint8_t)0x01) // Write in progress
#define MX25L512_SR_WREN                 ((uint8_t)0x02) // Write enable latch
#define MX25L512_SR_BLOCKPR              ((uint8_t)0x5C) // Block protected against program and erase operations
#define MX25L512_SR_PRBOTTOM             ((uint8_t)0x20) // Protected memory area defined by BLOCKPR starts from top or bottom
#define MX25L512_SR_QUADEN               ((uint8_t)0x40) // Quad IO mode enabled if =1
#define MX25L512_SR_SRWREN               ((uint8_t)0x80) // Status register write enable/disable

// Configuration Register
#define MX25L512_CR_ODS                  ((uint8_t)0x07) // Output driver strength
#define MX25L512_CR_ODS_30               ((uint8_t)0x07) // Output driver strength 30 ohms (default)
#define MX25L512_CR_ODS_15               ((uint8_t)0x06) // Output driver strength 15 ohms
#define MX25L512_CR_ODS_20               ((uint8_t)0x05) // Output driver strength 20 ohms
#define MX25L512_CR_ODS_45               ((uint8_t)0x03) // Output driver strength 45 ohms
#define MX25L512_CR_ODS_60               ((uint8_t)0x02) // Output driver strength 60 ohms
#define MX25L512_CR_ODS_90               ((uint8_t)0x01) // Output driver strength 90 ohms
#define MX25L512_CR_TB                   ((uint8_t)0x08) // Top/Bottom bit used to configure the block protect area
#define MX25L512_CR_PBE                  ((uint8_t)0x10) // Preamble Bit Enable
#define MX25L512_CR_4BYTE                ((uint8_t)0x20) // 3-bytes or 4-bytes addressing
#define MX25L512_CR_NB_DUMMY             ((uint8_t)0xC0) // Number of dummy clock cycles

#define MX25L512_MANUFACTURER_ID         ((uint8_t)0xC2)
#define MX25L512_DEVICE_ID_MEM_TYPE      ((uint8_t)0x20)
#define MX25L512_DEVICE_ID_MEM_CAPACITY  ((uint8_t)0x1A)
#define MX25L512_UNIQUE_ID_DATA_LENGTH   ((uint8_t)0x10)

/*{{{  Reset Operations*/
#define RESET_ENABLE_CMD                     0x66
#define RESET_MEMORY_CMD                     0x99
/*}}}*/
/*{{{  Identification Operations*/
#define READ_ID_CMD                          0x9F
#define MULTIPLE_IO_READ_ID_CMD              0xAF
#define READ_SERIAL_FLASH_DISCO_PARAM_CMD    0x5A
/*}}}*/
/*{{{  Read Operations*/
#define READ_CMD                             0x03
#define READ_4_BYTE_ADDR_CMD                 0x13

#define FAST_READ_CMD                        0x0B
#define FAST_READ_DTR_CMD                    0x0D
#define FAST_READ_4_BYTE_ADDR_CMD            0x0C

#define DUAL_OUT_FAST_READ_CMD               0x3B
#define DUAL_OUT_FAST_READ_4_BYTE_ADDR_CMD   0x3C

#define DUAL_INOUT_FAST_READ_CMD             0xBB
#define DUAL_INOUT_FAST_READ_DTR_CMD         0xBD
#define DUAL_INOUT_FAST_READ_4_BYTE_ADDR_CMD 0xBC

#define QUAD_OUT_FAST_READ_CMD               0x6B
#define QUAD_OUT_FAST_READ_4_BYTE_ADDR_CMD   0x6C

#define QUAD_INOUT_FAST_READ_CMD             0xEB
#define QUAD_INOUT_FAST_READ_DTR_CMD         0xED
#define QPI_READ_4_BYTE_ADDR_CMD             0xEC
/*}}}*/
/*{{{  Write Operations*/
#define WRITE_ENABLE_CMD                     0x06
#define WRITE_DISABLE_CMD                    0x04
/*}}}*/
/*{{{  Register Operations*/
#define READ_STATUS_REG_CMD                  0x05
#define READ_CFG_REG_CMD                     0x15
#define WRITE_STATUS_CFG_REG_CMD             0x01

#define READ_LOCK_REG_CMD                    0x2D
#define WRITE_LOCK_REG_CMD                   0x2C

#define READ_EXT_ADDR_REG_CMD                0xC8
#define WRITE_EXT_ADDR_REG_CMD               0xC5
/*}}}*/
/*{{{  Program Operations*/
#define PAGE_PROG_CMD                        0x02
#define QPI_PAGE_PROG_4_BYTE_ADDR_CMD        0x12

#define QUAD_IN_FAST_PROG_CMD                0x38
#define EXT_QUAD_IN_FAST_PROG_CMD            0x38
#define QUAD_IN_FAST_PROG_4_BYTE_ADDR_CMD    0x3E
/*}}}*/
/*{{{  Erase Operations*/
#define SUBSECTOR_ERASE_CMD                  0x20
#define SUBSECTOR_ERASE_4_BYTE_ADDR_CMD      0x21

#define SECTOR_ERASE_CMD                     0xD8
#define SECTOR_ERASE_4_BYTE_ADDR_CMD         0xDC

#define BULK_ERASE_CMD                       0xC7

#define PROG_ERASE_RESUME_CMD                0x30
#define PROG_ERASE_SUSPEND_CMD               0xB0
/*}}}*/
/*{{{  4-byte Address Mode Operations*/
#define ENTER_4_BYTE_ADDR_MODE_CMD           0xB7
#define EXIT_4_BYTE_ADDR_MODE_CMD            0xE9
/*}}}*/
/*{{{  Quad Operations*/
#define ENTER_QUAD_CMD                       0x35
#define EXIT_QUAD_CMD                        0xF5
/*}}}*/
/*}}}*/
/*{{{  QSPI defines*/
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
/*}}}*/

QSPI_HandleTypeDef QSPIHandle;

/*{{{*/
static uint8_t QSPI_AutoPollingMemReady (QSPI_HandleTypeDef* hqspi, uint32_t Timeout) {

  QSPI_CommandTypeDef     s_command;
  QSPI_AutoPollingTypeDef s_config;

  /* Configure automatic polling mode to wait for memory ready */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  s_config.Match              = 0;
  s_config.Mask               = MX25L512_SR_WIP;
  s_config.MatchMode          = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize    = 1;
  s_config.Interval           = 0x10;
  s_config.AutomaticStop      = QSPI_AUTOMATIC_STOP_ENABLE;
  if (HAL_QSPI_AutoPolling (hqspi, &s_command, &s_config, Timeout) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_ResetMemory (QSPI_HandleTypeDef* hqspi) {

  QSPI_CommandTypeDef      s_command;
  QSPI_AutoPollingTypeDef  s_config;
  uint8_t                  reg;

  /* Send command RESET command in QPI mode (QUAD I/Os) */
  /* Initialize the reset enable command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = RESET_ENABLE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command(hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Send the reset memory command */
  s_command.Instruction = RESET_MEMORY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Send command RESET command in SPI mode */
  /* Initialize the reset enable command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = RESET_ENABLE_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Send the reset memory command */
  s_command.Instruction = RESET_MEMORY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* After reset CMD, 1000ms requested if QSPI memory SWReset occured during full chip erase operation */
  HAL_Delay (1000);

  /* Configure automatic polling mode to wait the WIP bit=0 */
  s_config.Match           = 0;
  s_config.Mask            = MX25L512_SR_WIP;
  s_config.MatchMode       = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize = 1;
  s_config.Interval        = 0x10;
  s_config.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

  s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction     = READ_STATUS_REG_CMD;
  s_command.DataMode        = QSPI_DATA_1_LINE;
  if (HAL_QSPI_AutoPolling (hqspi, &s_command, &s_config, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Initialize the reading of status register */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_1_LINE;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command(hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive (hqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Enable write operations, command in 1 bit */
  /* Enable write operations */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = WRITE_ENABLE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Configure automatic polling mode to wait for write enabling */
  s_config.Match           = MX25L512_SR_WREN;
  s_config.Mask            = MX25L512_SR_WREN;
  s_config.MatchMode       = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize = 1;
  s_config.Interval        = 0x10;
  s_config.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;
  s_command.Instruction    = READ_STATUS_REG_CMD;
  s_command.DataMode       = QSPI_DATA_1_LINE;
  if (HAL_QSPI_AutoPolling (hqspi, &s_command, &s_config, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Update the configuration register with new dummy cycles */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = WRITE_STATUS_CFG_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_1_LINE;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Enable the Quad IO on the QSPI memory (Non-volatile bit) */
  reg |= MX25L512_SR_QUADEN;

  /* Configure the command */
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Transmission of the data */
  if (HAL_QSPI_Transmit (hqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* 40ms  Write Status/Configuration Register Cycle Time */
  HAL_Delay (40);

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_WriteEnable (QSPI_HandleTypeDef* hqspi)
{
  QSPI_CommandTypeDef     s_command;
  QSPI_AutoPollingTypeDef s_config;

  /* Enable write operations */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = WRITE_ENABLE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Configure automatic polling mode to wait for write enabling */
  s_config.Match           = MX25L512_SR_WREN;
  s_config.Mask            = MX25L512_SR_WREN;
  s_config.MatchMode       = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize = 1;
  s_config.Interval        = 0x10;
  s_config.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;
  s_command.Instruction    = READ_STATUS_REG_CMD;
  s_command.DataMode       = QSPI_DATA_4_LINES;
  if (HAL_QSPI_AutoPolling (hqspi, &s_command, &s_config, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
}
/*}}}*/
/*{{{*/
static uint8_t QSPI_DummyCyclesCfg (QSPI_HandleTypeDef* hqspi) {

  QSPI_CommandTypeDef s_command;
  uint8_t reg[2];

  /* Initialize the reading of status register */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive (hqspi, &(reg[0]), HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Initialize the reading of configuration register */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = READ_CFG_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive (hqspi, &(reg[1]), HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Enable write operations */
  if (QSPI_WriteEnable (hqspi) != QSPI_OK)
    return QSPI_ERROR;

  /* Update the configuration register with new dummy cycles */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = WRITE_STATUS_CFG_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 2;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* MX25L512_DUMMY_CYCLES_READ_QUAD = 3 for 10 cycles in QPI mode */
  MODIFY_REG( reg[1], MX25L512_CR_NB_DUMMY, (MX25L512_DUMMY_CYCLES_READ_QUAD << POSITION_VAL(MX25L512_CR_NB_DUMMY)));

  /* Configure the write volatile configuration register command */
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Transmission of the data */
  if (HAL_QSPI_Transmit (hqspi, &(reg[0]), HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* 40ms  Write Status/Configuration Register Cycle Time */
  HAL_Delay( 40 );

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
static uint8_t QSPI_OutDrvStrengthCfg (QSPI_HandleTypeDef* hqspi) {

  uint8_t reg[2];

  /* Initialize the reading of status register */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive (hqspi, &(reg[0]), HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Initialize the reading of configuration register */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = READ_CFG_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive (hqspi, &(reg[1]), HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Enable write operations */
  if (QSPI_WriteEnable (&QSPIHandle) != QSPI_OK)
    return QSPI_ERROR;

  /* Update the configuration register with new output driver strength */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = WRITE_STATUS_CFG_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 2;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Set Output Strength of the QSPI memory 15 ohms */
  MODIFY_REG( reg[1], MX25L512_CR_ODS, (MX25L512_CR_ODS_15 << POSITION_VAL(MX25L512_CR_ODS)));

  /* Configure the write volatile configuration register command */
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Transmission of the data */
  if (HAL_QSPI_Transmit (hqspi, &(reg[0]), HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_EnterFourBytesAddress(QSPI_HandleTypeDef* hqspi) {

  /* Initialize the command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = ENTER_4_BYTE_ADDR_MODE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (QSPI_WriteEnable(hqspi) != QSPI_OK)
    return QSPI_ERROR;

  /* Send the command */
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Configure automatic polling mode to wait the memory is ready */
  if (QSPI_AutoPollingMemReady (hqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != QSPI_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_EnterMemory_QPI (QSPI_HandleTypeDef* hqspi) {

  /* Initialize the QPI enable command */
  /* QSPI memory is supported to be in SPI mode, so CMD on 1 LINE */
  QSPI_CommandTypeDef      s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = ENTER_QUAD_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Configure automatic polling mode to wait the QUADEN bit=1 and WIP bit=0 */
  QSPI_AutoPollingTypeDef  s_config;
  s_config.Match           = MX25L512_SR_QUADEN;
  s_config.Mask            = MX25L512_SR_QUADEN|MX25L512_SR_WIP;
  s_config.MatchMode       = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize = 1;
  s_config.Interval        = 0x10;
  s_config.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  if (HAL_QSPI_AutoPolling (hqspi, &s_command, &s_config, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_ExitMemory_QPI (QSPI_HandleTypeDef* hqspi) {


  /* Initialize the QPI enable command */
  /* QSPI memory is supported to be in QPI mode, so CMD on 4 LINES */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = EXIT_QUAD_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_QSPI_Init() {

  QSPIHandle.Instance = QUADSPI;

  /* Call the DeInit function to reset the driver */
  if (HAL_QSPI_DeInit (&QSPIHandle) != HAL_OK)
    return QSPI_ERROR;

  /* System level initialization */
  BSP_QSPI_MspInit (&QSPIHandle, NULL);

  /* QSPI initialization */
  /* QSPI freq = SYSCLK /(1 + ClockPrescaler) = 216 MHz/(1+1) = 108 Mhz */
  QSPIHandle.Init.ClockPrescaler     = 1;   /* QSPI freq = 216 MHz/(1+1) = 108 Mhz */
  QSPIHandle.Init.FifoThreshold      = 16;
  QSPIHandle.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  QSPIHandle.Init.FlashSize          = POSITION_VAL(MX25L512_FLASH_SIZE) - 1;
  QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
  QSPIHandle.Init.ClockMode          = QSPI_CLOCK_MODE_0;
  QSPIHandle.Init.FlashID            = QSPI_FLASH_ID_1;
  QSPIHandle.Init.DualFlash          = QSPI_DUALFLASH_DISABLE;
  if (HAL_QSPI_Init (&QSPIHandle) != HAL_OK)
    return QSPI_ERROR;

  /* QSPI memory reset */
  if (QSPI_ResetMemory (&QSPIHandle) != QSPI_OK)
    return QSPI_NOT_SUPPORTED;

  /* Put QSPI memory in QPI mode */
  if (QSPI_EnterMemory_QPI (&QSPIHandle) != QSPI_OK )
    return QSPI_NOT_SUPPORTED;

  /* Set the QSPI memory in 4-bytes address mode */
  if (QSPI_EnterFourBytesAddress (&QSPIHandle) != QSPI_OK)
    return QSPI_NOT_SUPPORTED;

  /* Configuration of the dummy cycles on QSPI memory side */
  if (QSPI_DummyCyclesCfg (&QSPIHandle) != QSPI_OK)
    return QSPI_NOT_SUPPORTED;

  /* Configuration of the Output driver strength on memory side */
  if (QSPI_OutDrvStrengthCfg (&QSPIHandle) != QSPI_OK )
    return QSPI_NOT_SUPPORTED;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_DeInit() {

  QSPIHandle.Instance = QUADSPI;

  /* Put QSPI memory in SPI mode */
  if (QSPI_ExitMemory_QPI (&QSPIHandle )!=QSPI_OK )
    return QSPI_NOT_SUPPORTED;

  /* Call the DeInit function to reset the driver */
  if (HAL_QSPI_DeInit (&QSPIHandle) != HAL_OK)
    return QSPI_ERROR;

  /* System level De-initialization */
  BSP_QSPI_MspDeInit (&QSPIHandle, NULL);

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_QSPI_Read (uint8_t* pData, uint32_t ReadAddr, uint32_t Size) {

  /* Initialize the read command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = QPI_READ_4_BYTE_ADDR_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_32_BITS;
  s_command.Address           = ReadAddr;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = MX25L512_DUMMY_CYCLES_READ_QUAD_IO;
  s_command.NbData            = Size;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive (&QSPIHandle, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_Write (uint8_t* pData, uint32_t WriteAddr, uint32_t Size) {

  /* Calculation of the size between the write address and the end of the page */
  uint32_t current_addr = 0;
  while (current_addr <= WriteAddr)
    current_addr += MX25L512_PAGE_SIZE;

  /* Check if the size of the data is less than the remaining place in the page */
  uint32_t current_size = current_addr - WriteAddr;
  if (current_size > Size)
    current_size = Size;

  /* Initialize the address variables */
  current_addr = WriteAddr;
  uint32_t end_addr = WriteAddr + Size;

  /* Initialize the program command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = QPI_PAGE_PROG_4_BYTE_ADDR_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_32_BITS;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Perform the write page by page */
  do {
    s_command.Address = current_addr;
    s_command.NbData  = current_size;

    /* Enable write operations */
    if (QSPI_WriteEnable (&QSPIHandle) != QSPI_OK)
      return QSPI_ERROR;

    /* Configure the command */
    if (HAL_QSPI_Command (&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return QSPI_ERROR;

    /* Transmission of the data */
    if (HAL_QSPI_Transmit (&QSPIHandle, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return QSPI_ERROR;

    /* Configure automatic polling mode to wait for end of program */
    if (QSPI_AutoPollingMemReady (&QSPIHandle, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != QSPI_OK)
      return QSPI_ERROR;

    /* Update the address and size variables for next page programming */
    current_addr += current_size;
    pData += current_size;
    current_size = ((current_addr + MX25L512_PAGE_SIZE) > end_addr) ? (end_addr - current_addr) : MX25L512_PAGE_SIZE;
    } while (current_addr < end_addr);

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_QSPI_Erase_Block (uint32_t BlockAddress) {

  /* Initialize the erase command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = SUBSECTOR_ERASE_4_BYTE_ADDR_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_32_BITS;
  s_command.Address           = BlockAddress;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (QSPI_WriteEnable (&QSPIHandle) != QSPI_OK)
    return QSPI_ERROR;

  /* Send the command */
  if (HAL_QSPI_Command (&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Configure automatic polling mode to wait for end of erase */
  if (QSPI_AutoPollingMemReady (&QSPIHandle, MX25L512_SUBSECTOR_ERASE_MAX_TIME) != QSPI_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_Erase_Chip() {

  /* Initialize the erase command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = BULK_ERASE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (QSPI_WriteEnable(&QSPIHandle) != QSPI_OK)
    return QSPI_ERROR;

  /* Send the command */
  if (HAL_QSPI_Command (&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Configure automatic polling mode to wait for end of erase */
  if (QSPI_AutoPollingMemReady (&QSPIHandle, MX25L512_BULK_ERASE_MAX_TIME) != QSPI_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_QSPI_GetStatus() {

  /* Initialize the read flag status register command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  uint8_t reg;
  if (HAL_QSPI_Receive (&QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Check the value of the register*/
  if ((reg & MX25L512_SR_WIP) == 0)
    return QSPI_OK;
  else
    return QSPI_BUSY;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_GetInfo (QSPI_Info* pInfo) {

  /* Configure the structure with the memory configuration */
  pInfo->FlashSize          = MX25L512_FLASH_SIZE;
  pInfo->EraseSectorSize    = MX25L512_SUBSECTOR_SIZE;
  pInfo->EraseSectorsNumber = (MX25L512_FLASH_SIZE/MX25L512_SUBSECTOR_SIZE);
  pInfo->ProgPageSize       = MX25L512_PAGE_SIZE;
  pInfo->ProgPagesNumber    = (MX25L512_FLASH_SIZE/MX25L512_PAGE_SIZE);

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_EnableMemoryMappedMode() {

  QSPI_CommandTypeDef s_command;
  QSPI_MemoryMappedTypeDef s_mem_mapped_cfg;

  /* Configure the command for the read instruction */
  s_command.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
  s_command.Instruction       = QPI_READ_4_BYTE_ADDR_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_32_BITS;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = MX25L512_DUMMY_CYCLES_READ_QUAD_IO;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Configure the memory mapped mode */
  s_mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  s_mem_mapped_cfg.TimeOutPeriod     = 0;
  if (HAL_QSPI_MemoryMapped (&QSPIHandle, &s_command, &s_mem_mapped_cfg) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
__weak void BSP_QSPI_MspInit (QSPI_HandleTypeDef* hqspi, void* Params) {

  GPIO_InitTypeDef gpio_init_structure;

  /* Enable the QuadSPI memory interface clock */
  QSPI_CLK_ENABLE();

  /* Reset the QuadSPI memory interface */
  QSPI_FORCE_RESET();
  QSPI_RELEASE_RESET();

  QSPI_CS_GPIO_CLK_ENABLE();
  QSPI_CLK_GPIO_CLK_ENABLE();
  QSPI_D0_GPIO_CLK_ENABLE();
  QSPI_D1_GPIO_CLK_ENABLE();
  QSPI_D2_GPIO_CLK_ENABLE();
  QSPI_D3_GPIO_CLK_ENABLE();

  /* QSPI CS GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_CS_PIN;
  gpio_init_structure.Alternate = QSPI_CS_PIN_AF;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(QSPI_CS_GPIO_PORT, &gpio_init_structure);

  /* QSPI CLK GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_CLK_PIN;
  gpio_init_structure.Alternate = QSPI_CLK_PIN_AF;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  HAL_GPIO_Init(QSPI_CLK_GPIO_PORT, &gpio_init_structure);

  /* QSPI D0 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D0_PIN;
  gpio_init_structure.Alternate = QSPI_D0_PIN_AF;
  HAL_GPIO_Init(QSPI_D0_GPIO_PORT, &gpio_init_structure);

  /* QSPI D1 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D1_PIN;
  gpio_init_structure.Alternate = QSPI_D1_PIN_AF;
  HAL_GPIO_Init(QSPI_D1_GPIO_PORT, &gpio_init_structure);

  /* QSPI D2 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D2_PIN;
  gpio_init_structure.Alternate = QSPI_D2_PIN_AF;
  HAL_GPIO_Init(QSPI_D2_GPIO_PORT, &gpio_init_structure);

  /* QSPI D3 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D3_PIN;
  gpio_init_structure.Alternate = QSPI_D3_PIN_AF;
  HAL_GPIO_Init(QSPI_D3_GPIO_PORT, &gpio_init_structure);

  /* NVIC configuration for QSPI interrupt */
  HAL_NVIC_SetPriority(QUADSPI_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ(QUADSPI_IRQn);
  }
/*}}}*/
/*{{{*/
__weak void BSP_QSPI_MspDeInit (QSPI_HandleTypeDef* hqspi, void* Params) {

  /*##-1- Disable the NVIC for QSPI ###########################################*/
  HAL_NVIC_DisableIRQ(QUADSPI_IRQn);

  /*##-2- Disable peripherals and GPIO Clocks ################################*/
  /* De-Configure QSPI pins */
  HAL_GPIO_DeInit(QSPI_CS_GPIO_PORT, QSPI_CS_PIN);
  HAL_GPIO_DeInit(QSPI_CLK_GPIO_PORT, QSPI_CLK_PIN);
  HAL_GPIO_DeInit(QSPI_D0_GPIO_PORT, QSPI_D0_PIN);
  HAL_GPIO_DeInit(QSPI_D1_GPIO_PORT, QSPI_D1_PIN);
  HAL_GPIO_DeInit(QSPI_D2_GPIO_PORT, QSPI_D2_PIN);
  HAL_GPIO_DeInit(QSPI_D3_GPIO_PORT, QSPI_D3_PIN);

  /*##-3- Reset peripherals ##################################################*/
  /* Reset the QuadSPI memory interface */
  QSPI_FORCE_RESET();
  QSPI_RELEASE_RESET();

  /* Disable the QuadSPI memory interface clock */
  QSPI_CLK_DISABLE();
  }
/*}}}*/
#endif
