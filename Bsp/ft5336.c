// ft5336.c version V1.0.0 25-June-2015
#include "ft5336.h"

/*{{{*/
TS_DrvTypeDef ft5336_ts_drv = {
  ft5336_Init,
  ft5336_ReadID,
  ft5336_Reset,

  ft5336_TS_Start,
  ft5336_TS_DetectTouch,
  ft5336_TS_GetXY,

  ft5336_TS_EnableIT,
  ft5336_TS_ClearIT,
  ft5336_TS_ITStatus,
  ft5336_TS_DisableIT
  };
/*}}}*/
static ft5336_handle_TypeDef ft5336_handle = { FT5336_I2C_NOT_INITIALIZED, 0, 0};

/*{{{*/
static uint8_t ft5336_Get_I2C_InitializedStatus()
{
  return(ft5336_handle.i2cInitialized);
}
/*}}}*/
/*{{{*/
static void ft5336_I2C_InitializeIfRequired()
{
  if(ft5336_Get_I2C_InitializedStatus() == FT5336_I2C_NOT_INITIALIZED)
  {
    /* Initialize TS IO BUS layer (I2C) */
    TS_IO_Init();

    /* Set state to initialized */
    ft5336_handle.i2cInitialized = FT5336_I2C_INITIALIZED;
  }
}
/*}}}*/
/*{{{*/
static uint32_t ft5336_TS_Configure (uint16_t DeviceAddr)
{
  uint32_t status = FT5336_STATUS_OK;

  /* Nothing special to be done for FT5336 */

  return(status);
}
/*}}}*/

/*{{{*/
void ft5336_Init (uint16_t DeviceAddr)
{
  /* Wait at least 200ms after power up before accessing registers
   * Trsi timing (Time of starting to report point after resetting) from FT5336GQQ datasheet */
  TS_IO_Delay (200);

  /* Initialize I2C link if needed */
  ft5336_I2C_InitializeIfRequired();
}
/*}}}*/
/*{{{*/
void ft5336_Reset (uint16_t DeviceAddr)
{
  /* Do nothing */
  /* No software reset sequence available in FT5336 IC */
}
/*}}}*/
/*{{{*/
uint16_t ft5336_ReadID (uint16_t DeviceAddr)
{
  volatile uint8_t ucReadId = 0;

  uint8_t nbReadAttempts = 0;
  uint8_t bFoundDevice = 0; /* Device not found by default */

  /* Initialize I2C link if needed */
  ft5336_I2C_InitializeIfRequired();

  /* At maximum 4 attempts to read ID : exit at first finding of the searched device ID */
  for (nbReadAttempts = 0; ((nbReadAttempts < 3) && !(bFoundDevice)); nbReadAttempts++)
  {
    /* Read register FT5336_CHIP_ID_REG as DeviceID detection */
    ucReadId = TS_IO_Read(DeviceAddr, FT5336_CHIP_ID_REG);

    /* Found the searched device ID ? */
    if(ucReadId == FT5336_ID_VALUE)
    {
      /* Set device as found */
      bFoundDevice = 1;
    }
  }

  /* Return the device ID value */
  return (ucReadId);
}
/*}}}*/

