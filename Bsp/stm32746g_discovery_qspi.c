#ifdef STM32F746G_DISCO
#include "stm32746g_discovery_qspi.h"
/*{{{  N25Q128A defines*/
#define N25Q128A_FLASH_SIZE                  0x1000000 /* 128 MBits => 16MBytes */
#define N25Q128A_SECTOR_SIZE                 0x10000   /* 256 sectors of 64KBytes */
#define N25Q128A_SUBSECTOR_SIZE              0x1000    /* 4096 subsectors of 4kBytes */
#define N25Q128A_PAGE_SIZE                   0x100     /* 65536 pages of 256 bytes */

#define N25Q128A_DUMMY_CYCLES_READ           8
#define N25Q128A_DUMMY_CYCLES_READ_QUAD      10

#define N25Q128A_BULK_ERASE_MAX_TIME         250000
#define N25Q128A_SECTOR_ERASE_MAX_TIME       3000
#define N25Q128A_SUBSECTOR_ERASE_MAX_TIME    800

/* Reset Operations */
#define RESET_ENABLE_CMD                     0x66
#define RESET_MEMORY_CMD                     0x99

/* Identification Operations */
#define READ_ID_CMD                          0x9E
#define READ_ID_CMD2                         0x9F
#define MULTIPLE_IO_READ_ID_CMD              0xAF
#define READ_SERIAL_FLASH_DISCO_PARAM_CMD    0x5A

/* Read Operations */
#define READ_CMD                             0x03
#define FAST_READ_CMD                        0x0B
#define DUAL_OUT_FAST_READ_CMD               0x3B
#define DUAL_INOUT_FAST_READ_CMD             0xBB
#define QUAD_OUT_FAST_READ_CMD               0x6B
#define QUAD_INOUT_FAST_READ_CMD             0xEB

/* Write Operations */
#define WRITE_ENABLE_CMD                     0x06
#define WRITE_DISABLE_CMD                    0x04

/* Register Operations */
#define READ_STATUS_REG_CMD                  0x05
#define WRITE_STATUS_REG_CMD                 0x01

#define READ_LOCK_REG_CMD                    0xE8
#define WRITE_LOCK_REG_CMD                   0xE5

#define READ_FLAG_STATUS_REG_CMD             0x70
#define CLEAR_FLAG_STATUS_REG_CMD            0x50

#define READ_NONVOL_CFG_REG_CMD              0xB5
#define WRITE_NONVOL_CFG_REG_CMD             0xB1

#define READ_VOL_CFG_REG_CMD                 0x85
#define WRITE_VOL_CFG_REG_CMD                0x81

#define READ_ENHANCED_VOL_CFG_REG_CMD        0x65
#define WRITE_ENHANCED_VOL_CFG_REG_CMD       0x61

/* Program Operations */
#define PAGE_PROG_CMD                        0x02
#define DUAL_IN_FAST_PROG_CMD                0xA2
#define EXT_DUAL_IN_FAST_PROG_CMD            0xD2
#define QUAD_IN_FAST_PROG_CMD                0x32
#define EXT_QUAD_IN_FAST_PROG_CMD            0x12

/* Erase Operations */
#define SUBSECTOR_ERASE_CMD                  0x20
#define SECTOR_ERASE_CMD                     0xD8
#define BULK_ERASE_CMD                       0xC7

#define PROG_ERASE_RESUME_CMD                0x7A
#define PROG_ERASE_SUSPEND_CMD               0x75

/* One-Time Programmable Operations */
#define READ_OTP_ARRAY_CMD                   0x4B
#define PROG_OTP_ARRAY_CMD                   0x42

/* Status Register */
#define N25Q128A_SR_WIP                      ((uint8_t)0x01)    /*!< Write in progress */
#define N25Q128A_SR_WREN                     ((uint8_t)0x02)    /*!< Write enable latch */
#define N25Q128A_SR_BLOCKPR                  ((uint8_t)0x5C)    /*!< Block protected against program and erase operations */
#define N25Q128A_SR_PRBOTTOM                 ((uint8_t)0x20)    /*!< Protected memory area defined by BLOCKPR starts from top or bottom */
#define N25Q128A_SR_SRWREN                   ((uint8_t)0x80)    /*!< Status register write enable/disable */

