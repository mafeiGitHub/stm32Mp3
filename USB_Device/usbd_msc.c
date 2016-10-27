#include "usbd_msc.h"

uint8_t  USBD_MSC_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
uint8_t  USBD_MSC_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
uint8_t  USBD_MSC_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
uint8_t  USBD_MSC_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);
uint8_t  USBD_MSC_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);
uint8_t  *USBD_MSC_GetHSCfgDesc (uint16_t *length);
uint8_t  *USBD_MSC_GetFSCfgDesc (uint16_t *length);
uint8_t  *USBD_MSC_GetOtherSpeedCfgDesc (uint16_t *length);
uint8_t  *USBD_MSC_GetDeviceQualifierDescriptor (uint16_t *length);

/*{{{*/
USBD_ClassTypeDef USBD_MSC =
{
  USBD_MSC_Init,
  USBD_MSC_DeInit,
  USBD_MSC_Setup,
  NULL, /*EP0_TxSent*/
  NULL, /*EP0_RxReady*/
  USBD_MSC_DataIn,
  USBD_MSC_DataOut,
  NULL, /*SOF */
  NULL,
  NULL,
  USBD_MSC_GetHSCfgDesc,
  USBD_MSC_GetFSCfgDesc,
  USBD_MSC_GetOtherSpeedCfgDesc,
  USBD_MSC_GetDeviceQualifierDescriptor,
};
/*}}}*/