/*{{{*/
void ft5336_TS_Start (uint16_t DeviceAddr)
{
  /* Minimum static configuration of FT5336 */
  FT5336_ASSERT(ft5336_TS_Configure(DeviceAddr));

  /* By default set FT5336 IC in Polling mode : no INT generation on FT5336 for new touch available */
  /* Note TS_INT is active low                                                                      */
  ft5336_TS_DisableIT(DeviceAddr);
}
/*}}}*/
/*{{{*/
uint8_t ft5336_TS_DetectTouch (uint16_t DeviceAddr)
{
  volatile uint8_t nbTouch = 0;

  /* Read register FT5336_TD_STAT_REG to check number of touches detection */
  nbTouch = TS_IO_Read(DeviceAddr, FT5336_TD_STAT_REG);
  nbTouch &= FT5336_TD_STAT_MASK;

  if(nbTouch > FT5336_MAX_DETECTABLE_TOUCH)
    /* If invalid number of touch detected, set it to zero */
    nbTouch = 0;

  /* Update ft5336 driver internal global : current number of active touches */
  ft5336_handle.currActiveTouchNb = nbTouch;

  /* Reset current active touch index on which to work on */
  ft5336_handle.currActiveTouchIdx = 0;

  return(nbTouch);
}
/*}}}*/
/*{{{*/
void ft5336_TS_GetXY (uint16_t DeviceAddr, uint16_t* X, uint16_t* Y)
{
  volatile uint8_t ucReadData = 0;
  static uint16_t coord;
  uint8_t regAddressXLow = 0;
  uint8_t regAddressXHigh = 0;
  uint8_t regAddressYLow = 0;
  uint8_t regAddressYHigh = 0;

  if(ft5336_handle.currActiveTouchIdx < ft5336_handle.currActiveTouchNb)
  {
    switch(ft5336_handle.currActiveTouchIdx)
    {
    case 0 :
      regAddressXLow  = FT5336_P1_XL_REG;
      regAddressXHigh = FT5336_P1_XH_REG;
      regAddressYLow  = FT5336_P1_YL_REG;
      regAddressYHigh = FT5336_P1_YH_REG;
      break;

    case 1 :
      regAddressXLow  = FT5336_P2_XL_REG;
      regAddressXHigh = FT5336_P2_XH_REG;
      regAddressYLow  = FT5336_P2_YL_REG;
      regAddressYHigh = FT5336_P2_YH_REG;
      break;

    case 2 :
      regAddressXLow  = FT5336_P3_XL_REG;
      regAddressXHigh = FT5336_P3_XH_REG;
      regAddressYLow  = FT5336_P3_YL_REG;
      regAddressYHigh = FT5336_P3_YH_REG;
      break;

    case 3 :
      regAddressXLow  = FT5336_P4_XL_REG;
      regAddressXHigh = FT5336_P4_XH_REG;
      regAddressYLow  = FT5336_P4_YL_REG;
      regAddressYHigh = FT5336_P4_YH_REG;
      break;

    case 4 :
      regAddressXLow  = FT5336_P5_XL_REG;
      regAddressXHigh = FT5336_P5_XH_REG;
      regAddressYLow  = FT5336_P5_YL_REG;
      regAddressYHigh = FT5336_P5_YH_REG;
      break;

    case 5 :
      regAddressXLow  = FT5336_P6_XL_REG;
      regAddressXHigh = FT5336_P6_XH_REG;
      regAddressYLow  = FT5336_P6_YL_REG;
      regAddressYHigh = FT5336_P6_YH_REG;
      break;

    case 6 :
      regAddressXLow  = FT5336_P7_XL_REG;
      regAddressXHigh = FT5336_P7_XH_REG;
      regAddressYLow  = FT5336_P7_YL_REG;
      regAddressYHigh = FT5336_P7_YH_REG;
      break;

    case 7 :
      regAddressXLow  = FT5336_P8_XL_REG;
      regAddressXHigh = FT5336_P8_XH_REG;
      regAddressYLow  = FT5336_P8_YL_REG;
      regAddressYHigh = FT5336_P8_YH_REG;
      break;

    case 8 :
      regAddressXLow  = FT5336_P9_XL_REG;
      regAddressXHigh = FT5336_P9_XH_REG;
      regAddressYLow  = FT5336_P9_YL_REG;
      regAddressYHigh = FT5336_P9_YH_REG;
      break;

    case 9 :
      regAddressXLow  = FT5336_P10_XL_REG;
      regAddressXHigh = FT5336_P10_XH_REG;
      regAddressYLow  = FT5336_P10_YL_REG;
      regAddressYHigh = FT5336_P10_YH_REG;
      break;

    default :
      break;

    } /* end switch(ft5336_handle.currActiveTouchIdx) */

    /* Read low part of X position */
    ucReadData = TS_IO_Read(DeviceAddr, regAddressXLow);
    coord = (ucReadData & FT5336_TOUCH_POS_LSB_MASK) >> FT5336_TOUCH_POS_LSB_SHIFT;

    /* Read high part of X position */
    ucReadData = TS_IO_Read(DeviceAddr, regAddressXHigh);
    coord |= ((ucReadData & FT5336_TOUCH_POS_MSB_MASK) >> FT5336_TOUCH_POS_MSB_SHIFT) << 8;

    /* Send back ready X position to caller */
    *X = coord;

    /* Read low part of Y position */
    ucReadData = TS_IO_Read(DeviceAddr, regAddressYLow);
    coord = (ucReadData & FT5336_TOUCH_POS_LSB_MASK) >> FT5336_TOUCH_POS_LSB_SHIFT;

    /* Read high part of Y position */
    ucReadData = TS_IO_Read(DeviceAddr, regAddressYHigh);
    coord |= ((ucReadData & FT5336_TOUCH_POS_MSB_MASK) >> FT5336_TOUCH_POS_MSB_SHIFT) << 8;

    /* Send back ready Y position to caller */
    *Y = coord;

    ft5336_handle.currActiveTouchIdx++; /* next call will work on next touch */

  } /* of if(ft5336_handle.currActiveTouchIdx < ft5336_handle.currActiveTouchNb) */
}
/*}}}*/