/* Nonvolatile Configuration Register */
#define N25Q128A_NVCR_LOCK                   ((uint16_t)0x0001) /*!< Lock nonvolatile configuration register */
#define N25Q128A_NVCR_DUAL                   ((uint16_t)0x0004) /*!< Dual I/O protocol */
#define N25Q128A_NVCR_QUAB                   ((uint16_t)0x0008) /*!< Quad I/O protocol */
#define N25Q128A_NVCR_RH                     ((uint16_t)0x0010) /*!< Reset/hold */
#define N25Q128A_NVCR_ODS                    ((uint16_t)0x01C0) /*!< Output driver strength */
#define N25Q128A_NVCR_XIP                    ((uint16_t)0x0E00) /*!< XIP mode at power-on reset */
#define N25Q128A_NVCR_NB_DUMMY               ((uint16_t)0xF000) /*!< Number of dummy clock cycles */

/* Volatile Configuration Register */
#define N25Q128A_VCR_WRAP                    ((uint8_t)0x03)    /*!< Wrap */
#define N25Q128A_VCR_XIP                     ((uint8_t)0x08)    /*!< XIP */
#define N25Q128A_VCR_NB_DUMMY                ((uint8_t)0xF0)    /*!< Number of dummy clock cycles */

/* Enhanced Volatile Configuration Register */
#define N25Q128A_EVCR_ODS                    ((uint8_t)0x07)    /*!< Output driver strength */
#define N25Q128A_EVCR_VPPA                   ((uint8_t)0x08)    /*!< Vpp accelerator */
#define N25Q128A_EVCR_RH                     ((uint8_t)0x10)    /*!< Reset/hold */
#define N25Q128A_EVCR_DUAL                   ((uint8_t)0x40)    /*!< Dual I/O protocol */
#define N25Q128A_EVCR_QUAD                   ((uint8_t)0x80)    /*!< Quad I/O protocol */

/* Flag Status Register */
#define N25Q128A_FSR_PRERR                   ((uint8_t)0x02)    /*!< Protection error */
#define N25Q128A_FSR_PGSUS                   ((uint8_t)0x04)    /*!< Program operation suspended */
#define N25Q128A_FSR_VPPERR                  ((uint8_t)0x08)    /*!< Invalid voltage during program or erase */
#define N25Q128A_FSR_PGERR                   ((uint8_t)0x10)    /*!< Program error */
#define N25Q128A_FSR_ERERR                   ((uint8_t)0x20)    /*!< Erase error */
#define N25Q128A_FSR_ERSUS                   ((uint8_t)0x40)    /*!< Erase operation suspended */
#define N25Q128A_FSR_READY                   ((uint8_t)0x80)    /*!< Ready or command in progress */
/*}}}*/
/*{{{  QSPI defines*/
#define QSPI_CS_PIN                GPIO_PIN_6
#define QSPI_CS_GPIO_PORT          GPIOB

#define QSPI_CLK_PIN               GPIO_PIN_2
#define QSPI_CLK_GPIO_PORT         GPIOB

#define QSPI_D0_PIN                GPIO_PIN_11
#define QSPI_D0_GPIO_PORT          GPIOD

#define QSPI_D1_PIN                GPIO_PIN_12
#define QSPI_D1_GPIO_PORT          GPIOD

#define QSPI_D2_PIN                GPIO_PIN_2
#define QSPI_D2_GPIO_PORT          GPIOE

#define QSPI_D3_PIN                GPIO_PIN_13
#define QSPI_D3_GPIO_PORT          GPIOD

#define QSPI_FORCE_RESET()         __HAL_RCC_QSPI_FORCE_RESET()
#define QSPI_RELEASE_RESET()       __HAL_RCC_QSPI_RELEASE_RESET()