/*{{{*/
/* USB Mass storage device Configuration Descriptor */
/*   All Descriptors (Configuration, Interface, Endpoint, Class, Vendor */
__ALIGN_BEGIN uint8_t USBD_MSC_CfgHSDesc[USB_MSC_CONFIG_DESC_SIZ]  __ALIGN_END =
{

  0x09,   /* bLength: Configuation Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,   /* bDescriptorType: Configuration */
  USB_MSC_CONFIG_DESC_SIZ,

  0x00,
  0x01,   /* bNumInterfaces: 1 interface */
  0x01,   /* bConfigurationValue: */
  0x04,   /* iConfiguration: */
  0xC0,   /* bmAttributes: */
  0x32,   /* MaxPower 100 mA */

  /********************  Mass Storage interface ********************/
  0x09,   /* bLength: Interface Descriptor size */
  0x04,   /* bDescriptorType: */
  0x00,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x02,   /* bNumEndpoints*/
  0x08,   /* bInterfaceClass: MSC Class */
  0x06,   /* bInterfaceSubClass : SCSI transparent*/
  0x50,   /* nInterfaceProtocol */
  0x05,          /* iInterface: */
  /********************  Mass Storage Endpoints ********************/
  0x07,   /*Endpoint descriptor length = 7*/
  0x05,   /*Endpoint descriptor type */
  MSC_EPIN_ADDR,   /*Endpoint address (IN, address 1) */
  0x02,   /*Bulk endpoint type */
  LOBYTE(MSC_MAX_HS_PACKET),
  HIBYTE(MSC_MAX_HS_PACKET),
  0x00,   /*Polling interval in milliseconds */

  0x07,   /*Endpoint descriptor length = 7 */
  0x05,   /*Endpoint descriptor type */
  MSC_EPOUT_ADDR,   /*Endpoint address (OUT, address 1) */
  0x02,   /*Bulk endpoint type */
  LOBYTE(MSC_MAX_HS_PACKET),
  HIBYTE(MSC_MAX_HS_PACKET),
  0x00     /*Polling interval in milliseconds*/
};
/*}}}*/
/*{{{*/
/* USB Mass storage device Configuration Descriptor */
/*   All Descriptors (Configuration, Interface, Endpoint, Class, Vendor */
uint8_t USBD_MSC_CfgFSDesc[USB_MSC_CONFIG_DESC_SIZ]  __ALIGN_END =
{

  0x09,   /* bLength: Configuation Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,   /* bDescriptorType: Configuration */
  USB_MSC_CONFIG_DESC_SIZ,

  0x00,
  0x01,   /* bNumInterfaces: 1 interface */
  0x01,   /* bConfigurationValue: */
  0x04,   /* iConfiguration: */
  0xC0,   /* bmAttributes: */
  0x32,   /* MaxPower 100 mA */

  /********************  Mass Storage interface ********************/
  0x09,   /* bLength: Interface Descriptor size */
  0x04,   /* bDescriptorType: */
  0x00,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x02,   /* bNumEndpoints*/
  0x08,   /* bInterfaceClass: MSC Class */
  0x06,   /* bInterfaceSubClass : SCSI transparent*/
  0x50,   /* nInterfaceProtocol */
  0x05,          /* iInterface: */
  /********************  Mass Storage Endpoints ********************/
  0x07,   /*Endpoint descriptor length = 7*/
  0x05,   /*Endpoint descriptor type */
  MSC_EPIN_ADDR,   /*Endpoint address (IN, address 1) */
  0x02,   /*Bulk endpoint type */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00,   /*Polling interval in milliseconds */

  0x07,   /*Endpoint descriptor length = 7 */
  0x05,   /*Endpoint descriptor type */
  MSC_EPOUT_ADDR,   /*Endpoint address (OUT, address 1) */
  0x02,   /*Bulk endpoint type */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00     /*Polling interval in milliseconds*/
};
/*}}}*/
/*{{{*/
__ALIGN_BEGIN uint8_t USBD_MSC_OtherSpeedCfgDesc[USB_MSC_CONFIG_DESC_SIZ]   __ALIGN_END  =
{

  0x09,   /* bLength: Configuation Descriptor size */
  USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION,
  USB_MSC_CONFIG_DESC_SIZ,

  0x00,
  0x01,   /* bNumInterfaces: 1 interface */
  0x01,   /* bConfigurationValue: */
  0x04,   /* iConfiguration: */
  0xC0,   /* bmAttributes: */
  0x32,   /* MaxPower 100 mA */

  /********************  Mass Storage interface ********************/
  0x09,   /* bLength: Interface Descriptor size */
  0x04,   /* bDescriptorType: */
  0x00,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x02,   /* bNumEndpoints*/
  0x08,   /* bInterfaceClass: MSC Class */
  0x06,   /* bInterfaceSubClass : SCSI transparent command set*/
  0x50,   /* nInterfaceProtocol */
  0x05,          /* iInterface: */
  /********************  Mass Storage Endpoints ********************/
  0x07,   /*Endpoint descriptor length = 7*/
  0x05,   /*Endpoint descriptor type */
  MSC_EPIN_ADDR,   /*Endpoint address (IN, address 1) */
  0x02,   /*Bulk endpoint type */
  0x40,
  0x00,
  0x00,   /*Polling interval in milliseconds */

  0x07,   /*Endpoint descriptor length = 7 */
  0x05,   /*Endpoint descriptor type */
  MSC_EPOUT_ADDR,   /*Endpoint address (OUT, address 1) */
  0x02,   /*Bulk endpoint type */
  0x40,
  0x00,
  0x00     /*Polling interval in milliseconds*/
};
/*}}}*/
/*{{{*/
/* USB Standard Device Descriptor */
__ALIGN_BEGIN  uint8_t USBD_MSC_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC]  __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  MSC_MAX_FS_PACKET,
  0x01,
  0x00,
};
/*}}}*/

/*{{{*/
/**
  * @brief  USBD_MSC_Init
  *         Initialize  the mass storage configuration
  * @param  pdev: device instance
  * @param  cfgidx: configuration index
  * @retval status
  */
