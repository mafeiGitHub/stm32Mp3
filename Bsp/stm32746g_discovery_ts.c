/*{{{  stm32746g_discovery_ts.c  25-June-2015*/
//      - This driver is used to drive the touch screen module of the STM32746G-Discovery
//        board on the RK043FN48H-CT672B 480x272 LCD screen with capacitive touch screen.
//      - The FT5336 component driver must be included in project files according to
//        the touch screen driver present on this board.
//     + Initialization steps:
//        o Initialize the TS module using the BSP_TS_Init() function. This
//          function includes the MSP layer hardware resources initialization and the
//          communication layer configuration to start the TS use. The LCD size properties
//          (x and y) are passed as parameters.
//        o If TS interrupt mode is desired, you must configure the TS interrupt mode
//          by calling the function BSP_TS_ITConfig(). The TS interrupt mode is generated
//          as an external interrupt whenever a touch is detected.
//          The interrupt mode internally uses the IO functionalities driver driven by
//          the IO expander, to configure the IT line.
//
//     + Touch screen use
//        o The touch screen state is captured whenever the function BSP_TS_GetState() is
//          used. This function returns information about the last LCD touch occurred
//          in the TS_StateTypeDef structure.
//        o If TS interrupt mode is used, the function BSP_TS_ITGetStatus() is needed to get
//          the interrupt status. To clear the IT pending bits, you should call the
//          function BSP_TS_ITClear().
//        o The IT is handled using the corresponding external interrupt IRQ handler,
//          the user IT callback treatment is implemented on the same external interrupt
//          callback.
/*}}}*/
#include "stm32746g_discovery_ts.h"

static TS_DrvTypeDef* tsDriver;
static uint16_t tsXBoundary, tsYBoundary;
static uint8_t tsOrientation;
static uint8_t I2cAddress;

/*{{{*/
uint8_t BSP_TS_Init (uint16_t ts_SizeX, uint16_t ts_SizeY) {

  uint8_t status = TS_OK;
  tsXBoundary = ts_SizeX;
  tsYBoundary = ts_SizeY;

  /* Read ID and verify if the touch screen driver is ready */
  ft5336_ts_drv.Init(TS_I2C_ADDRESS);
  if (ft5336_ts_drv.ReadID(TS_I2C_ADDRESS) == FT5336_ID_VALUE) {
    /* Initialize the TS driver structure */
    tsDriver = &ft5336_ts_drv;
    I2cAddress = TS_I2C_ADDRESS;
    tsOrientation = TS_SWAP_XY;

    /* Initialize the TS driver */
    tsDriver->Start(I2cAddress);
    }
  else
    status = TS_DEVICE_NOT_FOUND;

  return status;
  }
/*}}}*/
/*{{{*/
uint8_t BSP_TS_DeInit()
{
  /* Actually ts_driver does not provide a DeInit function */
  return TS_OK;
}
/*}}}*/

/*{{{*/
uint8_t BSP_TS_ITConfig()
{
  GPIO_InitTypeDef gpio_init_structure;

  /* Configure Interrupt mode for SD detection pin */
  gpio_init_structure.Pin = TS_INT_PIN;
  gpio_init_structure.Pull = GPIO_NOPULL;
  gpio_init_structure.Speed = GPIO_SPEED_FAST;
  gpio_init_structure.Mode = GPIO_MODE_IT_RISING;
  HAL_GPIO_Init(TS_INT_GPIO_PORT, &gpio_init_structure);

  /* Enable and set Touch screen EXTI Interrupt to the lowest priority */
  HAL_NVIC_SetPriority((IRQn_Type)(TS_INT_EXTI_IRQn), 0x0F, 0x00);
  HAL_NVIC_EnableIRQ((IRQn_Type)(TS_INT_EXTI_IRQn));

  /* Enable the TS ITs */
  tsDriver->EnableIT(I2cAddress);

  return TS_OK;
}
/*}}}*/
/*{{{*/
uint8_t BSP_TS_ITGetStatus ()
{
  /* Return the TS IT status */
  return (tsDriver->GetITStatus(I2cAddress));
}
/*}}}*/
/*{{{*/
void BSP_TS_ITClear()
{
  /* Clear TS IT pending bits */
  tsDriver->ClearIT(I2cAddress);
}
/*}}}*/

