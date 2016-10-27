#include "usbd_core.h"

/*{{{*/
USBD_StatusTypeDef USBD_Init (USBD_HandleTypeDef *pdev, USBD_DescriptorsTypeDef *pdesc, uint8_t id) {

  /* Check whether the USB Host handle is valid */
  if (pdev == NULL) {
    USBD_ErrLog ("Invalid Device handle");
    return USBD_FAIL;
    }

  /* Unlink previous class*/
  if (pdev->pClass != NULL)
    pdev->pClass = NULL;

  /* Assign USBD Descriptors */
  if (pdesc != NULL)
    pdev->pDesc = pdesc;

  /* Set Device initial State */
  pdev->dev_state = USBD_STATE_DEFAULT;
  pdev->id = id;

  /* Initialize low level driver */
  USBD_LL_Init (pdev);

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_DeInit (USBD_HandleTypeDef *pdev) {

  /* Set Default State */
  pdev->dev_state = USBD_STATE_DEFAULT;

  /* Free Class Resources */
  pdev->pClass->DeInit (pdev, pdev->dev_config);

    /* Stop the low level driver  */
  USBD_LL_Stop (pdev);

  /* Initialize low level driver */
  USBD_LL_DeInit (pdev);

  return USBD_OK;
  }
/*}}}*/

/*{{{*/
USBD_StatusTypeDef USBD_RegisterClass (USBD_HandleTypeDef *pdev, USBD_ClassTypeDef *pclass) {

  USBD_StatusTypeDef   status = USBD_OK;
  if (pclass != 0) {
    /* link the class to the USB Device handle */
    pdev->pClass = pclass;
    status = USBD_OK;
    }
  else {
    USBD_ErrLog("Invalid Class handle");
    status = USBD_FAIL;
    }

  return status;
  }
/*}}}*/

/*{{{*/
USBD_StatusTypeDef USBD_Start (USBD_HandleTypeDef *pdev) {
  USBD_LL_Start (pdev);
  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_Stop (USBD_HandleTypeDef *pdev) {

  /* Free Class Resources */
  pdev->pClass->DeInit (pdev, pdev->dev_config);

  /* Stop the low level driver  */
  USBD_LL_Stop (pdev);

  return USBD_OK;
  }
/*}}}*/

USBD_StatusTypeDef USBD_RunTestMode (USBD_HandleTypeDef *pdev) { return USBD_OK; }

/*{{{*/
USBD_StatusTypeDef USBD_SetClassConfig (USBD_HandleTypeDef *pdev, uint8_t cfgidx) {

  USBD_StatusTypeDef   ret = USBD_FAIL;

  if(pdev->pClass != NULL) {
    /* Set configuration  and Start the Class*/
    if(pdev->pClass->Init(pdev, cfgidx) == 0)
      ret = USBD_OK;
    }
  return ret;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_ClrClassConfig (USBD_HandleTypeDef *pdev, uint8_t cfgidx) {

  /* Clear configuration  and De-initialize the Class process*/
  pdev->pClass->DeInit(pdev, cfgidx);
  return USBD_OK;
  }
/*}}}*/

/*{{{*/
USBD_StatusTypeDef USBD_LL_SetupStage (USBD_HandleTypeDef* pdev, uint8_t* psetup) {

  USBD_ParseSetupRequest (&pdev->request, psetup);

  pdev->ep0_state = USBD_EP0_SETUP;
  pdev->ep0_data_len = pdev->request.wLength;

  switch (pdev->request.bmRequest & 0x1F) {
    case USB_REQ_RECIPIENT_DEVICE:
      USBD_StdDevReq (pdev, &pdev->request);
      break;

    case USB_REQ_RECIPIENT_INTERFACE:
      USBD_StdItfReq (pdev, &pdev->request);
      break;

    case USB_REQ_RECIPIENT_ENDPOINT:
      USBD_StdEPReq (pdev, &pdev->request);
      break;

    default:
      USBD_LL_StallEP (pdev , pdev->request.bmRequest & 0x80);
      break;
    }

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_LL_DataOutStage (USBD_HandleTypeDef *pdev , uint8_t epnum, uint8_t *pdata) {

  USBD_EndpointTypeDef    *pep;

  if (epnum == 0) {
    pep = &pdev->ep_out[0];

    if (pdev->ep0_state == USBD_EP0_DATA_OUT) {
      if (pep->rem_length > pep->maxpacket) {
        pep->rem_length -=  pep->maxpacket;
        USBD_CtlContinueRx (pdev, pdata, MIN(pep->rem_length, pep->maxpacket));
        }
      else {
        if ((pdev->pClass->EP0_RxReady != NULL) && (pdev->dev_state == USBD_STATE_CONFIGURED))
          pdev->pClass->EP0_RxReady (pdev);
        USBD_CtlSendStatus (pdev);
        }
      }
    }
  else if ((pdev->pClass->DataOut != NULL)&& (pdev->dev_state == USBD_STATE_CONFIGURED))
    pdev->pClass->DataOut (pdev, epnum);

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_LL_DataInStage (USBD_HandleTypeDef *pdev ,uint8_t epnum, uint8_t *pdata) {

  USBD_EndpointTypeDef* pep;

  if (epnum == 0) {
    pep = &pdev->ep_in[0];
    if (pdev->ep0_state == USBD_EP0_DATA_IN) {
      if (pep->rem_length > pep->maxpacket) {
        pep->rem_length -=  pep->maxpacket;
        USBD_CtlContinueSendData (pdev, pdata, pep->rem_length);

        /* Prepare endpoint for premature end of transfer */
        USBD_LL_PrepareReceive (pdev, 0, NULL, 0);
        }

      else {
        /* last packet is MPS multiple, so send ZLP packet */
        if ((pep->total_length % pep->maxpacket == 0) && (pep->total_length >= pep->maxpacket) &&
            (pep->total_length < pdev->ep0_data_len )) {
          USBD_CtlContinueSendData(pdev , NULL, 0);
          pdev->ep0_data_len = 0;

          /* Prepare endpoint for premature end of transfer */
          USBD_LL_PrepareReceive (pdev, 0, NULL, 0);
          }

        else {
          if ((pdev->pClass->EP0_TxSent != NULL) && (pdev->dev_state == USBD_STATE_CONFIGURED))
            pdev->pClass->EP0_TxSent(pdev);
          USBD_CtlReceiveStatus(pdev);
          }
        }
      }

    if (pdev->dev_test_mode == 1) {
      USBD_RunTestMode(pdev);
      pdev->dev_test_mode = 0;
      }
    }

  else if ((pdev->pClass->DataIn != NULL)&& (pdev->dev_state == USBD_STATE_CONFIGURED))
    pdev->pClass->DataIn(pdev, epnum);

  return USBD_OK;
  }
/*}}}*/

/*{{{*/
USBD_StatusTypeDef USBD_LL_Reset (USBD_HandleTypeDef *pdev) {

  /* Open EP0 OUT */
  USBD_LL_OpenEP (pdev, 0x00, USBD_EP_TYPE_CTRL, USB_MAX_EP0_SIZE);
  pdev->ep_out[0].maxpacket = USB_MAX_EP0_SIZE;

  /* Open EP0 IN */
  USBD_LL_OpenEP (pdev, 0x80, USBD_EP_TYPE_CTRL, USB_MAX_EP0_SIZE);
  pdev->ep_in[0].maxpacket = USB_MAX_EP0_SIZE;

  /* Upon Reset call user call back */
  pdev->dev_state = USBD_STATE_DEFAULT;

  if (pdev->pClassData)
    pdev->pClass->DeInit (pdev, pdev->dev_config);

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_LL_SetSpeed (USBD_HandleTypeDef *pdev, USBD_SpeedTypeDef speed) {
  pdev->dev_speed = speed;
  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_LL_Suspend (USBD_HandleTypeDef *pdev) {
  pdev->dev_old_state =  pdev->dev_state;
  pdev->dev_state  = USBD_STATE_SUSPENDED;
  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_LL_Resume (USBD_HandleTypeDef *pdev) {
  pdev->dev_state = pdev->dev_old_state;
  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_LL_SOF (USBD_HandleTypeDef *pdev) {

  if (pdev->dev_state == USBD_STATE_CONFIGURED)
    if (pdev->pClass->SOF != NULL)
      pdev->pClass->SOF (pdev);

  return USBD_OK;
  }
/*}}}*/

USBD_StatusTypeDef USBD_LL_IsoINIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum) { return USBD_OK; }
USBD_StatusTypeDef USBD_LL_IsoOUTIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum) { return USBD_OK; }

USBD_StatusTypeDef USBD_LL_DevConnected (USBD_HandleTypeDef *pdev) { return USBD_OK; }
/*{{{*/
USBD_StatusTypeDef USBD_LL_DevDisconnected (USBD_HandleTypeDef *pdev) {

  /* Free Class Resources */
  pdev->dev_state = USBD_STATE_DEFAULT;
  pdev->pClass->DeInit(pdev, pdev->dev_config);

  return USBD_OK;
  }
/*}}}*/
