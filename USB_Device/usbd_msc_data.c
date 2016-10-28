// usbd_msc_data.c
#include "usbd_msc_data.h"

const uint8_t MSC_Page00_Inquiry_Data[] = { //7
  0x00,
  0x00,
  0x00,
  (LENGTH_INQUIRY_PAGE00 - 4),
  0x00,
  0x80,
  0x83
  };

const uint8_t MSC_Mode_Sense6_data[] = {
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00
  };

const uint8_t MSC_Mode_Sense10_data[] = {
  0x00,
  0x06,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00
  };
