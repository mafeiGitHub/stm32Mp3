// usbd_msc.c
/*{{{  includes*/
#include "usbd_msc.h"
#include "usbd_core.h"

#include "memory.h"
/*}}}*/
/*{{{  scsi const*/
/*{{{  scsi Commands defines*/
#define SCSI_FORMAT_UNIT                            0x04
#define SCSI_INQUIRY                                0x12
#define SCSI_MODE_SELECT6                           0x15
#define SCSI_MODE_SELECT10                          0x55
#define SCSI_MODE_SENSE6                            0x1A
#define SCSI_MODE_SENSE10                           0x5A
#define SCSI_ALLOW_MEDIUM_REMOVAL                   0x1E
#define SCSI_READ6                                  0x08
#define SCSI_READ10                                 0x28
#define SCSI_READ12                                 0xA8
#define SCSI_READ16                                 0x88

#define SCSI_READ_CAPACITY10                        0x25
#define SCSI_READ_CAPACITY16                        0x9E

#define SCSI_REQUEST_SENSE                          0x03
#define SCSI_START_STOP_UNIT                        0x1B
#define SCSI_TEST_UNIT_READY                        0x00
#define SCSI_WRITE6                                 0x0A
#define SCSI_WRITE10                                0x2A
#define SCSI_WRITE12                                0xAA
#define SCSI_WRITE16                                0x8A

#define SCSI_VERIFY10                               0x2F
#define SCSI_VERIFY12                               0xAF
#define SCSI_VERIFY16                               0x8F

#define SCSI_SEND_DIAGNOSTIC                        0x1D
#define SCSI_READ_FORMAT_CAPACITIES                 0x23
/*}}}*/
/*{{{  scsi errors? defines*/
#define NO_SENSE                                    0
#define RECOVERED_ERROR                             1
#define NOT_READY                                   2
#define MEDIUM_ERROR                                3
#define HARDWARE_ERROR                              4
#define ILLEGAL_REQUEST                             5
#define UNIT_ATTENTION                              6
#define DATA_PROTECT                                7
#define BLANK_CHECK                                 8
#define VENDOR_SPECIFIC                             9
#define COPY_ABORTED                                10
#define ABORTED_COMMAND                             11
#define VOLUME_OVERFLOW                             13
#define MISCOMPARE                                  14
/*}}}*/

#define INVALID_CDB                                 0x20
#define INVALID_FIELED_IN_COMMAND                   0x24
#define PARAMETER_LIST_LENGTH_ERROR                 0x1A
#define INVALID_FIELD_IN_PARAMETER_LIST             0x26
#define ADDRESS_OUT_OF_RANGE                        0x21
#define MEDIUM_NOT_PRESENT                          0x3A
#define MEDIUM_HAVE_CHANGED                         0x28
#define WRITE_PROTECTED                             0x27
#define UNRECOVERED_READ_ERROR                      0x11
#define WRITE_FAULT                                 0x03

#define READ_FORMAT_CAPACITY_DATA_LEN               0x0C
#define READ_CAPACITY10_DATA_LEN                    0x08
#define MODE_SENSE10_DATA_LEN                       0x08
#define MODE_SENSE6_DATA_LEN                        0x04
#define REQUEST_SENSE_DATA_LEN                      0x12
#define STANDARD_INQUIRY_DATA_LEN                   0x24
#define BLKVFY                                      0x04

#define MODE_SENSE6_LEN  8
#define MODE_SENSE10_LEN 8
#define LENGTH_INQUIRY_PAGE00  7
#define LENGTH_FORMAT_CAPACITIES  20