// Definition for QSPI clock resources
#define QSPI_CLK_ENABLE()          __HAL_RCC_QSPI_CLK_ENABLE()
#define QSPI_CLK_DISABLE()         __HAL_RCC_QSPI_CLK_DISABLE()

#define QSPI_CS_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()
#define QSPI_CLK_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define QSPI_D0_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#define QSPI_D1_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#define QSPI_D2_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOE_CLK_ENABLE()
#define QSPI_D3_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
/*}}}*/

QSPI_HandleTypeDef QSPIHandle;

/*{{{*/
static uint8_t QSPI_AutoPollingMemReady (QSPI_HandleTypeDef* hqspi, uint32_t Timeout) {

  QSPI_CommandTypeDef s_command;
  QSPI_AutoPollingTypeDef s_config;

  /* Configure automatic polling mode to wait for memory ready */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_1_LINE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  s_config.Match           = 0;
  s_config.Mask            = N25Q128A_SR_WIP;
  s_config.MatchMode       = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize = 1;
  s_config.Interval        = 0x10;
  s_config.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;
  if (HAL_QSPI_AutoPolling (hqspi, &s_command, &s_config, Timeout) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_ResetMemory (QSPI_HandleTypeDef* hqspi) {

  /* Initialize the reset enable command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = RESET_ENABLE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Send the reset memory command */
  s_command.Instruction = RESET_MEMORY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Configure automatic polling mode to wait the memory is ready */
  if (QSPI_AutoPollingMemReady (hqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != QSPI_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_WriteEnable (QSPI_HandleTypeDef* hqspi) {


  /* Enable write operations */
  QSPI_CommandTypeDef     s_command;
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
  QSPI_AutoPollingTypeDef s_config;
  s_config.Match           = N25Q128A_SR_WREN;
  s_config.Mask            = N25Q128A_SR_WREN;
  s_config.MatchMode       = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize = 1;
  s_config.Interval        = 0x10;
  s_config.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;
  s_command.Instruction    = READ_STATUS_REG_CMD;
  s_command.DataMode       = QSPI_DATA_1_LINE;
  if (HAL_QSPI_AutoPolling (hqspi, &s_command, &s_config, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
static uint8_t QSPI_DummyCyclesCfg (QSPI_HandleTypeDef* hqspi) {

  /* Initialize the read volatile configuration register command */
  QSPI_CommandTypeDef s_command;
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = READ_VOL_CFG_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_1_LINE;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive (hqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Enable write operations */
  if (QSPI_WriteEnable(hqspi) != QSPI_OK)
    return QSPI_ERROR;

  /* Update volatile configuration register (with new dummy cycles) */
  s_command.Instruction = WRITE_VOL_CFG_REG_CMD;
  uint8_t reg;
  MODIFY_REG(reg, N25Q128A_VCR_NB_DUMMY, (N25Q128A_DUMMY_CYCLES_READ_QUAD << POSITION_VAL(N25Q128A_VCR_NB_DUMMY)));

  /* Configure the write volatile configuration register command */
  if (HAL_QSPI_Command (hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Transmission of the data */
  if (HAL_QSPI_Transmit (hqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
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
  QSPIHandle.Init.ClockPrescaler     = 1; /* QSPI freq = 216 MHz/(1+1) = 108 Mhz */
  QSPIHandle.Init.FifoThreshold      = 4;
  QSPIHandle.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  QSPIHandle.Init.FlashSize          = POSITION_VAL(N25Q128A_FLASH_SIZE) - 1;
  QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE;
  QSPIHandle.Init.ClockMode          = QSPI_CLOCK_MODE_0;
  QSPIHandle.Init.FlashID            = QSPI_FLASH_ID_1;
  QSPIHandle.Init.DualFlash          = QSPI_DUALFLASH_DISABLE;
  if (HAL_QSPI_Init (&QSPIHandle) != HAL_OK)
    return QSPI_ERROR;

  /* QSPI memory reset */
  if (QSPI_ResetMemory(&QSPIHandle) != QSPI_OK)
    return QSPI_NOT_SUPPORTED;

  /* Configuration of the dummy cycles on QSPI memory side */
  if (QSPI_DummyCyclesCfg (&QSPIHandle) != QSPI_OK)
    return QSPI_NOT_SUPPORTED;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_DeInit() {

  QSPIHandle.Instance = QUADSPI;

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

  QSPI_CommandTypeDef s_command;

  /* Initialize the read command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = QUAD_INOUT_FAST_READ_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_24_BITS;
  s_command.Address           = ReadAddr;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = N25Q128A_DUMMY_CYCLES_READ_QUAD;
  s_command.NbData            = Size;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command(&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive(&QSPIHandle, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_Write (uint8_t* pData, uint32_t WriteAddr, uint32_t Size) {

  QSPI_CommandTypeDef s_command;
  uint32_t end_addr, current_size, current_addr;

  /* Calculation of the size between the write address and the end of the page */
  current_addr = 0;
  while (current_addr <= WriteAddr)
    current_addr += N25Q128A_PAGE_SIZE;
  current_size = current_addr - WriteAddr;

  /* Check if the size of the data is less than the remaining place in the page */
  if (current_size > Size)
    current_size = Size;

  /* Initialize the adress variables */
  current_addr = WriteAddr;
  end_addr = WriteAddr + Size;

  /* Initialize the program command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = EXT_QUAD_IN_FAST_PROG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_24_BITS;
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
    if (QSPI_WriteEnable(&QSPIHandle) != QSPI_OK)
      return QSPI_ERROR;

    /* Configure the command */
    if (HAL_QSPI_Command(&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return QSPI_ERROR;

    /* Transmission of the data */
    if (HAL_QSPI_Transmit(&QSPIHandle, pData, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return QSPI_ERROR;

    /* Configure automatic polling mode to wait for end of program */
    if (QSPI_AutoPollingMemReady(&QSPIHandle, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != QSPI_OK)
      return QSPI_ERROR;

    /* Update the address and size variables for next page programming */
    current_addr += current_size;
    pData += current_size;
    current_size = ((current_addr + N25Q128A_PAGE_SIZE) > end_addr) ? (end_addr - current_addr) : N25Q128A_PAGE_SIZE;
    } while (current_addr < end_addr);

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_QSPI_Erase_Block (uint32_t BlockAddress) {

  QSPI_CommandTypeDef s_command;

  /* Initialize the erase command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = SUBSECTOR_ERASE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_1_LINE;
  s_command.AddressSize       = QSPI_ADDRESS_24_BITS;
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
  if (QSPI_AutoPollingMemReady (&QSPIHandle, N25Q128A_SUBSECTOR_ERASE_MAX_TIME) != QSPI_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_Erase_Chip() {

  QSPI_CommandTypeDef s_command;

  /* Initialize the erase command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = BULK_ERASE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
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
  if (QSPI_AutoPollingMemReady (&QSPIHandle, N25Q128A_BULK_ERASE_MAX_TIME) != QSPI_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
uint8_t BSP_QSPI_GetStatus() {

  QSPI_CommandTypeDef s_command;
  uint8_t reg;

  /* Initialize the read flag status register command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = READ_FLAG_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_1_LINE;
  s_command.DummyCycles       = 0;
  s_command.NbData            = 1;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
  if (HAL_QSPI_Command(&QSPIHandle, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Reception of the data */
  if (HAL_QSPI_Receive(&QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return QSPI_ERROR;

  /* Check the value of the register */
  if ((reg & (N25Q128A_FSR_PRERR | N25Q128A_FSR_VPPERR | N25Q128A_FSR_PGERR | N25Q128A_FSR_ERERR)) != 0)
    return QSPI_ERROR;
  else if ((reg & (N25Q128A_FSR_PGSUS | N25Q128A_FSR_ERSUS)) != 0)
    return QSPI_SUSPENDED;
  else if ((reg & N25Q128A_FSR_READY) != 0)
    return QSPI_OK;
  else
    return QSPI_BUSY;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_GetInfo (QSPI_Info* pInfo) {

  /* Configure the structure with the memory configuration */
  pInfo->FlashSize          = N25Q128A_FLASH_SIZE;
  pInfo->EraseSectorSize    = N25Q128A_SUBSECTOR_SIZE;
  pInfo->EraseSectorsNumber = (N25Q128A_FLASH_SIZE/N25Q128A_SUBSECTOR_SIZE);
  pInfo->ProgPageSize       = N25Q128A_PAGE_SIZE;
  pInfo->ProgPagesNumber    = (N25Q128A_FLASH_SIZE/N25Q128A_PAGE_SIZE);

  return QSPI_OK;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_QSPI_EnableMemoryMappedMode() {

  QSPI_CommandTypeDef      s_command;
  QSPI_MemoryMappedTypeDef s_mem_mapped_cfg;

  /* Configure the command for the read instruction */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = QUAD_INOUT_FAST_READ_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_24_BITS;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = N25Q128A_DUMMY_CYCLES_READ_QUAD;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Configure the memory mapped mode */
  s_mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  s_mem_mapped_cfg.TimeOutPeriod     = 0;
  if (HAL_QSPI_MemoryMapped(&QSPIHandle, &s_command, &s_mem_mapped_cfg) != HAL_OK)
    return QSPI_ERROR;

  return QSPI_OK;
  }
/*}}}*/

/*{{{*/
__weak void BSP_QSPI_MspInit (QSPI_HandleTypeDef* hqspi, void* Params) {

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

  /*{{{  QSPI CS GPIO pin configuration*/
  GPIO_InitTypeDef gpio_init_structure;
  gpio_init_structure.Pin       = QSPI_CS_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  gpio_init_structure.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(QSPI_CS_GPIO_PORT, &gpio_init_structure);
  /*}}}*/
  /*{{{  QSPI CLK GPIO pin configuration*/
  gpio_init_structure.Pin       = QSPI_CLK_PIN;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_CLK_GPIO_PORT, &gpio_init_structure);
  /*}}}*/
  /*{{{  QSPI D0 GPIO pin configuration*/
  gpio_init_structure.Pin       = QSPI_D0_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D0_GPIO_PORT, &gpio_init_structure);

  /*}}}*/
  /*{{{  QSPI D1 GPIO pin configuration*/
  gpio_init_structure.Pin       = QSPI_D1_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D1_GPIO_PORT, &gpio_init_structure);
  /*}}}*/
  /*{{{  QSPI D2 GPIO pin configuration*/
  gpio_init_structure.Pin       = QSPI_D2_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D2_GPIO_PORT, &gpio_init_structure);
  /*}}}*/
  /*{{{  QSPI D3 GPIO pin configuration*/
  gpio_init_structure.Pin       = QSPI_D3_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D3_GPIO_PORT, &gpio_init_structure);
  /*}}}*/

  HAL_NVIC_SetPriority (QUADSPI_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ (QUADSPI_IRQn);
  }
/*}}}*/
/*{{{*/
__weak void BSP_QSPI_MspDeInit (QSPI_HandleTypeDef* hqspi, void* Params) {

  HAL_NVIC_DisableIRQ(QUADSPI_IRQn);

  /* De-Configure QSPI pins */
  HAL_GPIO_DeInit(QSPI_CS_GPIO_PORT, QSPI_CS_PIN);
  HAL_GPIO_DeInit(QSPI_CLK_GPIO_PORT, QSPI_CLK_PIN);
  HAL_GPIO_DeInit(QSPI_D0_GPIO_PORT, QSPI_D0_PIN);
  HAL_GPIO_DeInit(QSPI_D1_GPIO_PORT, QSPI_D1_PIN);
  HAL_GPIO_DeInit(QSPI_D2_GPIO_PORT, QSPI_D2_PIN);
  HAL_GPIO_DeInit(QSPI_D3_GPIO_PORT, QSPI_D3_PIN);

  /* Reset the QuadSPI memory interface */
  QSPI_FORCE_RESET();
  QSPI_RELEASE_RESET();

  /* Disable the QuadSPI memory interface clock */
  QSPI_CLK_DISABLE();
  }
/*}}}*/
#endif