uint8_t  USBD_MSC_Init (USBD_HandleTypeDef *pdev,
                            uint8_t cfgidx)
{
  int16_t ret = 0;

  if(pdev->dev_speed == USBD_SPEED_HIGH  )
  {
    /* Open EP OUT */
    USBD_LL_OpenEP(pdev,
                   MSC_EPOUT_ADDR,
                   USBD_EP_TYPE_BULK,
                   MSC_MAX_HS_PACKET);

    /* Open EP IN */
    USBD_LL_OpenEP(pdev,
                   MSC_EPIN_ADDR,
                   USBD_EP_TYPE_BULK,
                   MSC_MAX_HS_PACKET);
  }
  else
  {
    /* Open EP OUT */
    USBD_LL_OpenEP(pdev,
                   MSC_EPOUT_ADDR,
                   USBD_EP_TYPE_BULK,
                   MSC_MAX_FS_PACKET);

    /* Open EP IN */
    USBD_LL_OpenEP(pdev,
                   MSC_EPIN_ADDR,
                   USBD_EP_TYPE_BULK,
                   MSC_MAX_FS_PACKET);
  }
  pdev->pClassData = USBD_malloc(sizeof (USBD_MSC_BOT_HandleTypeDef));

  if(pdev->pClassData == NULL)
  {
    ret = 1;
  }
  else
  {
    /* Init the BOT  layer */
    MSC_BOT_Init(pdev);
    ret = 0;
  }

  return ret;
}
/*}}}*/
/*{{{*/
/**
  * @brief  USBD_MSC_DeInit
  *         DeInitilaize  the mass storage configuration
  * @param  pdev: device instance
  * @param  cfgidx: configuration index
  * @retval status
  */
uint8_t  USBD_MSC_DeInit (USBD_HandleTypeDef *pdev,
                              uint8_t cfgidx)
{
  /* Close MSC EPs */
  USBD_LL_CloseEP(pdev,
                  MSC_EPOUT_ADDR);

  /* Open EP IN */
  USBD_LL_CloseEP(pdev,
                  MSC_EPIN_ADDR);


    /* De-Init the BOT layer */
  MSC_BOT_DeInit(pdev);

  /* Free MSC Class Resources */
  if(pdev->pClassData != NULL)
  {
    USBD_free(pdev->pClassData);
    pdev->pClassData  = NULL;
  }
  return 0;
}
/*}}}*/
/*{{{*/
/**
* @brief  USBD_MSC_Setup
*         Handle the MSC specific requests
* @param  pdev: device instance
* @param  req: USB request
* @retval status
*/
uint8_t  USBD_MSC_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_MSC_BOT_HandleTypeDef     *hmsc = (USBD_MSC_BOT_HandleTypeDef*) pdev->pClassData;

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {

  /* Class request */
  case USB_REQ_TYPE_CLASS :
    switch (req->bRequest)
    {
    case BOT_GET_MAX_LUN :

      if((req->wValue  == 0) &&
         (req->wLength == 1) &&
         ((req->bmRequest & 0x80) == 0x80))
      {
        hmsc->max_lun = ((USBD_StorageTypeDef *)pdev->pUserData)->GetMaxLun();
        USBD_CtlSendData (pdev,
                          (uint8_t *)&hmsc->max_lun,
                          1);
      }
      else
      {
         USBD_CtlError(pdev , req);
         return USBD_FAIL;
      }
      break;

    case BOT_RESET :
      if((req->wValue  == 0) &&
         (req->wLength == 0) &&
        ((req->bmRequest & 0x80) != 0x80))
      {
         MSC_BOT_Reset(pdev);
      }
      else
      {
         USBD_CtlError(pdev , req);
         return USBD_FAIL;
      }
      break;

    default:
       USBD_CtlError(pdev , req);
       return USBD_FAIL;
    }
    break;
  /* Interface & Endpoint request */
  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {
    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        (uint8_t *)&hmsc->interface,
                        1);
      break;

    case USB_REQ_SET_INTERFACE :
      hmsc->interface = (uint8_t)(req->wValue);
      break;

    case USB_REQ_CLEAR_FEATURE:

      /* Flush the FIFO and Clear the stall status */
      USBD_LL_FlushEP(pdev, (uint8_t)req->wIndex);

      /* Reactivate the EP */
      USBD_LL_CloseEP (pdev , (uint8_t)req->wIndex);
      if((((uint8_t)req->wIndex) & 0x80) == 0x80)
      {
        if(pdev->dev_speed == USBD_SPEED_HIGH  )
        {
          /* Open EP IN */
          USBD_LL_OpenEP(pdev,
                         MSC_EPIN_ADDR,
                         USBD_EP_TYPE_BULK,
                         MSC_MAX_HS_PACKET);
        }
        else
        {
          /* Open EP IN */
          USBD_LL_OpenEP(pdev,
                         MSC_EPIN_ADDR,
                         USBD_EP_TYPE_BULK,
                         MSC_MAX_FS_PACKET);
        }
      }
      else
      {
        if(pdev->dev_speed == USBD_SPEED_HIGH  )
        {
          /* Open EP IN */
          USBD_LL_OpenEP(pdev,
                         MSC_EPOUT_ADDR,
                         USBD_EP_TYPE_BULK,
                         MSC_MAX_HS_PACKET);
        }
        else
        {
          /* Open EP IN */
          USBD_LL_OpenEP(pdev,
                         MSC_EPOUT_ADDR,
                         USBD_EP_TYPE_BULK,
                         MSC_MAX_FS_PACKET);
        }
      }

      /* Handle BOT error */
      MSC_BOT_CplClrFeature(pdev, (uint8_t)req->wIndex);
      break;

    }
    break;

  default:
    break;
  }
  return 0;
}
/*}}}*/

