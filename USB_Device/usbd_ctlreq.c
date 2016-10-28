// usbd_ctlreq.c
/*{{{  includes*/
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"
/*}}}*/

/*{{{*/
static void USBD_GetDescriptor (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {

  uint16_t len;
  uint8_t *pbuf;

  switch (req->wValue >> 8) {
    #if (USBD_LPM_ENABLED == 1)
    /*{{{*/
    case USB_DESC_TYPE_BOS:
      pbuf = pdev->pDesc->GetBOSDescriptor(pdev->dev_speed, &len);
      break;
    /*}}}*/
    #endif
    /*{{{*/
    case USB_DESC_TYPE_DEVICE:
      pbuf = pdev->pDesc->GetDeviceDescriptor(pdev->dev_speed, &len);
      break;
    /*}}}*/
    /*{{{*/
    case USB_DESC_TYPE_CONFIGURATION:
      if (pdev->dev_speed == USBD_SPEED_HIGH ) {
        pbuf   = (uint8_t *)pdev->pClass->GetHSConfigDescriptor(&len);
        pbuf[1] = USB_DESC_TYPE_CONFIGURATION;
        }
      else {
        pbuf   = (uint8_t *)pdev->pClass->GetFSConfigDescriptor(&len);
        pbuf[1] = USB_DESC_TYPE_CONFIGURATION;
        }
      break;
    /*}}}*/
    /*{{{*/
    case USB_DESC_TYPE_STRING:
      switch ((uint8_t)(req->wValue)) {
        case USBD_IDX_LANGID_STR:
         pbuf = pdev->pDesc->GetLangIDStrDescriptor(pdev->dev_speed, &len);
          break;

        case USBD_IDX_MFC_STR:
          pbuf = pdev->pDesc->GetManufacturerStrDescriptor(pdev->dev_speed, &len);
          break;

        case USBD_IDX_PRODUCT_STR:
          pbuf = pdev->pDesc->GetProductStrDescriptor(pdev->dev_speed, &len);
          break;

        case USBD_IDX_SERIAL_STR:
          pbuf = pdev->pDesc->GetSerialStrDescriptor(pdev->dev_speed, &len);
          break;

        case USBD_IDX_CONFIG_STR:
          pbuf = pdev->pDesc->GetConfigurationStrDescriptor(pdev->dev_speed, &len);
          break;

        case USBD_IDX_INTERFACE_STR:
          pbuf = pdev->pDesc->GetInterfaceStrDescriptor(pdev->dev_speed, &len);
          break;

        default:
          #if (USBD_SUPPORT_USER_STRING == 1)
            pbuf = pdev->pClass->GetUsrStrDescriptor(pdev, (req->wValue) , &len);
            break;
          #else
            USBD_CtlError(pdev , req);
            return;
          #endif
        }

      break;
    /*}}}*/
    /*{{{*/
    case USB_DESC_TYPE_DEVICE_QUALIFIER:

      if (pdev->dev_speed == USBD_SPEED_HIGH  ) {
        pbuf = (uint8_t *)pdev->pClass->GetDeviceQualifierDescriptor(&len);
        break;
        }

      else {
        USBD_CtlError(pdev , req);
        return;
      }
    /*}}}*/
    /*{{{*/
    case USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION:
      if (pdev->dev_speed == USBD_SPEED_HIGH  ) {
        pbuf   = (uint8_t *)pdev->pClass->GetOtherSpeedConfigDescriptor(&len);
        pbuf[1] = USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION;
        break;
        }
      else {
        USBD_CtlError(pdev , req);
        return;
        }
    /*}}}*/
    /*{{{*/
    default:
      USBD_CtlError(pdev , req);
      return;
    /*}}}*/
    }

  if ((len != 0)&& (req->wLength != 0)) {
    len = MIN(len , req->wLength);
    USBD_CtlSendData (pdev, pbuf, len);
    }
  }
/*}}}*/
/*{{{*/
static void USBD_SetAddress (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {

  uint8_t  dev_addr;
  if ((req->wIndex == 0) && (req->wLength == 0)) {
    dev_addr = (uint8_t)(req->wValue) & 0x7F;

    if (pdev->dev_state == USBD_STATE_CONFIGURED)
      USBD_CtlError(pdev , req);
    else {
      pdev->dev_address = dev_addr;
      USBD_LL_SetUSBAddress(pdev, dev_addr);
      USBD_CtlSendStatus(pdev);

      if (dev_addr != 0)
        pdev->dev_state  = USBD_STATE_ADDRESSED;
      else
        pdev->dev_state  = USBD_STATE_DEFAULT;
      }
    }
  else
     USBD_CtlError(pdev , req);
  }
/*}}}*/
/*{{{*/
static void USBD_SetConfig (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {

  static uint8_t  cfgidx;
  cfgidx = (uint8_t)(req->wValue);
  if (cfgidx > USBD_MAX_NUM_CONFIGURATION )
     USBD_CtlError (pdev , req);

  else {
    switch (pdev->dev_state) {
      case USBD_STATE_ADDRESSED:
        if (cfgidx) {
          pdev->dev_config = cfgidx;
          pdev->dev_state = USBD_STATE_CONFIGURED;
          if (USBD_SetClassConfig (pdev , cfgidx) == USBD_FAIL) {
            USBD_CtlError (pdev,  req);
            return;
            }
          USBD_CtlSendStatus (pdev);
          }
        else
           USBD_CtlSendStatus (pdev);
        break;

      case USBD_STATE_CONFIGURED:
        if (cfgidx == 0) {
          pdev->dev_state = USBD_STATE_ADDRESSED;
          pdev->dev_config = cfgidx;
          USBD_ClrClassConfig (pdev , cfgidx);
          USBD_CtlSendStatus (pdev);
          }
        else  if (cfgidx != pdev->dev_config) {
          /* Clear old configuration */
          USBD_ClrClassConfig (pdev , pdev->dev_config);

          /* set new configuration */
          pdev->dev_config = cfgidx;
          if (USBD_SetClassConfig (pdev , cfgidx) == USBD_FAIL) {
            USBD_CtlError (pdev , req);
            return;
            }
          USBD_CtlSendStatus (pdev);
          }
        else
          USBD_CtlSendStatus (pdev);
        break;

      default:
         USBD_CtlError (pdev , req);
        break;
      }
    }
  }
/*}}}*/
/*{{{*/
static void USBD_GetConfig (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {

  if (req->wLength != 1)
     USBD_CtlError (pdev , req);

  else {
    switch (pdev->dev_state ) {
      case USBD_STATE_ADDRESSED:
        pdev->dev_default_config = 0;
        USBD_CtlSendData (pdev, (uint8_t*)&pdev->dev_default_config, 1);
        break;

      case USBD_STATE_CONFIGURED:
        USBD_CtlSendData (pdev, (uint8_t*)&pdev->dev_config, 1);
        break;

      default:
         USBD_CtlError (pdev , req);
        break;
      }
    }
  }
/*}}}*/
/*{{{*/
static void USBD_GetStatus (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {

  switch (pdev->dev_state) {
    case USBD_STATE_ADDRESSED:
    case USBD_STATE_CONFIGURED:
      #if (USBD_SELF_POWERED == 1)
        pdev->dev_config_status = USB_CONFIG_SELF_POWERED;
      #else
        pdev->dev_config_status = 0;
      #endif

      if (pdev->dev_remote_wakeup)
         pdev->dev_config_status |= USB_CONFIG_REMOTE_WAKEUP;

      USBD_CtlSendData (pdev, (uint8_t *)& pdev->dev_config_status, 2);
      break;

    default :
      USBD_CtlError (pdev , req);
      break;
    }
  }

/*}}}*/
/*{{{*/
static void USBD_SetFeature (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {

  if (req->wValue == USB_FEATURE_REMOTE_WAKEUP) {
    pdev->dev_remote_wakeup = 1;
    pdev->pClass->Setup (pdev, req);
    USBD_CtlSendStatus(pdev);
    }
  }
/*}}}*/
/*{{{*/
static void USBD_ClrFeature (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {

  switch (pdev->dev_state) {
    case USBD_STATE_ADDRESSED:
    case USBD_STATE_CONFIGURED:
      if (req->wValue == USB_FEATURE_REMOTE_WAKEUP) {
        pdev->dev_remote_wakeup = 0;
        pdev->pClass->Setup (pdev, req);
        USBD_CtlSendStatus (pdev);
        }
      break;

    default :
       USBD_CtlError (pdev , req);
      break;
    }
  }
/*}}}*/
/*{{{*/
static uint8_t USBD_GetLen (uint8_t *buf) {

  uint8_t  len = 0;
  while (*buf != '\0') {
    len++;
    buf++;
    }

  return len;
  }
/*}}}*/

/*{{{*/
USBD_StatusTypeDef USBD_StdDevReq (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef  *req) {
  USBD_StatusTypeDef ret = USBD_OK;

  switch (req->bRequest) {
    case USB_REQ_GET_DESCRIPTOR:
      USBD_GetDescriptor (pdev, req) ;
      break;

    case USB_REQ_SET_ADDRESS:
      USBD_SetAddress(pdev, req);
      break;

    case USB_REQ_SET_CONFIGURATION:
      USBD_SetConfig (pdev , req);
      break;

    case USB_REQ_GET_CONFIGURATION:
      USBD_GetConfig (pdev , req);
      break;

    case USB_REQ_GET_STATUS:
      USBD_GetStatus (pdev , req);
      break;

    case USB_REQ_SET_FEATURE:
      USBD_SetFeature (pdev , req);
      break;

    case USB_REQ_CLEAR_FEATURE:
      USBD_ClrFeature (pdev , req);
      break;

    default:
      USBD_CtlError(pdev , req);
      break;
    }

  return ret;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_StdItfReq (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef  *req) {

  USBD_StatusTypeDef ret = USBD_OK;

  switch (pdev->dev_state) {
    case USBD_STATE_CONFIGURED:

      if (LOBYTE(req->wIndex) <= USBD_MAX_NUM_INTERFACES) {
        pdev->pClass->Setup (pdev, req);
        if ((req->wLength == 0)&& (ret == USBD_OK))
           USBD_CtlSendStatus(pdev);
        }
      else
        USBD_CtlError (pdev , req);
      break;

    default:
       USBD_CtlError (pdev , req);
      break;
    }

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_StdEPReq (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef  *req) {

  uint8_t   ep_addr;
  USBD_StatusTypeDef ret = USBD_OK;
  USBD_EndpointTypeDef   *pep;
  ep_addr  = LOBYTE(req->wIndex);

  /* Check if it is a class request */
  if ((req->bmRequest & 0x60) == 0x20) {
    pdev->pClass->Setup (pdev, req);
    return USBD_OK;
    }

  switch (req->bRequest) {
    case USB_REQ_SET_FEATURE :
      switch (pdev->dev_state) {
        /*{{{*/
        case USBD_STATE_ADDRESSED:
          if ((ep_addr != 0x00) && (ep_addr != 0x80))
            USBD_LL_StallEP(pdev , ep_addr);
          break;
        /*}}}*/
        /*{{{*/
        case USBD_STATE_CONFIGURED:
          if (req->wValue == USB_FEATURE_EP_HALT) {
            if ((ep_addr != 0x00) && (ep_addr != 0x80))
              USBD_LL_StallEP(pdev , ep_addr);
          }
          pdev->pClass->Setup (pdev, req);
          USBD_CtlSendStatus(pdev);

          break;
        /*}}}*/
        /*{{{*/
        default:
          USBD_CtlError(pdev , req);
          break;
        /*}}}*/
        }
      break;

    case USB_REQ_CLEAR_FEATURE :
      switch (pdev->dev_state) {
        /*{{{*/
        case USBD_STATE_ADDRESSED:
          if ((ep_addr != 0x00) && (ep_addr != 0x80))
            USBD_LL_StallEP(pdev , ep_addr);
          break;
        /*}}}*/
        /*{{{*/
        case USBD_STATE_CONFIGURED:
          if (req->wValue == USB_FEATURE_EP_HALT) {
            if ((ep_addr & 0x7F) != 0x00) {
              USBD_LL_ClearStallEP(pdev , ep_addr);
              pdev->pClass->Setup (pdev, req);
            }
            USBD_CtlSendStatus(pdev);
          }
          break;
        /*}}}*/
        /*{{{*/
        default:
          USBD_CtlError(pdev , req);
          break;
        }
        /*}}}*/
      break;

    case USB_REQ_GET_STATUS:
      switch (pdev->dev_state) {
        /*{{{*/
        case USBD_STATE_ADDRESSED:
          if ((ep_addr & 0x7F) != 0x00)
            USBD_LL_StallEP(pdev , ep_addr);
          break;
        /*}}}*/
        /*{{{*/
        case USBD_STATE_CONFIGURED:
          pep = ((ep_addr & 0x80) == 0x80) ? &pdev->ep_in[ep_addr & 0x7F] : &pdev->ep_out[ep_addr & 0x7F];
          if (USBD_LL_IsStallEP(pdev, ep_addr))
            pep->status = 0x0001;
          else
            pep->status = 0x0000;

          USBD_CtlSendData (pdev, (uint8_t *)&pep->status, 2);
          break;
        /*}}}*/
        /*{{{*/
        default:
          USBD_CtlError(pdev , req);
          break;
        /*}}}*/
        }
      break;

    default:
      break;
    }

  return ret;
  }
/*}}}*/

/*{{{*/
void USBD_ParseSetupRequest (USBD_SetupReqTypedef *req, uint8_t *pdata) {

  req->bmRequest = *(uint8_t*)(pdata);
  req->bRequest  = *(uint8_t*)(pdata +  1);
  req->wValue    = SWAPBYTE (pdata +  2);
  req->wIndex    = SWAPBYTE (pdata +  4);
  req->wLength   = SWAPBYTE (pdata +  6);
  }
/*}}}*/
/*{{{*/
void USBD_CtlError (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef *req) {
  USBD_LL_StallEP (pdev , 0x80);
  USBD_LL_StallEP (pdev , 0);
  }
/*}}}*/
/*{{{*/
void USBD_GetString (uint8_t *desc, uint8_t *unicode, uint16_t *len) {

  uint8_t idx = 0;
  if (desc != NULL) {
    *len =  USBD_GetLen (desc) * 2 + 2;
    unicode[idx++] = *len;
    unicode[idx++] =  USB_DESC_TYPE_STRING;

    while (*desc != '\0') {
      unicode[idx++] = *desc++;
      unicode[idx++] =  0x00;
      }
    }
  }
/*}}}*/