static const uint8_t MSC_Mode_Sense6_data[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t MSC_Mode_Sense10_data[] = { 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t MSC_Page00_Inquiry_Data[] = { 0x00, 0x00, 0x00, (LENGTH_INQUIRY_PAGE00 - 4), 0x00, 0x80, 0x83 };

// struct USBD_SCSI_SenseTypeDef
typedef struct _SENSE_ITEM {
  char Skey;
  union {
    struct _ASCs {
      char ASC;
      char ASCQ;
      } b;
    unsigned int  ASC;
    char *pData;
    } w;
  } USBD_SCSI_SenseTypeDef;
/*}}}*/
/*{{{  bot defines*/
#define USBD_BOT_IDLE                      0       /* Idle state */
#define USBD_BOT_DATA_OUT                  1       /* Data Out state */
#define USBD_BOT_DATA_IN                   2       /* Data In state */
#define USBD_BOT_LAST_DATA_IN              3       /* Last Data In Last */
#define USBD_BOT_SEND_DATA                 4       /* Send Immediate data */
#define USBD_BOT_NO_DATA                   5       /* No data Stage */

#define USBD_BOT_CBW_SIGNATURE             0x43425355
#define USBD_BOT_CSW_SIGNATURE             0x53425355
#define USBD_BOT_CBW_LENGTH                31
#define USBD_BOT_CSW_LENGTH                13
#define USBD_BOT_MAX_DATA                  256

/* CSW Status Definitions */
#define USBD_CSW_CMD_PASSED                0x00
#define USBD_CSW_CMD_FAILED                0x01
#define USBD_CSW_PHASE_ERROR               0x02

/* BOT Status */
#define USBD_BOT_STATUS_NORMAL             0
#define USBD_BOT_STATUS_RECOVERY           1
#define USBD_BOT_STATUS_ERROR              2

#define USBD_DIR_IN                        0
#define USBD_DIR_OUT                       1
#define USBD_BOTH_DIR                      2

#define BOT_GET_MAX_LUN          0xFE
#define BOT_RESET                0xFF

#define USB_MSC_CONFIG_DESC_SIZ  32
#define SENSE_LIST_DEEPTH  4

/*{{{  struct USBD_MSC_BOT_CBWTypeDef*/
typedef struct {
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataLength;
  uint8_t  bmFlags;
  uint8_t  bLUN;
  uint8_t  bCBLength;
  uint8_t  CB[16];
  uint8_t  ReservedForAlign;
  } USBD_MSC_BOT_CBWTypeDef;
/*}}}*/
/*{{{  struct USBD_MSC_BOT_CSWTypeDef*/
typedef struct {
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataResidue;
  uint8_t  bStatus;
  uint8_t  ReservedForAlign[3];
  } USBD_MSC_BOT_CSWTypeDef;
/*}}}*/
/*{{{  struct USBD_MSC_BOT_HandleTypeDef*/
typedef struct {
  uint32_t                 max_lun;
  uint32_t                 interface;
  uint8_t                  bot_state;
  uint8_t                  bot_status;
  uint16_t                 bot_data_length;

  uint8_t                  bot_data [MSC_MEDIA_PACKET];
  USBD_MSC_BOT_CBWTypeDef  cbw;
  USBD_MSC_BOT_CSWTypeDef  csw;

  USBD_SCSI_SenseTypeDef   scsi_sense [SENSE_LIST_DEEPTH];
  uint8_t                  scsi_sense_head;
  uint8_t                  scsi_sense_tail;

  uint16_t                 scsi_blk_size;
  uint32_t                 scsi_blk_nbr;

  uint32_t                 scsi_blk_addr;
  uint32_t                 scsi_blk_len;
  } USBD_MSC_BOT_HandleTypeDef;
/*}}}*/
/*}}}*/
/*{{{  msc defines*/
#define MSC_MAX_FS_PACKET  0x40
#define MSC_MAX_HS_PACKET  0x200
#define MSC_EPIN_ADDR      0x81
#define MSC_EPOUT_ADDR     0x01
/*}}}*/
/*{{{  msc desc static const*/
/*{{{*/
__ALIGN_BEGIN static const uint8_t USBD_MSC_CfgHSDesc[USB_MSC_CONFIG_DESC_SIZ] __ALIGN_END = {
  0x09,   /* bLength: Configuation Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,   /* bDescriptorType: Configuration */
  USB_MSC_CONFIG_DESC_SIZ,

  0x00,
  0x01, /* bNumInterfaces: 1 interface */
  0x01, /* bConfigurationValue: */
  0x04, /* iConfiguration: */
  0xC0, /* bmAttributes: */
  0x32, /* MaxPower 100 mA */

  /********************  Mass Storage interface ********************/
  0x09, /* bLength: Interface Descriptor size */
  0x04, /* bDescriptorType: */
  0x00, /* bInterfaceNumber: Number of Interface */
  0x00, /* bAlternateSetting: Alternate setting */
  0x02, /* bNumEndpoints*/
  0x08, /* bInterfaceClass: MSC Class */
  0x06, /* bInterfaceSubClass : SCSI transparent*/
  0x50, /* nInterfaceProtocol */
  0x05, /* iInterface: */

  /********************  Mass Storage Endpoints ********************/
  0x07, /*Endpoint descriptor length = 7*/
  0x05, /*Endpoint descriptor type */
  MSC_EPIN_ADDR,   /*Endpoint address (IN, address 1) */
  0x02, /*Bulk endpoint type */
  LOBYTE(MSC_MAX_HS_PACKET),
  HIBYTE(MSC_MAX_HS_PACKET),
  0x00, /*Polling interval in milliseconds */

  0x07, /*Endpoint descriptor length = 7 */
  0x05, /*Endpoint descriptor type */
  MSC_EPOUT_ADDR,   /*Endpoint address (OUT, address 1) */
  0x02, /*Bulk endpoint type */
  LOBYTE(MSC_MAX_HS_PACKET),
  HIBYTE(MSC_MAX_HS_PACKET),
  0x00  /*Polling interval in milliseconds*/
  };
/*}}}*/
/*{{{*/
__ALIGN_BEGIN static const uint8_t USBD_MSC_CfgFSDesc[USB_MSC_CONFIG_DESC_SIZ] __ALIGN_END = {
  0x09,   /* bLength: Configuation Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,   /* bDescriptorType: Configuration */
  USB_MSC_CONFIG_DESC_SIZ,

  0x00,
  0x01, /* bNumInterfaces: 1 interface */
  0x01, /* bConfigurationValue: */
  0x04, /* iConfiguration: */
  0xC0, /* bmAttributes: */
  0x32, /* MaxPower 100 mA */

  /********************  Mass Storage interface ********************/
  0x09, /* bLength: Interface Descriptor size */
  0x04, /* bDescriptorType: */
  0x00, /* bInterfaceNumber: Number of Interface */
  0x00, /* bAlternateSetting: Alternate setting */
  0x02, /* bNumEndpoints*/
  0x08, /* bInterfaceClass: MSC Class */
  0x06, /* bInterfaceSubClass : SCSI transparent*/
  0x50, /* nInterfaceProtocol */
  0x05, /* iInterface: */

  /********************  Mass Storage Endpoints ********************/
  0x07, /*Endpoint descriptor length = 7*/
  0x05, /*Endpoint descriptor type */
  MSC_EPIN_ADDR, /*Endpoint address (IN, address 1) */
  0x02, /*Bulk endpoint type */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00, /*Polling interval in milliseconds */

  0x07, /*Endpoint descriptor length = 7 */
  0x05, /*Endpoint descriptor type */
  MSC_EPOUT_ADDR,  /*Endpoint address (OUT, address 1) */
  0x02, /*Bulk endpoint type */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00  /*Polling interval in milliseconds*/
  };
/*}}}*/
/*{{{*/
__ALIGN_BEGIN static const uint8_t USBD_MSC_OtherSpeedCfgDesc[USB_MSC_CONFIG_DESC_SIZ] __ALIGN_END  = {
  0x09,   /* bLength: Configuation Descriptor size */
  USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION,
  USB_MSC_CONFIG_DESC_SIZ,

  0x00,
  0x01, /* bNumInterfaces: 1 interface */
  0x01, /* bConfigurationValue: */
  0x04, /* iConfiguration: */
  0xC0, /* bmAttributes: */
  0x32, /* MaxPower 100 mA */

  /********************  Mass Storage interface ********************/
  0x09, /* bLength: Interface Descriptor size */
  0x04, /* bDescriptorType: */
  0x00, /* bInterfaceNumber: Number of Interface */
  0x00, /* bAlternateSetting: Alternate setting */
  0x02, /* bNumEndpoints*/
  0x08, /* bInterfaceClass: MSC Class */
  0x06, /* bInterfaceSubClass : SCSI transparent command set*/
  0x50, /* nInterfaceProtocol */
  0x05, /* iInterface: */

  /********************  Mass Storage Endpoints ********************/
  0x07, /*Endpoint descriptor length = 7*/
  0x05, /*Endpoint descriptor type */
  MSC_EPIN_ADDR, /*Endpoint address (IN, address 1) */
  0x02, /*Bulk endpoint type */
  0x40,
  0x00,
  0x00, /*Polling interval in milliseconds */

  0x07, /*Endpoint descriptor length = 7 */
  0x05, /*Endpoint descriptor type */
  MSC_EPOUT_ADDR, /*Endpoint address (OUT, address 1) */
  0x02, /*Bulk endpoint type */
  0x40,
  0x00,
  0x00  /*Polling interval in milliseconds*/
  };
/*}}}*/
/*{{{*/
/* USB Standard Device Descriptor */
__ALIGN_BEGIN static const uint8_t USBD_MSC_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC]  __ALIGN_END = {
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
/*}}}*/

/*{{{*/
static void BOT_SendCSW (USBD_HandleTypeDef* pdev, uint8_t CSW_Status) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->csw.dSignature = USBD_BOT_CSW_SIGNATURE;
  hmsc->csw.bStatus = CSW_Status;
  hmsc->bot_state = USBD_BOT_IDLE;
  USBD_LL_Transmit (pdev, MSC_EPIN_ADDR, (uint8_t*)&hmsc->csw, USBD_BOT_CSW_LENGTH);

  /* Prepare EP to Receive next Cmd */
  USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t*)&hmsc->cbw, USBD_BOT_CBW_LENGTH);
  }