/*{{{*/
void ft5336_TS_EnableIT (uint16_t DeviceAddr)
{
   uint8_t regValue = 0;
   regValue = (FT5336_G_MODE_INTERRUPT_TRIGGER & (FT5336_G_MODE_INTERRUPT_MASK >> FT5336_G_MODE_INTERRUPT_SHIFT)) << FT5336_G_MODE_INTERRUPT_SHIFT;

   /* Set interrupt trigger mode in FT5336_GMODE_REG */
   TS_IO_Write(DeviceAddr, FT5336_GMODE_REG, regValue);
}
/*}}}*/
/*{{{*/
void ft5336_TS_DisableIT (uint16_t DeviceAddr)
{
  uint8_t regValue = 0;
  regValue = (FT5336_G_MODE_INTERRUPT_POLLING & (FT5336_G_MODE_INTERRUPT_MASK >> FT5336_G_MODE_INTERRUPT_SHIFT)) << FT5336_G_MODE_INTERRUPT_SHIFT;

  /* Set interrupt polling mode in FT5336_GMODE_REG */
  TS_IO_Write(DeviceAddr, FT5336_GMODE_REG, regValue);
}
/*}}}*/
/*{{{*/
uint8_t ft5336_TS_ITStatus (uint16_t DeviceAddr)
{
  /* Always return 0 as feature not applicable to FT5336 */
  return 0;
}
/*}}}*/
/*{{{*/
void ft5336_TS_ClearIT (uint16_t DeviceAddr)
{
  /* Nothing to be done here for FT5336 */
}
/*}}}*/

/*{{{*/
void ft5336_TS_GetGestureID (uint16_t DeviceAddr, uint32_t* pGestureId) {

  volatile uint8_t ucReadData = 0;

  ucReadData = TS_IO_Read(DeviceAddr, FT5336_GEST_ID_REG);

  *pGestureId = ucReadData;
  }
/*}}}*/
/*{{{*/
void ft5336_TS_GetTouchInfo (uint16_t DeviceAddr, uint32_t touchIdx, uint32_t* pWeight, uint32_t* pArea, uint32_t* pEvent) {

  volatile uint8_t ucReadData = 0;
  uint8_t regAddressXHigh = 0;
  uint8_t regAddressPWeight = 0;
  uint8_t regAddressPMisc = 0;

  if (touchIdx < ft5336_handle.currActiveTouchNb) {
    switch (touchIdx) {
    case 0 :
      regAddressXHigh   = FT5336_P1_XH_REG;
      regAddressPWeight = FT5336_P1_WEIGHT_REG;
      regAddressPMisc   = FT5336_P1_MISC_REG;
      break;

    case 1 :
      regAddressXHigh   = FT5336_P2_XH_REG;
      regAddressPWeight = FT5336_P2_WEIGHT_REG;
      regAddressPMisc   = FT5336_P2_MISC_REG;
      break;

    case 2 :
      regAddressXHigh   = FT5336_P3_XH_REG;
      regAddressPWeight = FT5336_P3_WEIGHT_REG;
      regAddressPMisc   = FT5336_P3_MISC_REG;
      break;

    case 3 :
      regAddressXHigh   = FT5336_P4_XH_REG;
      regAddressPWeight = FT5336_P4_WEIGHT_REG;
      regAddressPMisc   = FT5336_P4_MISC_REG;
      break;

    case 4 :
      regAddressXHigh   = FT5336_P5_XH_REG;
      regAddressPWeight = FT5336_P5_WEIGHT_REG;
      regAddressPMisc   = FT5336_P5_MISC_REG;
      break;

    case 5 :
      regAddressXHigh   = FT5336_P6_XH_REG;
      regAddressPWeight = FT5336_P6_WEIGHT_REG;
      regAddressPMisc   = FT5336_P6_MISC_REG;
      break;

    case 6 :
      regAddressXHigh   = FT5336_P7_XH_REG;
      regAddressPWeight = FT5336_P7_WEIGHT_REG;
      regAddressPMisc   = FT5336_P7_MISC_REG;
      break;

    case 7 :
      regAddressXHigh   = FT5336_P8_XH_REG;
      regAddressPWeight = FT5336_P8_WEIGHT_REG;
      regAddressPMisc   = FT5336_P8_MISC_REG;
      break;

    case 8 :
      regAddressXHigh   = FT5336_P9_XH_REG;
      regAddressPWeight = FT5336_P9_WEIGHT_REG;
      regAddressPMisc   = FT5336_P9_MISC_REG;
      break;

    case 9 :
      regAddressXHigh   = FT5336_P10_XH_REG;
      regAddressPWeight = FT5336_P10_WEIGHT_REG;
      regAddressPMisc   = FT5336_P10_MISC_REG;
      break;

    default :
      break;
      } /* end switch(touchIdx) */

    /* Read Event Id of touch index */
    ucReadData = TS_IO_Read(DeviceAddr, regAddressXHigh);
    *pEvent = (ucReadData & FT5336_TOUCH_EVT_FLAG_MASK) >> FT5336_TOUCH_EVT_FLAG_SHIFT;

    /* Read weight of touch index */
    ucReadData = TS_IO_Read(DeviceAddr, regAddressPWeight);
    *pWeight = (ucReadData & FT5336_TOUCH_WEIGHT_MASK) >> FT5336_TOUCH_WEIGHT_SHIFT;

    /* Read area of touch index */
    ucReadData = TS_IO_Read(DeviceAddr, regAddressPMisc);
    *pArea = (ucReadData & FT5336_TOUCH_AREA_MASK) >> FT5336_TOUCH_AREA_SHIFT;
   } /* of if(touchIdx < ft5336_handle.currActiveTouchNb) */
  }
/*}}}*/