/*{{{*/
/*
* @brief  USBD_MSC_DataIn
*         handle data IN Stage
* @param  pdev: device instance
* @param  epnum: endpoint index
* @retval status
*/
uint8_t  USBD_MSC_DataIn (USBD_HandleTypeDef *pdev,
                              uint8_t epnum)
{
  MSC_BOT_DataIn(pdev , epnum);
  return 0;
}
/*}}}*/
/*{{{*/
/**
* @brief  USBD_MSC_DataOut
*         handle data OUT Stage
* @param  pdev: device instance
* @param  epnum: endpoint index
* @retval status
*/
uint8_t  USBD_MSC_DataOut (USBD_HandleTypeDef *pdev,
                               uint8_t epnum)
{
  MSC_BOT_DataOut(pdev , epnum);
  return 0;
}
/*}}}*/

/*{{{*/
/**
* @brief  USBD_MSC_GetHSCfgDesc
*         return configuration descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
uint8_t  *USBD_MSC_GetHSCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_MSC_CfgHSDesc);
  return USBD_MSC_CfgHSDesc;
}
/*}}}*/
/*{{{*/
/**
* @brief  USBD_MSC_GetFSCfgDesc
*         return configuration descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
uint8_t  *USBD_MSC_GetFSCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_MSC_CfgFSDesc);
  return USBD_MSC_CfgFSDesc;
}
/*}}}*/
/*{{{*/
/**
* @brief  USBD_MSC_GetOtherSpeedCfgDesc
*         return other speed configuration descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
uint8_t  *USBD_MSC_GetOtherSpeedCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_MSC_OtherSpeedCfgDesc);
  return USBD_MSC_OtherSpeedCfgDesc;
}
/*}}}*/
/*{{{*/
/**
* @brief  DeviceQualifierDescriptor
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
uint8_t  *USBD_MSC_GetDeviceQualifierDescriptor (uint16_t *length)
{
  *length = sizeof (USBD_MSC_DeviceQualifierDesc);
  return USBD_MSC_DeviceQualifierDesc;
}
/*}}}*/

/*{{{*/
/**
* @brief  USBD_MSC_RegisterStorage
* @param  fops: storage callback
* @retval status
*/
uint8_t  USBD_MSC_RegisterStorage  (USBD_HandleTypeDef   *pdev,
                                    USBD_StorageTypeDef *fops)
{
  if(fops != NULL)
  {
    pdev->pUserData= fops;
  }
  return 0;
}
/*}}}*/