/*{{{*/
uint8_t BSP_TS_GetState (TS_StateTypeDef* TS_State) {

  static uint32_t _x[TS_MAX_NB_TOUCH] = {0, 0};
  static uint32_t _y[TS_MAX_NB_TOUCH] = {0, 0};

  uint8_t ts_status = TS_OK;
  uint16_t x[TS_MAX_NB_TOUCH];
  uint16_t y[TS_MAX_NB_TOUCH];
  uint16_t brute_x[TS_MAX_NB_TOUCH];
  uint16_t brute_y[TS_MAX_NB_TOUCH];
  uint16_t x_diff;
  uint16_t y_diff;
  uint32_t index;
  uint32_t weight = 0;
  uint32_t area = 0;
  uint32_t event = 0;

  /* Check and update the number of touches active detected */
  TS_State->touchDetected = tsDriver->DetectTouch(I2cAddress);

  if (TS_State->touchDetected) {
    for(index=0; index < TS_State->touchDetected; index++) {
      /* Get each touch coordinates */
      tsDriver->GetXY(I2cAddress, &(brute_x[index]), &(brute_y[index]));

      if(tsOrientation == TS_SWAP_NONE) {
        x[index] = brute_x[index];
        y[index] = brute_y[index];
        }

      if(tsOrientation & TS_SWAP_X)
        x[index] = 4096 - brute_x[index];

      if(tsOrientation & TS_SWAP_Y)
        y[index] = 4096 - brute_y[index];

      if(tsOrientation & TS_SWAP_XY) {
        y[index] = brute_x[index];
        x[index] = brute_y[index];
        }

      x_diff = x[index] > _x[index]? (x[index] - _x[index]): (_x[index] - x[index]);
      y_diff = y[index] > _y[index]? (y[index] - _y[index]): (_y[index] - y[index]);

      if ((x_diff + y_diff) > 5) {
        _x[index] = x[index];
        _y[index] = y[index];
        }

      if(I2cAddress == FT5336_I2C_SLAVE_ADDRESS) {
        TS_State->touchX[index] = x[index];
        TS_State->touchY[index] = y[index];
        }
      else {
        /* 2^12 = 4096 : indexes are expressed on a dynamic of 4096 */
        TS_State->touchX[index] = (tsXBoundary * _x[index]) >> 12;
        TS_State->touchY[index] = (tsYBoundary * _y[index]) >> 12;
        }

      /* Get touch info related to the current touch */
      ft5336_TS_GetTouchInfo(I2cAddress, index, &weight, &area, &event);

      /* Update TS_State structure */
      TS_State->touchWeight[index] = weight;
      TS_State->touchArea[index]   = area;

      /* Remap touch event */
      switch(event) {
        case FT5336_TOUCH_EVT_FLAG_PRESS_DOWN :
          TS_State->touchEventId[index] = TOUCH_EVENT_PRESS_DOWN;
          break;
        case FT5336_TOUCH_EVT_FLAG_LIFT_UP :
          TS_State->touchEventId[index] = TOUCH_EVENT_LIFT_UP;
          break;
        case FT5336_TOUCH_EVT_FLAG_CONTACT :
          TS_State->touchEventId[index] = TOUCH_EVENT_CONTACT;
          break;
        case FT5336_TOUCH_EVT_FLAG_NO_EVENT :
          TS_State->touchEventId[index] = TOUCH_EVENT_NO_EVT;
          break;
        default :
          ts_status = TS_ERROR;
          break;
        } /* of switch(event) */

      } /* of for(index=0; index < TS_State->touchDetected; index++) */

    /* Get gesture Id */
    ts_status = BSP_TS_Get_GestureId(TS_State);
    } /* end of if(TS_State->touchDetected != 0) */

  return (ts_status);
  }
/*}}}*/
/*{{{*/
uint8_t BSP_TS_Get_GestureId (TS_StateTypeDef* TS_State) {

  uint32_t gestureId = 0;
  uint8_t  ts_status = TS_OK;

  /* Get gesture Id */
  ft5336_TS_GetGestureID (I2cAddress, &gestureId);

  /* Remap gesture Id to a TS_GestureIdTypeDef value */
  switch(gestureId) {
    case FT5336_GEST_ID_NO_GESTURE :
      TS_State->gestureId = GEST_ID_NO_GESTURE;
      break;
    case FT5336_GEST_ID_MOVE_UP :
      TS_State->gestureId = GEST_ID_MOVE_UP;
      break;
    case FT5336_GEST_ID_MOVE_RIGHT :
      TS_State->gestureId = GEST_ID_MOVE_RIGHT;
      break;
    case FT5336_GEST_ID_MOVE_DOWN :
      TS_State->gestureId = GEST_ID_MOVE_DOWN;
      break;
    case FT5336_GEST_ID_MOVE_LEFT :
      TS_State->gestureId = GEST_ID_MOVE_LEFT;
      break;
    case FT5336_GEST_ID_ZOOM_IN :
      TS_State->gestureId = GEST_ID_ZOOM_IN;
      break;
    case FT5336_GEST_ID_ZOOM_OUT :
      TS_State->gestureId = GEST_ID_ZOOM_OUT;
      break;
    default :
      ts_status = TS_ERROR;
      break;
    } /* of switch(gestureId) */

  return(ts_status);
  }
/*}}}*/

/*{{{*/
uint8_t BSP_TS_ResetTouchData (TS_StateTypeDef* TS_State) {

  uint8_t ts_status = TS_ERROR;
  uint32_t index;

  if (TS_State != (TS_StateTypeDef *)NULL) {
    TS_State->gestureId = GEST_ID_NO_GESTURE;
    TS_State->touchDetected = 0;

    for (index = 0; index < TS_MAX_NB_TOUCH; index++) {
      TS_State->touchX[index]       = 0;
      TS_State->touchY[index]       = 0;
      TS_State->touchArea[index]    = 0;
      TS_State->touchEventId[index] = TOUCH_EVENT_NO_EVT;
      TS_State->touchWeight[index]  = 0;
      }

    ts_status = TS_OK;
    } /* of if (TS_State != (TS_StateTypeDef *)NULL) */

  return (ts_status);
  }
/*}}}*/