/*}}}*/

// scsi
/*{{{*/
static void SCSI_SenseCode (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t sKey, uint8_t ASC) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->scsi_sense[hmsc->scsi_sense_tail].Skey = sKey;
  hmsc->scsi_sense[hmsc->scsi_sense_tail].w.ASC = ASC << 8;
  hmsc->scsi_sense_tail++;
  if (hmsc->scsi_sense_tail == SENSE_LIST_DEEPTH)
    hmsc->scsi_sense_tail = 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_CheckAddressRange (USBD_HandleTypeDef* pdev, uint8_t lun , uint32_t blk_offset , uint16_t blk_nbr) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if ((blk_offset + blk_nbr) > hmsc->scsi_blk_nbr) {
    SCSI_SenseCode (pdev, lun, ILLEGAL_REQUEST, ADDRESS_OUT_OF_RANGE);
    return -1;
    }

  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_ProcessRead (USBD_HandleTypeDef* pdev, uint8_t lun) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  uint32_t len = MIN(hmsc->scsi_blk_len, MSC_MEDIA_PACKET);
  if (((USBD_StorageTypeDef*)pdev->pUserData)->Read (lun,
                                                     hmsc->bot_data,
                                                     hmsc->scsi_blk_addr / hmsc->scsi_blk_size,
                                                     len / hmsc->scsi_blk_size) < 0) {
    SCSI_SenseCode (pdev, lun, HARDWARE_ERROR, UNRECOVERED_READ_ERROR);
    return -1;
    }

  USBD_LL_Transmit (pdev, MSC_EPIN_ADDR, hmsc->bot_data, len);
  hmsc->scsi_blk_addr += len;
  hmsc->scsi_blk_len -= len;

  /* case 6 : Hi = Di */
  hmsc->csw.dDataResidue -= len;

  if (hmsc->scsi_blk_len == 0)
    hmsc->bot_state = USBD_BOT_LAST_DATA_IN;

  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_ProcessWrite (USBD_HandleTypeDef* pdev, uint8_t lun) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*) pdev->pClassData;

  uint32_t len = MIN(hmsc->scsi_blk_len, MSC_MEDIA_PACKET);
  if (((USBD_StorageTypeDef*)pdev->pUserData)->Write (lun,
                                                      hmsc->bot_data,
                                                      hmsc->scsi_blk_addr / hmsc->scsi_blk_size,
                                                      len / hmsc->scsi_blk_size) < 0) {
    SCSI_SenseCode (pdev, lun, HARDWARE_ERROR, WRITE_FAULT);
    return -1;
    }

  hmsc->scsi_blk_addr += len;
  hmsc->scsi_blk_len -= len;

  /* case 12 : Ho = Do */
  hmsc->csw.dDataResidue -= len;

  if (hmsc->scsi_blk_len == 0)
    BOT_SendCSW (pdev, USBD_CSW_CMD_PASSED);
  else /* Prepare EP to Receive next packet */
    USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, hmsc->bot_data, MIN (hmsc->scsi_blk_len, MSC_MEDIA_PACKET));

  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_TestUnitReady (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params)
{
  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  /* case 9 : Hi > D0 */
  if (hmsc->cbw.dDataLength != 0) {
    SCSI_SenseCode (pdev, hmsc->cbw.bLUN, ILLEGAL_REQUEST, INVALID_CDB);
    return -1;
    }

  if (((USBD_StorageTypeDef *)pdev->pUserData)->IsReady(lun) !=0 ) {
    SCSI_SenseCode (pdev, lun, NOT_READY, MEDIUM_NOT_PRESENT);

    hmsc->bot_state = USBD_BOT_NO_DATA;
    return -1;
    }

  hmsc->bot_data_length = 0;
  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_Inquiry (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  uint8_t* pPage;
  uint16_t len;
  if (params[1] & 0x01)/*Evpd is set*/ {
    pPage = (uint8_t*)MSC_Page00_Inquiry_Data;
    len = LENGTH_INQUIRY_PAGE00;
    }
  else {
    pPage = (uint8_t*) & ((USBD_StorageTypeDef*)pdev->pUserData)->pInquiry[lun * STANDARD_INQUIRY_DATA_LEN];
    len = pPage[4] + 5;
    if (params[4] <= len)
      len = params[4];
    }
  hmsc->bot_data_length = len;

  while (len) {
    len--;
    hmsc->bot_data[len] = pPage[len];
    }

  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_ReadCapacity10 (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if (((USBD_StorageTypeDef *)pdev->pUserData)->GetCapacity (lun, &hmsc->scsi_blk_nbr, &hmsc->scsi_blk_size) != 0) {
    SCSI_SenseCode(pdev, lun, NOT_READY, MEDIUM_NOT_PRESENT);
    return -1;
    }
  else {
    hmsc->bot_data[0] = (uint8_t)((hmsc->scsi_blk_nbr - 1) >> 24);
    hmsc->bot_data[1] = (uint8_t)((hmsc->scsi_blk_nbr - 1) >> 16);
    hmsc->bot_data[2] = (uint8_t)((hmsc->scsi_blk_nbr - 1) >>  8);
    hmsc->bot_data[3] = (uint8_t)(hmsc->scsi_blk_nbr - 1);

    hmsc->bot_data[4] = (uint8_t)(hmsc->scsi_blk_size >>  24);
    hmsc->bot_data[5] = (uint8_t)(hmsc->scsi_blk_size >>  16);
    hmsc->bot_data[6] = (uint8_t)(hmsc->scsi_blk_size >>  8);
    hmsc->bot_data[7] = (uint8_t)(hmsc->scsi_blk_size);

    hmsc->bot_data_length = 8;
    return 0;
    }
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_ReadFormatCapacity(USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  for (uint16_t i = 0 ; i < 12 ; i++)
    hmsc->bot_data[i] = 0;

  uint16_t blk_size;
  uint32_t blk_nbr;
  if (((USBD_StorageTypeDef*)pdev->pUserData)->GetCapacity (lun, &blk_nbr, &blk_size) != 0) {
    SCSI_SenseCode (pdev, lun, NOT_READY, MEDIUM_NOT_PRESENT);
    return -1;
    }

  else {
    hmsc->bot_data[3] = 0x08;
    hmsc->bot_data[4] = (uint8_t)((blk_nbr - 1) >> 24);
    hmsc->bot_data[5] = (uint8_t)((blk_nbr - 1) >> 16);
    hmsc->bot_data[6] = (uint8_t)((blk_nbr - 1) >>  8);
    hmsc->bot_data[7] = (uint8_t)(blk_nbr - 1);

    hmsc->bot_data[8] = 0x02;
    hmsc->bot_data[9] = (uint8_t)(blk_size >>  16);
    hmsc->bot_data[10] = (uint8_t)(blk_size >>  8);
    hmsc->bot_data[11] = (uint8_t)(blk_size);

    hmsc->bot_data_length = 12;
    return 0;
    }
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_ModeSense6 (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params)
{
  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  uint16_t len = 8 ;
  hmsc->bot_data_length = len;
  while (len) {
    len--;
    hmsc->bot_data[len] = MSC_Mode_Sense6_data[len];
    }

  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_ModeSense10 (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  uint16_t len = 8;
  hmsc->bot_data_length = len;
  while (len) {
    len--;
    hmsc->bot_data[len] = MSC_Mode_Sense10_data[len];
    }
  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_RequestSense (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  for (uint8_t i = 0; i < REQUEST_SENSE_DATA_LEN ; i++)
    hmsc->bot_data[i] = 0;

  hmsc->bot_data[0] = 0x70;
  hmsc->bot_data[7] = REQUEST_SENSE_DATA_LEN - 6;
  if ((hmsc->scsi_sense_head != hmsc->scsi_sense_tail)) {
    hmsc->bot_data[2] = hmsc->scsi_sense[hmsc->scsi_sense_head].Skey;
    hmsc->bot_data[12] = hmsc->scsi_sense[hmsc->scsi_sense_head].w.b.ASCQ;
    hmsc->bot_data[13] = hmsc->scsi_sense[hmsc->scsi_sense_head].w.b.ASC;
    hmsc->scsi_sense_head++;

    if (hmsc->scsi_sense_head == SENSE_LIST_DEEPTH)
      hmsc->scsi_sense_head = 0;
    }

  hmsc->bot_data_length = REQUEST_SENSE_DATA_LEN;

  if (params[4] <= REQUEST_SENSE_DATA_LEN)
    hmsc->bot_data_length = params[4];

  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_StartStopUnit (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;
  hmsc->bot_data_length = 0;
  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_Read10 (USBD_HandleTypeDef* pdev, uint8_t lun , uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if (hmsc->bot_state == USBD_BOT_IDLE)  /* Idle */ {
    /* case 10 : Ho <> Di */
    if ((hmsc->cbw.bmFlags & 0x80) != 0x80) {
      SCSI_SenseCode (pdev, hmsc->cbw.bLUN, ILLEGAL_REQUEST, INVALID_CDB);
      return -1;
      }

    if (((USBD_StorageTypeDef *)pdev->pUserData)->IsReady(lun) != 0) {
      SCSI_SenseCode (pdev, lun, NOT_READY, MEDIUM_NOT_PRESENT);
      return -1;
      }

    hmsc->scsi_blk_addr = (params[2] << 24) | (params[3] << 16) | (params[4] <<  8) | params[5];
    hmsc->scsi_blk_len =  (params[7] <<  8) | params[8];
    if (SCSI_CheckAddressRange (pdev, lun, hmsc->scsi_blk_addr, hmsc->scsi_blk_len) < 0)
      return -1; /* error */

    hmsc->bot_state = USBD_BOT_DATA_IN;
    hmsc->scsi_blk_addr *= hmsc->scsi_blk_size;
    hmsc->scsi_blk_len *= hmsc->scsi_blk_size;

    /* cases 4,5 : Hi <> Dn */
    if (hmsc->cbw.dDataLength != hmsc->scsi_blk_len) {
      SCSI_SenseCode (pdev, hmsc->cbw.bLUN, ILLEGAL_REQUEST, INVALID_CDB);
      return -1;
      }
    }

  hmsc->bot_data_length = MSC_MEDIA_PACKET;
  return SCSI_ProcessRead(pdev, lun);
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_Write10 (USBD_HandleTypeDef* pdev, uint8_t lun , uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if (hmsc->bot_state == USBD_BOT_IDLE) {
    /* case 8 : Hi <> Do */
    if ((hmsc->cbw.bmFlags & 0x80) == 0x80) {
      SCSI_SenseCode (pdev, hmsc->cbw.bLUN, ILLEGAL_REQUEST, INVALID_CDB);
      return -1;
      }

    /* Check whether Media is ready */
    if (((USBD_StorageTypeDef*)pdev->pUserData)->IsReady(lun) != 0) {
      SCSI_SenseCode (pdev, lun, NOT_READY, MEDIUM_NOT_PRESENT);
      return -1;
      }

    hmsc->scsi_blk_addr = (params[2] << 24) | (params[3] << 16) | (params[4] <<  8) | params[5];
    hmsc->scsi_blk_len = (params[7] <<  8) | params[8];

    /* check if LBA address is in the right range */
    if (SCSI_CheckAddressRange (pdev, lun, hmsc->scsi_blk_addr, hmsc->scsi_blk_len) < 0)
      return -1; /* error */

    hmsc->scsi_blk_addr *= hmsc->scsi_blk_size;
    hmsc->scsi_blk_len  *= hmsc->scsi_blk_size;

    /* cases 3,11,13 : Hn,Ho <> D0 */
    if (hmsc->cbw.dDataLength != hmsc->scsi_blk_len) {
      SCSI_SenseCode (pdev, hmsc->cbw.bLUN, ILLEGAL_REQUEST, INVALID_CDB);
      return -1;
      }

    /* Prepare EP to receive first data packet */
    hmsc->bot_state = USBD_BOT_DATA_OUT;
    USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, hmsc->bot_data, MIN (hmsc->scsi_blk_len, MSC_MEDIA_PACKET));
    }
  else /* Write Process ongoing */
    return SCSI_ProcessWrite (pdev, lun);

  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_Verify10 (USBD_HandleTypeDef* pdev, uint8_t lun , uint8_t* params) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*) pdev->pClassData;

  if ((params[1] & 0x02) == 0x02) {
    SCSI_SenseCode (pdev, lun, ILLEGAL_REQUEST, INVALID_FIELED_IN_COMMAND);
    return -1; /* Error, Verify Mode Not supported*/
    }

  if (SCSI_CheckAddressRange (pdev, lun, hmsc->scsi_blk_addr, hmsc->scsi_blk_len) < 0)
    return -1; /* error */

  hmsc->bot_data_length = 0;
  return 0;
  }
/*}}}*/
/*{{{*/
static int8_t SCSI_ProcessCmd (USBD_HandleTypeDef* pdev, uint8_t lun, uint8_t* params) {

  switch (params[0]) {
    case SCSI_TEST_UNIT_READY:
      return SCSI_TestUnitReady (pdev, lun, params);

    case SCSI_REQUEST_SENSE:
      return SCSI_RequestSense (pdev, lun, params);

    case SCSI_INQUIRY:
      return SCSI_Inquiry (pdev, lun, params);

    case SCSI_START_STOP_UNIT:
      return SCSI_StartStopUnit (pdev, lun, params);

    case SCSI_ALLOW_MEDIUM_REMOVAL:
      return SCSI_StartStopUnit (pdev, lun, params);

    case SCSI_MODE_SENSE6:
      return SCSI_ModeSense6 (pdev, lun, params);

    case SCSI_MODE_SENSE10:
      return SCSI_ModeSense10 (pdev, lun, params);

    case SCSI_READ_FORMAT_CAPACITIES:
      return SCSI_ReadFormatCapacity (pdev, lun, params);

    case SCSI_READ_CAPACITY10:
      return SCSI_ReadCapacity10 (pdev, lun, params);

    case SCSI_READ10:
      return SCSI_Read10 (pdev, lun, params);

    case SCSI_WRITE10:
      return SCSI_Write10 (pdev, lun, params);

    case SCSI_VERIFY10:
      return SCSI_Verify10 (pdev, lun, params);

    default:
      SCSI_SenseCode (pdev, lun, ILLEGAL_REQUEST, INVALID_CDB);
      return -1;
    }
  }
/*}}}*/

// bot
/*{{{*/
static void BOT_Abort (USBD_HandleTypeDef* pdev) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if ((hmsc->cbw.bmFlags == 0) && (hmsc->cbw.dDataLength != 0) && (hmsc->bot_status == USBD_BOT_STATUS_NORMAL) )
    USBD_LL_StallEP (pdev, MSC_EPOUT_ADDR);
  USBD_LL_StallEP (pdev, MSC_EPIN_ADDR);

  if (hmsc->bot_status == USBD_BOT_STATUS_ERROR)
    USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t*)&hmsc->cbw, USBD_BOT_CBW_LENGTH);
  }
/*}}}*/
/*{{{*/
static void BOT_SendData (USBD_HandleTypeDef* pdev, uint8_t* buf, uint16_t len) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  len = MIN (hmsc->cbw.dDataLength, len);
  hmsc->csw.dDataResidue -= len;
  hmsc->csw.bStatus = USBD_CSW_CMD_PASSED;
  hmsc->bot_state = USBD_BOT_SEND_DATA;
  USBD_LL_Transmit (pdev, MSC_EPIN_ADDR, buf, len);
  }
/*}}}*/
/*{{{*/
static void BOT_CBW_Decode (USBD_HandleTypeDef* pdev) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->csw.dTag = hmsc->cbw.dTag;
  hmsc->csw.dDataResidue = hmsc->cbw.dDataLength;

  if ((USBD_LL_GetRxDataSize (pdev ,MSC_EPOUT_ADDR) != USBD_BOT_CBW_LENGTH) ||
      (hmsc->cbw.dSignature != USBD_BOT_CBW_SIGNATURE) || (hmsc->cbw.bLUN > 1) ||
      (hmsc->cbw.bCBLength < 1) || (hmsc->cbw.bCBLength > 16)) {
    SCSI_SenseCode (pdev, hmsc->cbw.bLUN, ILLEGAL_REQUEST, INVALID_CDB);
    hmsc->bot_status = USBD_BOT_STATUS_ERROR;
    BOT_Abort (pdev);
    }

  else {
    if (SCSI_ProcessCmd (pdev, hmsc->cbw.bLUN, &hmsc->cbw.CB[0]) < 0) {
      if (hmsc->bot_state == USBD_BOT_NO_DATA)
        BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
      else
        BOT_Abort (pdev);
      }

    /*Burst xfer handled internally*/
    else if ((hmsc->bot_state != USBD_BOT_DATA_IN) &&
             (hmsc->bot_state != USBD_BOT_DATA_OUT) &&
             (hmsc->bot_state != USBD_BOT_LAST_DATA_IN)) {
      if (hmsc->bot_data_length > 0)
        BOT_SendData (pdev, hmsc->bot_data, hmsc->bot_data_length);
      else if (hmsc->bot_data_length == 0)
        BOT_SendCSW (pdev, USBD_CSW_CMD_PASSED);
      }
    }
  }
/*}}}*/
/*{{{*/
static void BOT_CplClrFeature (USBD_HandleTypeDef* pdev, uint8_t epnum) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if (hmsc->bot_status == USBD_BOT_STATUS_ERROR) {
    /* Bad CBW Signature */
    USBD_LL_StallEP (pdev, MSC_EPIN_ADDR);
    hmsc->bot_status = USBD_BOT_STATUS_NORMAL;
    }
  else if (((epnum & 0x80) == 0x80) && (hmsc->bot_status != USBD_BOT_STATUS_RECOVERY))
    BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
  }
/*}}}*/
/*{{{*/
void BOT_DataOut (USBD_HandleTypeDef* pdev, uint8_t epnum) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  switch (hmsc->bot_state) {
    case USBD_BOT_IDLE:
      BOT_CBW_Decode (pdev);
      break;

    case USBD_BOT_DATA_OUT:
      if (SCSI_ProcessCmd (pdev, hmsc->cbw.bLUN, &hmsc->cbw.CB[0]) < 0)
        BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
      break;

    default:
      break;
    }
  }
/*}}}*/

// msc
/*{{{*/
static uint8_t* USBD_MSC_GetHSCfgDesc (uint16_t* length) {

  *length = sizeof (USBD_MSC_CfgHSDesc);
  return (uint8_t*)USBD_MSC_CfgHSDesc;
  }
/*}}}*/
/*{{{*/
static uint8_t* USBD_MSC_GetFSCfgDesc (uint16_t* length) {
  *length = sizeof (USBD_MSC_CfgFSDesc);
  return (uint8_t*)USBD_MSC_CfgFSDesc;
  }
/*}}}*/
/*{{{*/
static uint8_t* USBD_MSC_GetOtherSpeedCfgDesc (uint16_t* length) {
  *length = sizeof (USBD_MSC_OtherSpeedCfgDesc);
  return (uint8_t*)USBD_MSC_OtherSpeedCfgDesc;
  }
/*}}}*/
/*{{{*/
static uint8_t* USBD_MSC_GetDeviceQualifierDescriptor (uint16_t* length) {
  *length = sizeof (USBD_MSC_DeviceQualifierDesc);
  return (uint8_t*)USBD_MSC_DeviceQualifierDesc;
  }
/*}}}*/
/*{{{*/
static uint8_t USBD_MSC_Init (USBD_HandleTypeDef* pdev, uint8_t cfgidx) {

  if (pdev->dev_speed == USBD_SPEED_HIGH) {
    USBD_LL_OpenEP (pdev, MSC_EPOUT_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_HS_PACKET);
    USBD_LL_OpenEP (pdev, MSC_EPIN_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_HS_PACKET);
    }
  else {
    USBD_LL_OpenEP (pdev, MSC_EPOUT_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
    USBD_LL_OpenEP (pdev, MSC_EPIN_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
    }

  //pdev->pClassData = USBD_malloc (sizeof (USBD_MSC_BOT_HandleTypeDef));
  pdev->pClassData = (void*)USB_BUFFER;

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->bot_state = USBD_BOT_IDLE;
  hmsc->bot_status = USBD_BOT_STATUS_NORMAL;

  hmsc->scsi_sense_tail = 0;
  hmsc->scsi_sense_head = 0;

  USBD_LL_FlushEP (pdev, MSC_EPOUT_ADDR);
  USBD_LL_FlushEP (pdev, MSC_EPIN_ADDR);

  /* Prapare EP to Receive First BOT Cmd */
  USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t*)&hmsc->cbw, USBD_BOT_CBW_LENGTH);

  return 0;
  }
/*}}}*/
/*{{{*/
static uint8_t USBD_MSC_DeInit (USBD_HandleTypeDef* pdev, uint8_t cfgidx) {

  USBD_LL_CloseEP (pdev, MSC_EPOUT_ADDR);
  USBD_LL_CloseEP (pdev, MSC_EPIN_ADDR);

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;
  hmsc->bot_state = USBD_BOT_IDLE;

  return 0;
  }
/*}}}*/
/*{{{*/
static uint8_t USBD_MSC_Setup (USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef *req) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*) pdev->pClassData;

  switch (req->bmRequest & USB_REQ_TYPE_MASK) {
    case USB_REQ_TYPE_CLASS : /* Class request */
      switch (req->bRequest) {
        /*{{{*/
        case BOT_GET_MAX_LUN :
          if ((req->wValue  == 0) && (req->wLength == 1) && ((req->bmRequest & 0x80) == 0x80)) {
            hmsc->max_lun = 0;
            USBD_CtlSendData (pdev, (uint8_t *)&hmsc->max_lun, 1);
            }
          else {
            USBD_CtlError(pdev , req);
            return USBD_FAIL;
            }
          break;
        /*}}}*/
        /*{{{*/
        case BOT_RESET :
          if ((req->wValue  == 0) && (req->wLength == 0) && ((req->bmRequest & 0x80) != 0x80)) {
             USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;
             hmsc->bot_state = USBD_BOT_IDLE;
             hmsc->bot_status = USBD_BOT_STATUS_RECOVERY;
             USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t*)&hmsc->cbw, USBD_BOT_CBW_LENGTH);
             }
          else {
            USBD_CtlError (pdev , req);
            return USBD_FAIL;
            }
          break;
        /*}}}*/
        default:
          USBD_CtlError (pdev , req);
          return USBD_FAIL;
        }
      break;

    case USB_REQ_TYPE_STANDARD: /* Interface & Endpoint request */
      switch (req->bRequest) {
        /*{{{*/
        case USB_REQ_GET_INTERFACE :
          USBD_CtlSendData (pdev, (uint8_t *)&hmsc->interface, 1);
          break;
        /*}}}*/
        /*{{{*/
        case USB_REQ_SET_INTERFACE :
          hmsc->interface = (uint8_t)(req->wValue);
          break;
        /*}}}*/
        /*{{{*/
        case USB_REQ_CLEAR_FEATURE:
          // Flush the FIFO and Clear the stall status
          USBD_LL_FlushEP (pdev, (uint8_t)req->wIndex);

          // Reactivate the EP
          USBD_LL_CloseEP (pdev , (uint8_t)req->wIndex);
          if ((((uint8_t)req->wIndex) & 0x80) == 0x80) {
            if (pdev->dev_speed == USBD_SPEED_HIGH)
              USBD_LL_OpenEP (pdev, MSC_EPIN_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_HS_PACKET);
            else
              USBD_LL_OpenEP (pdev, MSC_EPIN_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
            }
          else {
            if (pdev->dev_speed == USBD_SPEED_HIGH)
              USBD_LL_OpenEP (pdev, MSC_EPOUT_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_HS_PACKET);
            else
              USBD_LL_OpenEP (pdev, MSC_EPOUT_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
            }

          // Handle BOT error
          BOT_CplClrFeature (pdev, (uint8_t)req->wIndex);
          break;
        /*}}}*/
        }
      break;

    default:
      break;
    }

  return 0;
  }
/*}}}*/
/*{{{*/
static uint8_t USBD_MSC_DataIn (USBD_HandleTypeDef* pdev, uint8_t epnum) {

  USBD_MSC_BOT_HandleTypeDef* hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  switch (hmsc->bot_state) {
    case USBD_BOT_DATA_IN:
      if (SCSI_ProcessCmd (pdev, hmsc->cbw.bLUN, &hmsc->cbw.CB[0]) < 0)
        BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
      break;

    case USBD_BOT_SEND_DATA:
    case USBD_BOT_LAST_DATA_IN:
      BOT_SendCSW (pdev, USBD_CSW_CMD_PASSED);
      break;

    default:
      break;
    }

  return 0;
  }
/*}}}*/
/*{{{*/
static uint8_t USBD_MSC_DataOut (USBD_HandleTypeDef* pdev, uint8_t epnum) {
  BOT_DataOut (pdev, epnum);
  return 0;
  }
/*}}}*/

/*{{{*/
void USBD_MSC_RegisterStorage (USBD_HandleTypeDef* pdev, USBD_StorageTypeDef* fops) {
  pdev->pUserData = fops;
  }
/*}}}*/
/*{{{*/
USBD_ClassTypeDef USBD_MSC = {
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
