/*{{{  ff.c copyright*/
// FatFs - FAT file system module  R0.11                 (C)ChaN, 2015
// FatFs module is a free software that opened under license policy of following conditions.
// Copyright (C) 2015, ChaN, all right reserved.
// 1. Redistributions of source code must retain the above copyright notice,
//    this condition and the following disclaimer.
// This software is provided by the copyright holder and contributors "AS IS"
// and any warranties related to this software are DISCLAIMED.
// The copyright owner or contributors be NOT LIABLE for any damages caused by use of this software.
/*}}}*/
/*{{{  includes*/
#include "string.h"
#include <stdarg.h>

#include "ff.h"
#include "diskio.h"
/*}}}*/
/*{{{  ff_conf defines*/
#define _USE_TRIM   0

#define _FS_LOCK    2  /* 0:Disable or >=1:Enable */

#define N_FATS      1  /* Number of FATs (1 or 2) */
#define N_ROOTDIR 512  /* Number of root directory entries for FAT12/16 */

#define _NORTC_MDAY  1
#define _NORTC_MON   4
#define _NORTC_YEAR  2016
#define GET_FATTIME() ((DWORD)(_NORTC_YEAR - 1980) << 25 | (DWORD)_NORTC_MON << 21 | (DWORD)_NORTC_MDAY << 16)
/*}}}*/
/*{{{  code pages defines*/
/* DBCS code ranges and SBCS extend character conversion table */
/*{{{*/
#if _CODE_PAGE == 932 /* Japanese Shift-JIS */
  #define _DF1S 0x81  /* DBC 1st byte range 1 start */
  #define _DF1E 0x9F  /* DBC 1st byte range 1 end */
  #define _DF2S 0xE0  /* DBC 1st byte range 2 start */
  #define _DF2E 0xFC  /* DBC 1st byte range 2 end */
  #define _DS1S 0x40  /* DBC 2nd byte range 1 start */
  #define _DS1E 0x7E  /* DBC 2nd byte range 1 end */
  #define _DS2S 0x80  /* DBC 2nd byte range 2 start */
  #define _DS2E 0xFC  /* DBC 2nd byte range 2 end */
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 936 /* Simplified Chinese GBK */
  #define _DF1S 0x81
  #define _DF1E 0xFE
  #define _DS1S 0x40
  #define _DS1E 0x7E
  #define _DS2S 0x80
  #define _DS2E 0xFE
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 949 /* Korean */
  #define _DF1S 0x81
  #define _DF1E 0xFE
  #define _DS1S 0x41
  #define _DS1E 0x5A
  #define _DS2S 0x61
  #define _DS2E 0x7A
  #define _DS3S 0x81
  #define _DS3E 0xFE
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 950 /* Traditional Chinese Big5 */
  #define _DF1S 0x81
  #define _DF1E 0xFE
  #define _DS1S 0x40
  #define _DS1E 0x7E
  #define _DS2S 0xA1
  #define _DS2E 0xFE
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 437 /* U.S. (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x9A,0x90,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F,0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
          0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 720 /* Arabic (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x81,0x45,0x41,0x84,0x41,0x86,0x43,0x45,0x45,0x45,0x49,0x49,0x8D,0x8E,0x8F,0x90,0x92,0x92,0x93,0x94,0x95,0x49,0x49,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
          0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 737 /* Greek (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x92,0x92,0x93,0x94,0x95,0x96,0x97,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87, \
          0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0xAA,0x92,0x93,0x94,0x95,0x96,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0x97,0xEA,0xEB,0xEC,0xE4,0xED,0xEE,0xE7,0xE8,0xF1,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 775 /* Baltic (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x9A,0x91,0xA0,0x8E,0x95,0x8F,0x80,0xAD,0xED,0x8A,0x8A,0xA1,0x8D,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0x95,0x96,0x97,0x97,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
          0xA0,0xA1,0xE0,0xA3,0xA3,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xB5,0xB6,0xB7,0xB8,0xBD,0xBE,0xC6,0xC7,0xA5,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE3,0xE8,0xE8,0xEA,0xEA,0xEE,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 850 /* Multilingual Latin 1 (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0xDE,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x59,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
          0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE7,0xE9,0xEA,0xEB,0xED,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 852 /* Latin 2 (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xDE,0x8F,0x80,0x9D,0xD3,0x8A,0x8A,0xD7,0x8D,0x8E,0x8F,0x90,0x91,0x91,0xE2,0x99,0x95,0x95,0x97,0x97,0x99,0x9A,0x9B,0x9B,0x9D,0x9E,0x9F, \
          0xB5,0xD6,0xE0,0xE9,0xA4,0xA4,0xA6,0xA6,0xA8,0xA8,0xAA,0x8D,0xAC,0xB8,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBD,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC6,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD1,0xD1,0xD2,0xD3,0xD2,0xD5,0xD6,0xD7,0xB7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0xE0,0xE1,0xE2,0xE3,0xE3,0xD5,0xE6,0xE6,0xE8,0xE9,0xE8,0xEB,0xED,0xED,0xDD,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xEB,0xFC,0xFC,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 855 /* Cyrillic (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x81,0x81,0x83,0x83,0x85,0x85,0x87,0x87,0x89,0x89,0x8B,0x8B,0x8D,0x8D,0x8F,0x8F,0x91,0x91,0x93,0x93,0x95,0x95,0x97,0x97,0x99,0x99,0x9B,0x9B,0x9D,0x9D,0x9F,0x9F, \
          0xA1,0xA1,0xA3,0xA3,0xA5,0xA5,0xA7,0xA7,0xA9,0xA9,0xAB,0xAB,0xAD,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB6,0xB6,0xB8,0xB8,0xB9,0xBA,0xBB,0xBC,0xBE,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD1,0xD1,0xD3,0xD3,0xD5,0xD5,0xD7,0xD7,0xDD,0xD9,0xDA,0xDB,0xDC,0xDD,0xE0,0xDF, \
          0xE0,0xE2,0xE2,0xE4,0xE4,0xE6,0xE6,0xE8,0xE8,0xEA,0xEA,0xEC,0xEC,0xEE,0xEE,0xEF,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF8,0xFA,0xFA,0xFC,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 857 /* Turkish (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0x98,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x98,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9E, \
          0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA6,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xDE,0x59,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 858 /* Multilingual Latin 1 + Euro (OEM) */
  #define _DF1S 0
  #define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0xDE,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x59,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
          0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
          0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD1,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
          0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE7,0xE9,0xEA,0xEB,0xED,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 862 /* Hebrew (OEM) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
        0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 866 /* Russian (OEM) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
        0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0x90,0x91,0x92,0x93,0x9d,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xF0,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 874 /* Thai (OEM, Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
        0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1250 /* Central Europe (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x8A,0x9B,0x8C,0x8D,0x8E,0x8F, \
        0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xA3,0xB4,0xB5,0xB6,0xB7,0xB8,0xA5,0xAA,0xBB,0xBC,0xBD,0xBC,0xAF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1251 /* Cyrillic (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x82,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x80,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x8A,0x9B,0x8C,0x8D,0x8E,0x8F, \
        0xA0,0xA2,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB2,0xA5,0xB5,0xB6,0xB7,0xA8,0xB9,0xAA,0xBB,0xA3,0xBD,0xBD,0xAF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1252 /* Latin 1 (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0xAd,0x9B,0x8C,0x9D,0xAE,0x9F, \
        0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0x9F}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1253 /* Greek (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
        0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xA2,0xB8,0xB9,0xBA, \
        0xE0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xF2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xFB,0xBC,0xFD,0xBF,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1254 /* Turkish (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x8A,0x9B,0x8C,0x9D,0x9E,0x9F, \
        0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0x9F}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1255 /* Hebrew (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
        0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1256 /* Arabic (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x8C,0x9D,0x9E,0x9F, \
        0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0x41,0xE1,0x41,0xE3,0xE4,0xE5,0xE6,0x43,0x45,0x45,0x45,0x45,0xEC,0xED,0x49,0x49,0xF0,0xF1,0xF2,0xF3,0x4F,0xF5,0xF6,0xF7,0xF8,0x55,0xFA,0x55,0x55,0xFD,0xFE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1257 /* Baltic (Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
        0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xA8,0xB9,0xAA,0xBB,0xBC,0xBD,0xBE,0xAF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xFF}
/*}}}*/
/*{{{*/
#elif _CODE_PAGE == 1258 /* Vietnam (OEM, Windows) */
#define _DF1S 0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0xAC,0x9D,0x9E,0x9F, \
        0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
        0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xEC,0xCD,0xCE,0xCF,0xD0,0xD1,0xF2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xFE,0x9F}
/*}}}*/
#elif _CODE_PAGE == 1 /* ASCII (for only non-LFN cfg) */
  #define _DF1S 0
#else
  #error Unknown code page
#endif

/* Character code support macros */
#define IsUpper(c)  (((c)>='A')&&((c)<='Z'))
#define IsLower(c)  (((c)>='a')&&((c)<='z'))
#define IsDigit(c)  (((c)>='0')&&((c)<='9'))

#if _DF1S   /* Code page is DBCS */
  #ifdef _DF2S  /* Two 1st byte areas */
    #define IsDBCS1(c)  (((BYTE)(c) >= _DF1S && (BYTE)(c) <= _DF1E) || ((BYTE)(c) >= _DF2S && (BYTE)(c) <= _DF2E))
  #else     /* One 1st byte area */
    #define IsDBCS1(c)  ((BYTE)(c) >= _DF1S && (BYTE)(c) <= _DF1E)
  #endif

  #ifdef _DS3S  /* Three 2nd byte areas */
    #define IsDBCS2(c)  (((BYTE)(c) >= _DS1S && (BYTE)(c) <= _DS1E) || ((BYTE)(c) >= _DS2S && (BYTE)(c) <= _DS2E) || ((BYTE)(c) >= _DS3S && (BYTE)(c) <= _DS3E))
  #else     /* Two 2nd byte areas */
    #define IsDBCS2(c)  (((BYTE)(c) >= _DS1S && (BYTE)(c) <= _DS1E) || ((BYTE)(c) >= _DS2S && (BYTE)(c) <= _DS2E))
  #endif
#else     /* Code page is SBCS */
  #define IsDBCS1(c)  0
  #define IsDBCS2(c)  0
#endif /* _DF1S */
/*}}}*/
/*{{{  name status flags defines*/
#define NSFLAG   11    /* Index of name status byte in fn[] */

#define NS_LOSS  0x01  /* Out of 8.3 format */
#define NS_LFN   0x02  /* Force to create LFN entry */
#define NS_LAST  0x04  /* Last segment */
#define NS_BODY  0x08  /* Lower case flag (body) */
#define NS_EXT   0x10  /* Lower case flag (ext) */
#define NS_DOT   0x20  /* Dot entry */
/*}}}*/
/*{{{  fatFs defines*/
#define MIN_FAT16    4086U    /* Minimum number of clusters as FAT16 */
#define MIN_FAT32   65526U    /* Minimum number of clusters as FAT32 */

#define BS_jmpBoot       0    /* x86 jump instruction (3) */
#define BS_OEMName       3    /* OEM name (8) */
#define BPB_BytsPerSec  11    /* Sector size [byte] (2) */
#define BPB_SecPerClus  13    /* Cluster size [sector] (1) */
#define BPB_RsvdSecCnt  14    /* Size of reserved area [sector] (2) */
#define BPB_NumFATs     16    /* Number of FAT copies (1) */
#define BPB_RootEntCnt  17    /* Number of root directory entries for FAT12/16 (2) */
#define BPB_TotSec16    19    /* Volume size [sector] (2) */
#define BPB_Media       21    /* Media descriptor (1) */
#define BPB_FATSz16     22    /* FAT size [sector] (2) */
#define BPB_SecPerTrk   24    /* Track size [sector] (2) */
#define BPB_NumHeads    26    /* Number of heads (2) */
#define BPB_HiddSec     28    /* Number of special hidden sectors (4) */
#define BPB_TotSec32    32    /* Volume size [sector] (4) */
#define BS_DrvNum       36    /* Physical drive number (2) */
#define BS_BootSig      38    /* Extended boot signature (1) */
#define BS_VolID        39    /* Volume serial number (4) */
#define BS_VolLab       43    /* Volume label (8) */
#define BS_FilSysType   54    /* File system type (1) */
#define BPB_FATSz32     36    /* FAT size [sector] (4) */
#define BPB_ExtFlags    40    /* Extended flags (2) */
#define BPB_FSVer       42    /* File system version (2) */
#define BPB_RootClus    44    /* Root directory first cluster (4) */
#define BPB_FSInfo      48    /* Offset of FSINFO sector (2) */
#define BPB_BkBootSec   50    /* Offset of backup boot sector (2) */
#define BS_DrvNum32     64    /* Physical drive number (2) */
#define BS_BootSig32    66    /* Extended boot signature (1) */
#define BS_VolID32      67    /* Volume serial number (4) */
#define BS_VolLab32     71    /* Volume label (8) */
#define BS_FilSysType32 82    /* File system type (1) */

#define FSI_LeadSig     0     /* FSI: Leading signature (4) */
#define FSI_StrucSig    484   /* FSI: Structure signature (4) */
#define FSI_Free_Count  488   /* FSI: Number of free clusters (4) */
#define FSI_Nxt_Free    492   /* FSI: Last allocated cluster (4) */
#define MBR_Table       446   /* MBR: Partition table offset (2) */
#define SZ_PTE          16    /* MBR: Size of a partition table entry */
#define BS_55AA         510   /* Signature word (2) */

#define DIR_Name          0   /* Short file name (11) */
#define DIR_Attr         11   /* Attribute (1) */
#define DIR_NTres        12   /* Lower case flag (1) */
#define DIR_CrtTimeTenth 13   /* Created time sub-second (1) */
#define DIR_CrtTime      14   /* Created time (2) */
#define DIR_CrtDate      16   /* Created date (2) */
#define DIR_LstAccDate   18   /* Last accessed date (2) */
#define DIR_FstClusHI    20   /* Higher 16-bit of first cluster (2) */
#define DIR_WrtTime      22   /* Modified time (2) */
#define DIR_WrtDate      24   /* Modified date (2) */
#define DIR_FstClusLO    26   /* Lower 16-bit of first cluster (2) */
#define DIR_FileSize     28   /* File size (4) */

#define LDIR_Ord          0   /* LFN entry order and LLE flag (1) */
#define LDIR_Attr        11   /* LFN attribute (1) */
#define LDIR_Type        12   /* LFN type (1) */
#define LDIR_Chksum      13   /* Sum of corresponding SFN entry */
#define LDIR_FstClusLO   26   /* Must be zero (0) */
#define SZ_DIRE          32   /* Size of a directory entry */

#define LLEF            0x40  /* Last long entry flag in LDIR_Ord */
#define DDEM            0xE5  /* Deleted directory entry mark at DIR_Name[0] */
#define RDDEM           0x05  /* Replacement of the character collides with DDEM */
/*}}}*/
/*{{{  struct putbuff*/
typedef struct {
  FIL* fp;
  int idx, nchr;
  BYTE buf[64];
  } putbuff;
/*}}}*/
/*{{{  static const*/
static const BYTE LfnOfs[] = { 1,3,5,7,9,14,16,18,20,22,24,28,30 };  /* Offset of LFN characters in the directory entry */

static const WORD vst[] = { 1024,   512,  256,  128,   64,    32,   16,    8,    4,    2,   0};
static const WORD cst[] = {32768, 16384, 8192, 4096, 2048, 16384, 8192, 4096, 2048, 1024, 512};

#ifdef _EXCVT
  static const BYTE ExCvt[] = _EXCVT; /* Upper conversion table for extended characters */
#endif
/*}}}*/
/*{{{  static vars*/
static FATFS* FatFs[1]; /* Pointer to the file system objects (logical drives) */
static WORD Fsid;       /* File system mount ID */

/*{{{  FILESEM struct*/
typedef struct {
  FATFS *fs;  /* Object ID 1, volume (NULL:blank entry) */
  DWORD clu;  /* Object ID 2, directory (0:root) */
  WORD idx;   /* Object ID 3, directory index */
  WORD ctr;   /* Object open counter, 0:none, 0x01..0xFF:read mode open count, 0x100:write mode */
  } FILESEM;
/*}}}*/
static FILESEM Files[_FS_LOCK]; /* Open object lock semaphores */
/*}}}*/

/*{{{  lock, synchronisation*/
/*{{{*/
static int ff_cre_syncobj (BYTE vol, osSemaphoreId *sobj) {

  osSemaphoreDef (SEM);
  *sobj = osSemaphoreCreate (osSemaphore(SEM), 1);
  return *sobj != NULL;
  }
/*}}}*/
/*{{{*/
static int ff_del_syncobj (osSemaphoreId sobj) {

  osSemaphoreDelete (sobj);
  return 1;
  }
/*}}}*/
/*{{{*/
static int ff_req_grant (osSemaphoreId sobj) {
  return (osSemaphoreWait (sobj, 1000) == osOK) ? 1 : 0;
  }
/*}}}*/
/*{{{*/
static void ff_rel_grant (osSemaphoreId sobj) {

  osSemaphoreRelease(sobj);
  }
/*}}}*/

/*{{{*/
static void unlock_fs (FATFS* fs, FRESULT res) {

  if (fs && res != FR_NOT_ENABLED && res != FR_INVALID_DRIVE && res != FR_INVALID_OBJECT && res != FR_TIMEOUT)
    ff_rel_grant (fs->sobj);
  }
/*}}}*/

#define ENTER_FF(fs)      { if (!ff_req_grant (fs->sobj)) return FR_TIMEOUT; }
#define LEAVE_FF(fs, res) { unlock_fs(fs, res); return res; }
#define ABORT(fs, res)    { file->err = (BYTE)(res); LEAVE_FF(fs, res); }

/*{{{*/
static FRESULT chk_lock (DIR* dp, int acc) {

  UINT i, be;

  // Search file semaphore table
  for (i = be = 0; i < _FS_LOCK; i++) {
    if (Files[i].fs) {
      // Existing entry
      if (Files[i].fs == dp->fs && Files[i].clu == dp->sclust && Files[i].idx == dp->index)
         break;
      }
    else
      // Blank entry
      be = 1;
    }

  if (i == _FS_LOCK)
    // The object is not opened
    return (be || acc == 2) ? FR_OK : FR_TOO_MANY_OPEN_FILES; /* Is there a blank entry for new object? */

  /* The object has been opened. Reject any open against writing file and all write mode open */
  return (acc || Files[i].ctr == 0x100) ? FR_LOCKED : FR_OK;
  }
/*}}}*/
/*{{{*/
static int enq_lock (void) {

  UINT i;
  for (i = 0; i < _FS_LOCK && Files[i].fs; i++) ;
  return (i == _FS_LOCK) ? 0 : 1;
  }
/*}}}*/
/*{{{*/
static UINT inc_lock (DIR* dp, int acc) {

  UINT i;
  for (i = 0; i < _FS_LOCK; i++) {  /* Find the object */
    if (Files[i].fs == dp->fs &&
      Files[i].clu == dp->sclust &&
      Files[i].idx == dp->index) break;
    }

  if (i == _FS_LOCK) {        /* Not opened. Register it as new. */
    for (i = 0; i < _FS_LOCK && Files[i].fs; i++) ;
    if (i == _FS_LOCK)
      return 0;  /* No free entry to register (int err) */
    Files[i].fs = dp->fs;
    Files[i].clu = dp->sclust;
    Files[i].idx = dp->index;
    Files[i].ctr = 0;
    }

  if (acc && Files[i].ctr)
    return 0;  /* Access violation (int err) */

  Files[i].ctr = acc ? 0x100 : Files[i].ctr + 1;  /* Set semaphore value */

  return i + 1;
  }
/*}}}*/
/*{{{*/
static FRESULT dec_lock (UINT i) {

  if (--i < _FS_LOCK) {
    /* Shift index number origin from 0 */
    WORD n = Files[i].ctr;
    if (n == 0x100)
      n = 0;    /* If write mode open, delete the entry */
    if (n)
      n--;         /* Decrement read mode open count */

    Files[i].ctr = n;
    if (!n)
      Files[i].fs = 0;  /* Delete the entry if open count gets zero */

    return FR_OK;
    }
  else
    return FR_INT_ERR;     /* Invalid index nunber */
  }
/*}}}*/
/*{{{*/
static void clear_lock (FATFS* fs) {

  for (UINT i = 0; i < _FS_LOCK; i++)
    if (Files[i].fs == fs)
      Files[i].fs = 0;
  }
/*}}}*/
/*}}}*/
/*{{{  util*/
/*{{{*/
static int mem_cmp (const void* dst, const void* src, UINT cnt) {
/* Compare memory to memory */

  const BYTE *d = (const BYTE *)dst, *s = (const BYTE *)src;
  int r = 0;

  while (cnt-- && (r = *d++ - *s++) == 0) ;
  return r;
}
/*}}}*/
/*{{{*/
static int chk_chr (const char* str, int chr) {
/* Check if chr is contained in the string */

  while (*str && *str != chr)
    str++;
  return *str;
  }
/*}}}*/

/*{{{*/
static WCHAR get_achar (const TCHAR** ptr) {

  WCHAR chr;

#if !_LFN_UNICODE
  chr = (BYTE)*(*ptr)++;          /* Get a byte */
  if (IsLower(chr))
    chr -= 0x20;      /* To upper ASCII char */
  if (IsDBCS1(chr) && IsDBCS2(**ptr))   /* Get DBC 2nd byte if needed */
    chr = chr << 8 | (BYTE)*(*ptr)++;

  #ifdef _EXCVT
    if (chr >= 0x80)
      chr = ExCvt[chr - 0x80]; /* To upper SBCS extended char */
  #endif

#else
  chr = ff_wtoupper(*(*ptr)++);     /* Get a word and to upper */
#endif

  return chr;
  }
/*}}}*/
/*{{{*/
static int pattern_matching (const TCHAR* pat, const TCHAR* nam, int skip, int inf) {

  while (skip--) {
    /* Pre-skip name chars */
    if (!get_achar (&nam))
      return 0; /* Branch mismatched if less name chars */
    }
  if (!*pat && inf)
    return 1;   /* (short circuit) */

  WCHAR nc;
  do {
    const TCHAR* pp = pat;
    const TCHAR* np = nam;  /* Top of pattern and name to match */
    for (;;) {
      if (*pp == '?' || *pp == '*') {
        /* Wildcard? */
        int nm = 0;
        int nx = 0;
        do {
          /* Analyze the wildcard chars */
          if (*pp++ == '?')
            nm++;
          else
            nx = 1;
          } while (*pp == '?' || *pp == '*');

        if (pattern_matching (pp, np, nm, nx))
          return 1; /* Test new branch (recurs upto number of wildcard blocks in the pattern) */

        nc = *np;
        break;  /* Branch mismatched */
        }

      WCHAR pc = get_achar (&pp);  /* Get a pattern char */
      nc = get_achar (&np);  /* Get a name char */
      if (pc != nc)
        break;  /* Branch mismatched? */
      if (!pc)
        return 1;    /* Branch matched? (matched at end of both strings) */
      }

    get_achar (&nam);     /* nam++ */
    } while (inf && nc);  /* Retry until end of name if infinite search is specified */

  return 0;
  }
/*}}}*/

/*{{{*/
static void putc_bfd (putbuff* pb, TCHAR c) {

  if (c == '\n')
    /* LF -> CRLF conversion */
    putc_bfd (pb, '\r');

  /* Buffer write index (-1:error) */
  int i = pb->idx;
  if (i < 0)
    return;

#if _LFN_UNICODE
  #if _STRF_ENCODE == 3     /* Write a character in UTF-8 */
    if (c < 0x80)        /* 7-bit */
      pb->buf[i++] = (BYTE)c;
    else {
      if (c < 0x800)     /* 11-bit */
        pb->buf[i++] = (BYTE)(0xC0 | c >> 6);
      else {        /* 16-bit */
        pb->buf[i++] = (BYTE)(0xE0 | c >> 12);
        pb->buf[i++] = (BYTE)(0x80 | (c >> 6 & 0x3F));
        }
      pb->buf[i++] = (BYTE)(0x80 | (c & 0x3F));
      }
  #elif _STRF_ENCODE == 2
    // Write a character in UTF-16BE
    pb->buf[i++] = (BYTE)(c >> 8);
    pb->buf[i++] = (BYTE)c;
  #elif _STRF_ENCODE == 1
    // Write a character in UTF-16LE
    pb->buf[i++] = (BYTE)c;
    pb->buf[i++] = (BYTE)(c >> 8);
  #else
    // Write a character in ANSI/OEM, Unicode -> OEM
    c = ff_convert (c, 0);
    if (!c)
      c = '?';
    if (c >= 0x100)
      pb->buf[i++] = (BYTE)(c >> 8);
    pb->buf[i++] = (BYTE)c;
  #endif
#else
  // Write a character without conversion
  pb->buf[i++] = (BYTE)c;
#endif

  if (i >= (int)(sizeof pb->buf) - 3) {
    // Write buffered characters to the file
    UINT bw;
    f_write (pb->fp, pb->buf, (UINT)i, &bw);
    i = (bw == (UINT)i) ? 0 : -1;
    }

  pb->idx = i;
  pb->nchr++;
  }
/*}}}*/
/*}}}*/
/*{{{  window*/
/*{{{*/
static FRESULT syncWindow (FATFS* fs) {

  FRESULT res = FR_OK;

  if (fs->wflag) {  /* Write back the sector if it is dirty */
    DWORD wsect = fs->winsect;  /* Current sector number */
    if (disk_write (fs->drv, fs->win.d8, wsect, 1) != RES_OK)
      res = FR_DISK_ERR;
    else {
      fs->wflag = 0;
      if (wsect - fs->fatbase < fs->fsize) {    /* Is it in the FAT area? */
        for (UINT nf = fs->n_fats; nf >= 2; nf--) {  /* Reflect the change to all FAT copies */
          wsect += fs->fsize;
          disk_write(fs->drv, fs->win.d8, wsect, 1);
          }
        }
      }
    }

  return res;
  }
/*}}}*/
/*{{{*/
static FRESULT moveWindow (FATFS* fs, DWORD sector) {

  FRESULT res = FR_OK;
  if (sector != fs->winsect) {
    /* Window offset changed?  Write-back changes */
    res = syncWindow(fs);
    if (res == FR_OK) {
      /* Fill sector window with new data */
      if (disk_read (fs->drv, fs->win.d8, sector, 1) != RES_OK) {
        /* Invalidate window if data is not reliable */
        sector = 0xFFFFFFFF;
        res = FR_DISK_ERR;
        }
      fs->winsect = sector;
      }
    }

  return res;
  }
/*}}}*/
/*{{{*/
static BYTE checkFs (FATFS* fs, DWORD sect) {

  // Invalidate window
  fs->wflag = 0;
  fs->winsect = 0xFFFFFFFF;

  // Load boot record
  if (moveWindow (fs, sect) != FR_OK)
    return 3;

  // Check boot record signature (always placed at offset 510 even if the sector size is >512)
  if (LD_WORD(&fs->win.d8[BS_55AA]) != 0xAA55)
    return 2;

  // Check "FAT" string
  if ((LD_DWORD(&fs->win.d8[BS_FilSysType]) & 0xFFFFFF) == 0x544146)
    return 0;

  // Check "FAT" string
  if ((LD_DWORD(&fs->win.d8[BS_FilSysType32]) & 0xFFFFFF) == 0x544146)
    return 0;

  return 1;
  }
/*}}}*/
/*{{{*/
static FRESULT syncFs (FATFS* fs) {

  FRESULT res = syncWindow(fs);
  if (res == FR_OK) {
    /* Update FSINFO sector if needed */
    if (fs->fs_type == FS_FAT32 && fs->fsi_flag == 1) {
      /* Create FSINFO structure */
      memset (fs->win.d8, 0, _MAX_SS);
      ST_WORD(fs->win.d8 + BS_55AA, 0xAA55);
      ST_DWORD(fs->win.d8 + FSI_LeadSig, 0x41615252);
      ST_DWORD(fs->win.d8 + FSI_StrucSig, 0x61417272);
      ST_DWORD(fs->win.d8 + FSI_Free_Count, fs->free_clust);
      ST_DWORD(fs->win.d8 + FSI_Nxt_Free, fs->last_clust);
      /* Write it into the FSINFO sector */
      fs->winsect = fs->volbase + 1;
      disk_write (fs->drv, fs->win.d8, fs->winsect, 1);
      fs->fsi_flag = 0;
      }

    /* Make sure that no pending write process in the physical drive */
    if (disk_ioctl(fs->drv, CTRL_SYNC, 0) != RES_OK)
      res = FR_DISK_ERR;
    }

  return res;
  }
/*}}}*/
/*}}}*/
/*{{{  cluster*/
/*{{{*/
static DWORD clusterToSector (FATFS* fs, DWORD clst) {

  clst -= 2;
  return (clst >= fs->n_fatent - 2) ? 0 : clst * fs->csize + fs->database;
  }
/*}}}*/
/*{{{*/
static DWORD clmtCluster (FIL* fp, DWORD ofs) {

  /* Top of CLMT */
  DWORD* tbl = fp->cltbl + 1;

  /* Cluster order from top of the file */
  DWORD cl = ofs / _MAX_SS / fp->fs->csize;
  for (;;) {
    /* Number of cluters in the fragment */
    DWORD ncl = *tbl++;
    if (!ncl)
      return 0;   /* End of table? (error) */

    if (cl < ncl)
      break;  /* In this fragment? */

    cl -= ncl;
    tbl++;     /* Next fragment */
    }

  /* Return the cluster number */
  return cl + *tbl;
  }
/*}}}*/
/*{{{*/
static DWORD loadCluster (FATFS* fs, BYTE* dir) {

  DWORD cl = LD_WORD(dir + DIR_FstClusLO);
  if (fs->fs_type == FS_FAT32)
    cl |= (DWORD)LD_WORD(dir + DIR_FstClusHI) << 16;

  return cl;
  }
/*}}}*/
/*{{{*/
static void storeCluster (BYTE* dir, DWORD cl) {

  ST_WORD(dir + DIR_FstClusLO, cl);
  ST_WORD(dir + DIR_FstClusHI, cl >> 16);
  }
/*}}}*/
/*}}}*/
/*{{{  fat*/
/*{{{*/
static DWORD getFat (FATFS* fs, DWORD clst) {

  UINT wc, bc;
  BYTE *p;
  DWORD val;

  if (clst < 2 || clst >= fs->n_fatent)  /* Check range */
    val = 1;  /* Internal error */
  else {
    val = 0xFFFFFFFF; /* Default value falls on disk error */

    switch (fs->fs_type) {
    case FS_FAT12 :
      bc = (UINT)clst; bc += bc / 2;
      if (moveWindow (fs, fs->fatbase + (bc / _MAX_SS)) != FR_OK)
        break;

      wc = fs->win.d8[bc++ % _MAX_SS];
      if (moveWindow(fs, fs->fatbase + (bc / _MAX_SS)) != FR_OK)
        break;

      wc |= fs->win.d8[bc % _MAX_SS] << 8;
      val = clst & 1 ? wc >> 4 : (wc & 0xFFF);
      break;

    case FS_FAT16 :
      if (moveWindow(fs, fs->fatbase + (clst / (_MAX_SS / 2))) != FR_OK)
        break;
      p = &fs->win.d8[clst * 2 % _MAX_SS];
      val = LD_WORD(p);
      break;

    case FS_FAT32 :
      if (moveWindow(fs, fs->fatbase + (clst / (_MAX_SS / 4))) != FR_OK)
        break;
      p = &fs->win.d8[clst * 4 % _MAX_SS];
      val = LD_DWORD(p) & 0x0FFFFFFF;
      break;

    default:
      val = 1;  /* Internal error */
      }
    }

  return val;
  }
/*}}}*/
/*{{{*/
static FRESULT putFat (FATFS* fs, DWORD clst, DWORD val) {

  UINT bc;
  BYTE *p;
  FRESULT res;

  if (clst < 2 || clst >= fs->n_fatent)  /* Check range */
    res = FR_INT_ERR;
  else {
    switch (fs->fs_type) {
    case FS_FAT12 :
      bc = (UINT)clst; bc += bc / 2;
      res = moveWindow (fs, fs->fatbase + (bc / _MAX_SS));
      if (res != FR_OK)
        break;
      p = &fs->win.d8[bc++ % _MAX_SS];
      *p = (clst & 1) ? ((*p & 0x0F) | ((BYTE)val << 4)) : (BYTE)val;
      fs->wflag = 1;
      res = moveWindow(fs, fs->fatbase + (bc / _MAX_SS));
      if (res != FR_OK)
        break;
      p = &fs->win.d8[bc % _MAX_SS];
      *p = (clst & 1) ? (BYTE)(val >> 4) : ((*p & 0xF0) | ((BYTE)(val >> 8) & 0x0F));
      fs->wflag = 1;
      break;

    case FS_FAT16 :
      res = moveWindow (fs, fs->fatbase + (clst / (_MAX_SS / 2)));
      if (res != FR_OK)
        break;
      p = &fs->win.d8[clst * 2 % _MAX_SS];
      ST_WORD(p, (WORD)val);
      fs->wflag = 1;
      break;

    case FS_FAT32 :
      res = moveWindow (fs, fs->fatbase + (clst / (_MAX_SS / 4)));
      if (res != FR_OK)
        break;
      p = &fs->win.d8[clst * 4 % _MAX_SS];
      val |= LD_DWORD(p) & 0xF0000000;
      ST_DWORD(p, val);
      fs->wflag = 1;
      break;

    default :
      res = FR_INT_ERR;
      }
    }

  return res;
  }
/*}}}*/
/*}}}*/
/*{{{  chain*/
/*{{{*/
static DWORD createChain (FATFS* fs, DWORD clst) {

  DWORD scl;
  if (clst == 0) {    /* Create a new chain */
    scl = fs->last_clust;     /* Get suggested start point */
    if (!scl || scl >= fs->n_fatent) scl = 1;
    }
  else {          /* Stretch the current chain */
    DWORD cs = getFat (fs, clst);     /* Check the cluster status */
    if (cs < 2)
      return 1;     /* Invalid value */
    if (cs == 0xFFFFFFFF)
      return cs;  /* A disk error occurred */
    if (cs < fs->n_fatent)
      return cs; /* It is already followed by next cluster */
    scl = clst;
    }

 DWORD ncl = scl;        /* Start cluster */
  for (;;) {
    ncl++;              /* Next cluster */
    if (ncl >= fs->n_fatent) {    /* Check wrap around */
      ncl = 2;
      if (ncl > scl) return 0;  /* No free cluster */
      }
    DWORD cs = getFat (fs, ncl);      /* Get the cluster status */
    if (cs == 0)
      break;       /* Found a free cluster */
    if (cs == 0xFFFFFFFF || cs == 1)/* An error occurred */
      return cs;
    if (ncl == scl)
      return 0;   /* No free cluster */
    }

  FRESULT res = putFat (fs, ncl, 0x0FFFFFFF); /* Mark the new cluster "last link" */
  if (res == FR_OK && clst != 0)
    res = putFat (fs, clst, ncl); /* Link it to the previous one if needed */

  if (res == FR_OK) {
    fs->last_clust = ncl;  /* Update FSINFO */
    if (fs->free_clust != 0xFFFFFFFF) {
      fs->free_clust--;
      fs->fsi_flag |= 1;
      }
    }
  else
    ncl = (res == FR_DISK_ERR) ? 0xFFFFFFFF : 1;

  return ncl;   /* Return new cluster number or error code */
  }
/*}}}*/
/*{{{*/
static FRESULT removeChain (FATFS* fs, DWORD clst) {

#if _USE_TRIM
  DWORD scl = clst, ecl = clst, rt[2];
#endif

  FRESULT res;
  if (clst < 2 || clst >= fs->n_fatent)
    /* Check range */
    res = FR_INT_ERR;
  else {
    res = FR_OK;
    while (clst < fs->n_fatent) {     /* Not a last link? */
      DWORD nxt = getFat (fs, clst); /* Get cluster status */
      if (nxt == 0)
        break;        /* Empty cluster? */

      if (nxt == 1) {
        /* Internal error? */
        res = FR_INT_ERR;
        break;
        }

      if (nxt == 0xFFFFFFFF) {
        /* Disk error? */
        res = FR_DISK_ERR;
        break;
        }

      res = putFat (fs, clst, 0);     /* Mark the cluster "empty" */
      if (res != FR_OK) break;
      if (fs->free_clust != 0xFFFFFFFF) { /* Update FSINFO */
        fs->free_clust++;
        fs->fsi_flag |= 1;
        }

#if _USE_TRIM
      if (ecl + 1 == nxt)
        /* Is next cluster contiguous? */
        ecl = nxt;
      else {        /* End of contiguous clusters */
        rt[0] = clust2sect(fs, scl);          /* Start sector */
        rt[1] = clust2sect(fs, ecl) + fs->csize - 1;  /* End sector */
        disk_ioctl(fs->drv, CTRL_TRIM, rt);       /* Erase the block */
        scl = ecl = nxt;
        }
#endif

      clst = nxt; /* Next cluster */
      }
    }

  return res;
  }
/*}}}*/
/*}}}*/
/*{{{  lfn*/
/*{{{*/
static int cmp_lfn (WCHAR* lfnbuf, BYTE* dir) {

  UINT i = ((dir[LDIR_Ord] & ~LLEF) - 1) * 13; /* Get offset in the LFN buffer */
  UINT s = 0;
  WCHAR wc = 1;
  do {
    WCHAR uc = LD_WORD(dir + LfnOfs[s]);  /* Pick an LFN character from the entry */
    if (wc) { /* Last character has not been processed */
      wc = ff_wtoupper (uc);   /* Convert it to upper case */
      if (i >= _MAX_LFN || wc != ff_wtoupper (lfnbuf[i++]))  /* Compare it */
        return 0;       /* Not matched */
      }
    else {
      if (uc != 0xFFFF)
        return 0; /* Check filler */
      }
    } while (++s < 13);       /* Repeat until all characters in the entry are checked */

  if ((dir[LDIR_Ord] & LLEF) && wc && lfnbuf[i])  /* Last segment matched but different length */
    return 0;

  return 1;           /* The part of LFN matched */
  }
/*}}}*/
/*{{{*/
static int pick_lfn (WCHAR* lfnbuf, BYTE* dir) {

  UINT i = ((dir[LDIR_Ord] & 0x3F) - 1) * 13;  /* Offset in the LFN buffer */
  UINT s = 0;
  WCHAR wc = 1;
  do {
    WCHAR uc = LD_WORD(dir + LfnOfs[s]);    /* Pick an LFN character from the entry */
    if (wc) { /* Last character has not been processed */
      if (i >= _MAX_LFN) return 0;  /* Buffer overflow? */
      lfnbuf[i++] = wc = uc;      /* Store it */
      }
    else {
      if (uc != 0xFFFF)
        return 0;   /* Check filler */
      }
    } while (++s < 13);           /* Read all character in the entry */

  if (dir[LDIR_Ord] & LLEF) {       /* Put terminator if it is the last LFN part */
    if (i >= _MAX_LFN)
      return 0;    /* Buffer overflow? */
    lfnbuf[i] = 0;
    }

  return 1;
  }
/*}}}*/
/*{{{*/
static void fit_lfn (const WCHAR* lfnbuf, BYTE* dir, BYTE ord, BYTE sum) {

  dir[LDIR_Chksum] = sum;   /* Set check sum */
  dir[LDIR_Attr] = AM_LFN;  /* Set attribute. LFN entry */
  dir[LDIR_Type] = 0;
  ST_WORD(dir + LDIR_FstClusLO, 0);

  UINT i = (ord - 1) * 13;  /* Get offset in the LFN buffer */
  UINT s = 0;
  WCHAR wc = 0;
  do {
    if (wc != 0xFFFF)
      wc = lfnbuf[i++];  /* Get an effective character */
    ST_WORD(dir+LfnOfs[s], wc);  /* Put it */
    if (!wc)
      wc = 0xFFFF;  /* Padding characters following last character */
    } while (++s < 13);

  if (wc == 0xFFFF || !lfnbuf[i])
    ord |= LLEF;  /* Bottom LFN part is the start of LFN sequence */

  dir[LDIR_Ord] = ord;      /* Set the LFN order */
  }
/*}}}*/
/*{{{*/
static BYTE sum_sfn (const BYTE* dir) {

  BYTE sum = 0;
  UINT n = 11;
  do sum = (sum >> 1) + (sum << 7) + *dir++;
    while (--n);

  return sum;
  }
/*}}}*/
/*{{{*/
static void gen_numname (BYTE* dst, const BYTE* src, const WCHAR* lfn, UINT seq) {

  BYTE ns[8], c;
  UINT i, j;
  WCHAR wc;
  DWORD sr;

  memcpy (dst, src, 11);

  if (seq > 5) {
    /* On many collisions, generate a hash number instead of sequential number */
    sr = seq;
    while (*lfn) {  /* Create a CRC */
      wc = *lfn++;
      for (i = 0; i < 16; i++) {
        sr = (sr << 1) + (wc & 1);
        wc >>= 1;
        if (sr & 0x10000) sr ^= 0x11021;
        }
      }
    seq = (UINT)sr;
    }

  /* itoa (hexdecimal) */
  i = 7;
  do {
    c = (seq % 16) + '0';
    if (c > '9') c += 7;
    ns[i--] = c;
    seq /= 16;
    } while (seq);
  ns[i] = '~';

  /* Append the number */
  for (j = 0; j < i && dst[j] != ' '; j++) {
    if (IsDBCS1(dst[j])) {
      if (j == i - 1) break;
      j++;
      }
     }

   do {
    dst[j++] = (i < 8) ? ns[i++] : ' ';
     } while (j < 8);
 }
/*}}}*/
/*}}}*/
/*{{{  dir*/
/*{{{*/
static FRESULT dir_sdi (DIR* dp, UINT idx) {

  dp->index = (WORD)idx;  /* Current index */

  DWORD clst = dp->sclust;    /* Table start cluster (0:root) */
  if (clst == 1 || clst >= dp->fs->n_fatent)  /* Check start cluster range */
    return FR_INT_ERR;
  if (!clst && dp->fs->fs_type == FS_FAT32) /* Replace cluster# 0 with root cluster# if in FAT32 */
    clst = dp->fs->dirbase;

  DWORD sect;
  if (clst == 0) {
    /* Static table (root-directory in FAT12/16) */
    if (idx >= dp->fs->n_rootdir) /* Is index out of range? */
      return FR_INT_ERR;
    sect = dp->fs->dirbase;
    }
  else {
    /* Dynamic table (root-directory in FAT32 or sub-directory) */
    UINT ic = _MAX_SS / SZ_DIRE * dp->fs->csize;  /* Entries per cluster */
    while (idx >= ic) { /* Follow cluster chain */
      clst = getFat (dp->fs, clst);       /* Get next cluster */
      if (clst == 0xFFFFFFFF)
        return FR_DISK_ERR; /* Disk error */
      if (clst < 2 || clst >= dp->fs->n_fatent) /* Reached to end of table or internal error */
        return FR_INT_ERR;
      idx -= ic;
      }
    sect = clusterToSector(dp->fs, clst);
    }

  dp->clust = clst; /* Current cluster# */
  if (!sect)
    return FR_INT_ERR;

  dp->sect = sect + idx / (_MAX_SS / SZ_DIRE);                       /* Sector# of the directory entry */
  dp->dir = dp->fs->win.d8 + (idx % (_MAX_SS / SZ_DIRE)) * SZ_DIRE;  /* Ptr to the entry in the sector */

  return FR_OK;
  }
/*}}}*/
/*{{{*/
static FRESULT dir_next (DIR* dp, int stretch) {

  UINT i = dp->index + 1;
  if (!(i & 0xFFFF) || !dp->sect) /* Report EOT when index has reached 65535 */
    return FR_NO_FILE;

  if (!(i % (_MAX_SS / SZ_DIRE))) {
    dp->sect++;
    if (!dp->clust) {
      /* Static table */
      if (i >= dp->fs->n_rootdir) /* Report EOT if it reached end of static table */
        return FR_NO_FILE;
      }
    else {
      /* Dynamic table */
      if (((i / (_MAX_SS / SZ_DIRE)) & (dp->fs->csize - 1)) == 0) {
        /* Cluster changed? */
        DWORD clst = getFat (dp->fs, dp->clust);        /* Get next cluster */
        if (clst <= 1)
          return FR_INT_ERR;
        if (clst == 0xFFFFFFFF)
          return FR_DISK_ERR;
        if (clst >= dp->fs->n_fatent) {
          /* If it reached end of dynamic table, */
          if (!stretch)
            return FR_NO_FILE;      /* If do not stretch, report EOT */
          clst = createChain (dp->fs, dp->clust);   /* Stretch cluster chain */
          if (clst == 0)
            return FR_DENIED;      /* No free cluster */
          if (clst == 1)
            return FR_INT_ERR;
          if (clst == 0xFFFFFFFF)
            return FR_DISK_ERR;

          /* Clean-up stretched table */
          if (syncWindow(dp->fs))
            return FR_DISK_ERR;/* Flush disk access window */

          /* Clear window buffer */
          memset (dp->fs->win.d8, 0, _MAX_SS);

          /* Cluster start sector */
          dp->fs->winsect = clusterToSector (dp->fs, clst);

          UINT c;
          for (c = 0; c < dp->fs->csize; c++) {
            /* Fill the new cluster with 0 */
            dp->fs->wflag = 1;
            if (syncWindow (dp->fs))
              return FR_DISK_ERR;
            dp->fs->winsect++;
            }
          dp->fs->winsect -= c;           /* Rewind window offset */
          }

        /* Initialize data for new cluster */
        dp->clust = clst;
        dp->sect = clusterToSector (dp->fs, clst);
        }
      }
    }

  /* Current index */
  dp->index = (WORD)i;

  /* Current entry in the window */
  dp->dir = dp->fs->win.d8 + (i % (_MAX_SS / SZ_DIRE)) * SZ_DIRE;

  return FR_OK;
  }
/*}}}*/
/*{{{*/
static FRESULT dir_alloc (DIR* dp, UINT nent) {

  UINT n;
  FRESULT res = dir_sdi (dp, 0);
  if (res == FR_OK) {
    n = 0;
    do {
      res = moveWindow (dp->fs, dp->sect);
      if (res != FR_OK)
        break;
      if (dp->dir[0] == DDEM || dp->dir[0] == 0) {  /* Is it a free entry? */
        if (++n == nent) 
          break; /* A block of contiguous free entries is found */
        }
      else {
        n = 0;          /* Not a blank entry. Restart to search */
        }
      res = dir_next(dp, 1);    /* Next entry with table stretch enabled */
      } while (res == FR_OK);
    }

  if (res == FR_NO_FILE)
    res = FR_DENIED; /* No directory entry to allocate */

  return res;
  }
/*}}}*/
/*{{{*/
static FRESULT dir_find (DIR* dp) {

  BYTE c, *dir;
  BYTE a, sum;

  FRESULT res = dir_sdi (dp, 0);     /* Rewind directory object */
  if (res != FR_OK)
    return res;

  BYTE ord = sum = 0xFF;
  dp->lfn_idx = 0xFFFF; /* Reset LFN sequence */
  do {
    res = moveWindow (dp->fs, dp->sect);
    if (res != FR_OK)
      break;

    dir = dp->dir;          /* Ptr to the directory entry of current index */
    c = dir[DIR_Name];
    if (c == 0) {
      res = FR_NO_FILE;
      break;
      }  /* Reached to end of table */

    a = dir[DIR_Attr] & AM_MASK;
    if (c == DDEM || ((a & AM_VOL) && a != AM_LFN)) { /* An entry without valid data */
      ord = 0xFF;
      dp->lfn_idx = 0xFFFF; /* Reset LFN sequence */
      }
    else {
      if (a == AM_LFN) {
        /* An LFN entry is found */
        if (dp->lfn) {
          if (c & LLEF) {
            /* Is it start of LFN sequence? */
            sum = dir[LDIR_Chksum];
            c &= ~LLEF;
            ord = c;  /* LFN start order */
            dp->lfn_idx = dp->index;  /* Start index of LFN */
            }
          /* Check validity of the LFN entry and compare it with given name */
          ord = (c == ord && sum == dir[LDIR_Chksum] && cmp_lfn(dp->lfn, dir)) ? ord - 1 : 0xFF;
          }
        }
      else { 
        /* An SFN entry is found */
        if (!ord && sum == sum_sfn(dir))
          break; /* LFN matched? */
        if (!(dp->fn[NSFLAG] & NS_LOSS) && !mem_cmp(dir, dp->fn, 11))
          break;  /* SFN matched? */
        ord = 0xFF;
        dp->lfn_idx = 0xFFFF; /* Reset LFN sequence */
        }
      }

    res = dir_next(dp, 0);    /* Next entry */
    } while (res == FR_OK);

  return res;
  }
/*}}}*/
/*{{{*/
static FRESULT dir_read (DIR* dp, int vol) {

  BYTE a, c, *dir;
  BYTE ord = 0xFF, sum = 0xFF;

  FRESULT res = FR_NO_FILE;
  while (dp->sect) {
    res = moveWindow(dp->fs, dp->sect);
    if (res != FR_OK) break;
    dir = dp->dir;          /* Ptr to the directory entry of current index */
    c = dir[DIR_Name];
    if (c == 0) {
      res = FR_NO_FILE;
      break;
      }  /* Reached to end of table */

    a = dir[DIR_Attr] & AM_MASK;
    if (c == DDEM || (int)((a & ~AM_ARC) == AM_VOL) != vol) { 
      /* An entry without valid data */
      ord = 0xFF;
      }
    else {
      if (a == AM_LFN) {
        /* An LFN entry is found */
        if (c & LLEF) { 
          /* Is it start of LFN sequence? */
          sum = dir[LDIR_Chksum];
          c &= ~LLEF; ord = c;
          dp->lfn_idx = dp->index;
          }

        /* Check LFN validity and capture it */
        ord = (c == ord && sum == dir[LDIR_Chksum] && pick_lfn(dp->lfn, dir)) ? ord - 1 : 0xFF;
        }

      else { 
        /* An SFN entry is found */
        if (ord || sum != sum_sfn(dir)) /* Is there a valid LFN? */
          dp->lfn_idx = 0xFFFF;   /* It has no LFN. */
        break;
        }
      }

    res = dir_next(dp, 0);  
    if (res != FR_OK)
      break;
    }

  if (res != FR_OK)
    dp->sect = 0;

  return res;
  }
/*}}}*/
/*{{{*/
static FRESULT dir_register (DIR* dp) {

  UINT n, nent;

  BYTE* fn = dp->fn;
  WCHAR* lfn = dp->lfn;

  BYTE sn[12];
  memcpy (sn, fn, 12);

  if (sn[NSFLAG] & NS_DOT)   /* Cannot create dot entry */
    return FR_INVALID_NAME;

  if (sn[NSFLAG] & NS_LOSS) {     /* When LFN is out of 8.3 format, generate a numbered name */
  FRESULT res;
  fn[NSFLAG] = 0;
    dp->lfn = 0;      /* Find only SFN */
    for (n = 1; n < 100; n++) {
      gen_numname (fn, sn, lfn, n);  /* Generate a numbered name */
      res = dir_find (dp);       /* Check if the name collides with existing SFN */
      if (res != FR_OK)
        break;
      }
    if (n == 100)
      return FR_DENIED;   /* Abort if too many collisions */
    if (res != FR_NO_FILE)
      return res;  /* Abort if the result is other than 'not collided' */

    fn[NSFLAG] = sn[NSFLAG];
    dp->lfn = lfn;
    }

  if (sn[NSFLAG] & NS_LFN) {      /* When LFN is to be created, allocate entries for an SFN + LFNs. */
    for (n = 0; lfn[n]; n++) ;
    nent = (n + 25) / 13;
    }
  else
    /* Otherwise allocate an entry for an SFN  */
    nent = 1;

  FRESULT res = dir_alloc (dp, nent);    /* Allocate entries */
  if (res == FR_OK && --nent) { 
    /* Set LFN entry if needed */
    res = dir_sdi (dp, dp->index - nent);
    if (res == FR_OK) {
      BYTE sum = sum_sfn (dp->fn);  /* Sum value of the SFN tied to the LFN */
      do {
        /* Store LFN entries in bottom first */
        res = moveWindow (dp->fs, dp->sect);
        if (res != FR_OK)
          break;
        fit_lfn (dp->lfn, dp->dir, (BYTE)nent, sum);
        dp->fs->wflag = 1;
        res = dir_next (dp, 0);  /* Next entry */
        } while (res == FR_OK && --nent);
      }
    }

  if (res == FR_OK) {
    /* Set SFN entry */
    res = moveWindow (dp->fs, dp->sect);
    if (res == FR_OK) {
      memset (dp->dir, 0, SZ_DIRE); /* Clean the entry */
      memcpy (dp->dir, dp->fn, 11); /* Put SFN */
      dp->dir[DIR_NTres] = dp->fn[NSFLAG] & (NS_BODY | NS_EXT); /* Put NT flag */
      dp->fs->wflag = 1;
      }
    }

  return res;
  }
/*}}}*/
/*{{{*/
static FRESULT dir_remove (DIR* dp) {

  UINT i = dp->index;  /* SFN index */

  FRESULT res = dir_sdi (dp, (dp->lfn_idx == 0xFFFF) ? i : dp->lfn_idx); /* Goto the SFN or top of the LFN entries */
  if (res == FR_OK) {
    do {
      res = moveWindow (dp->fs, dp->sect);
      if (res != FR_OK) break;
      memset (dp->dir, 0, SZ_DIRE); /* Clear and mark the entry "deleted" */
      *dp->dir = DDEM;
      dp->fs->wflag = 1;
      if (dp->index >= i)  
        break;  /* When reached SFN, all entries of the object has been deleted. */
      res = dir_next (dp, 0);    
      } while (res == FR_OK);

    if (res == FR_NO_FILE)
      res = FR_INT_ERR;
    }

  return res;
  }
/*}}}*/
/*}}}*/

/*{{{*/
static FRESULT validate (void* obj) {

  /* Assuming offset of .fs and .id in the FIL/DIR structure is identical */
  FIL* fil = (FIL*)obj;

  if (!fil ||
      !fil->fs ||
      !fil->fs->fs_type ||
      fil->fs->id != fil->id ||
      (disk_status(fil->fs->drv) & STA_NOINIT))
    return FR_INVALID_OBJECT;

  // Lock file system
  if (!ff_req_grant (fil->fs->sobj))
    return FR_TIMEOUT;

  return FR_OK;
  }
/*}}}*/
/*{{{*/
static int get_ldnumber (const TCHAR** path) {

  const TCHAR *tp, *tt;
  UINT i;
  int vol = -1;

#if _STR_VOLUME_ID    /* Find string drive id */
  static const char* const str[] = {_VOLUME_STRS};
  const char *sp;
  char c;
  TCHAR tc;
#endif

  if (*path) {  /* If the pointer is not a null */
    for (tt = *path; (UINT)*tt >= (' ') && *tt != ':'; tt++) ; /* Find ':' in the path */
    if (*tt == ':') { /* If a ':' is exist in the path name */
      tp = *path;
      i = *tp++ - '0';
      if (i < 10 && tp == tt) { /* Is there a numeric drive id? */
        if (i < 1) { /* If a drive id is found, get the value and strip it */
          vol = (int)i;
          *path = ++tt;
          }
        }

#if _STR_VOLUME_ID
       else { /* No numeric drive number, find string drive id */
        i = 0; tt++;
        do {
          sp = str[i]; tp = *path;
          do {  /* Compare a string drive id with path name */
            c = *sp++; tc = *tp++;
            if (IsLower(tc)) tc -= 0x20;
            } while (c && (TCHAR)c == tc);
          } while ((c || tp != tt) && ++i < 1);  /* Repeat for each id until pattern match */
        if (i < 1) { /* If a drive id is found, get the value and strip it */
          vol = (int)i;
          *path = tt;
          }
        }
#endif

      return vol;
      }

    vol = 0;    /* Drive 0 */
    }

  return vol;
  }
/*}}}*/
/*{{{*/
static FRESULT find_volume (FATFS** rfs, const TCHAR** path, BYTE wmode) {

  BYTE fmt, *pt;
  int vol;
  DSTATUS stat;
  DWORD bsect, fasize, tsect, sysect, nclst, szbfat, br[4];
  WORD nrsv;
  FATFS *fs;
  UINT i;

  /* Get logical drive number from the path name */
  *rfs = 0;
  vol = get_ldnumber(path);
  if (vol < 0)
    return FR_INVALID_DRIVE;

  /* Check if the file system object is valid or not */
  fs = FatFs[vol];  /* Get pointer to the file system object */
  if (!fs)
    return FR_NOT_ENABLED;   /* Is the file system object available? */

  /* Lock the volume */
  if (!ff_req_grant (fs->sobj))
    return FR_TIMEOUT;
  *rfs = fs;   /* Return pointer to the file system object */

  if (fs->fs_type) {
    /* If the volume has been mounted */
    stat = disk_status (fs->drv);
    if (!(stat & STA_NOINIT)) {   /* and the physical drive is kept initialized */
      if (wmode && (stat & STA_PROTECT)) /* Check write protection if needed */
        return FR_WRITE_PROTECTED;
      return FR_OK;       /* The file system object is valid */
      }
    }

  /* The file system object is not valid. */
  /* Following code attempts to mount the volume. (analyze BPB and initialize the fs object) */
  fs->fs_type = 0;          /* Clear the file system object */
  fs->drv = vol;       /* Bind the logical drive and a physical drive */
  stat = disk_initialize (fs->drv);  /* Initialize the physical drive */
  if (stat & STA_NOINIT)        /* Check if the initialization succeeded */
    return FR_NOT_READY;      /* Failed to initialize due to no medium or hard error */
  if (wmode && (stat & STA_PROTECT)) /* Check disk write protection if needed */
    return FR_WRITE_PROTECTED;

  /* Find an FAT partition on the drive. Supports only generic partitioning, FDISK and SFD. */
  bsect = 0;
  fmt = checkFs (fs, bsect);          /* Load sector 0 and check if it is an FAT boot sector as SFD */
  if (fmt == 1 || (!fmt && vol)) {
    /* Not an FAT boot sector or forced partition number */
    for (i = 0; i < 4; i++) {
      /* Get partition offset */
      pt = fs->win.d8 + MBR_Table + i * SZ_PTE;
      br[i] = pt[4] ? LD_DWORD(&pt[8]) : 0;
      }

    i = vol;  /* Partition number: 0:auto, 1-4:forced */
    if (i)
      i--;
    do {
      /* Find an FAT volume */
      bsect = br[i];
      fmt = bsect ? checkFs (fs, bsect) : 2;  /* Check the partition */
      } while (!vol && fmt && ++i < 4);
    }

  if (fmt == 3)
    return FR_DISK_ERR;   /* An error occured in the disk I/O layer */
  if (fmt)
    return FR_NO_FILESYSTEM;   /* No FAT volume is found */

  /* An FAT volume is found. Following code initializes the file system object */
  if (LD_WORD(fs->win.d8 + BPB_BytsPerSec) != _MAX_SS) /* (BPB_BytsPerSec must be equal to the physical sector size) */
    return FR_NO_FILESYSTEM;

  fasize = LD_WORD(fs->win.d8 + BPB_FATSz16);     /* Number of sectors per FAT */
  if (!fasize)
    fasize = LD_DWORD(fs->win.d8 + BPB_FATSz32);
  fs->fsize = fasize;

  fs->n_fats = fs->win.d8[BPB_NumFATs];         /* Number of FAT copies */
  if (fs->n_fats != 1 && fs->n_fats != 2)       /* (Must be 1 or 2) */
    return FR_NO_FILESYSTEM;
  fasize *= fs->n_fats;               /* Number of sectors for FAT area */

  fs->csize = fs->win.d8[BPB_SecPerClus];       /* Number of sectors per cluster */
  if (!fs->csize || (fs->csize & (fs->csize - 1)))  /* (Must be power of 2) */
    return FR_NO_FILESYSTEM;

  fs->n_rootdir = LD_WORD(fs->win.d8 + BPB_RootEntCnt); /* Number of root directory entries */
  if (fs->n_rootdir % (_MAX_SS / SZ_DIRE))       /* (Must be sector aligned) */
    return FR_NO_FILESYSTEM;

  tsect = LD_WORD(fs->win.d8 + BPB_TotSec16);     /* Number of sectors on the volume */
  if (!tsect)
    tsect = LD_DWORD(fs->win.d8 + BPB_TotSec32);

  nrsv = LD_WORD(fs->win.d8 + BPB_RsvdSecCnt);      /* Number of reserved sectors */
  if (!nrsv)
    return FR_NO_FILESYSTEM;         /* (Must not be 0) */

  /* Determine the FAT sub type */
  sysect = nrsv + fasize + fs->n_rootdir / (_MAX_SS / SZ_DIRE);  /* RSV + FAT + DIR */
  if (tsect < sysect)
    return FR_NO_FILESYSTEM;    /* (Invalid volume size) */

  nclst = (tsect - sysect) / fs->csize;       /* Number of clusters */
  if (!nclst)
    return FR_NO_FILESYSTEM;        /* (Invalid volume size) */
  fmt = FS_FAT12;
  if (nclst >= MIN_FAT16)
    fmt = FS_FAT16;
  if (nclst >= MIN_FAT32)
    fmt = FS_FAT32;

  /* Boundaries and Limits */
  fs->n_fatent = nclst + 2;             /* Number of FAT entries */
  fs->volbase = bsect;                /* Volume start sector */
  fs->fatbase = bsect + nrsv;             /* FAT start sector */
  fs->database = bsect + sysect;            /* Data start sector */
  if (fmt == FS_FAT32) {
    if (fs->n_rootdir)
      return FR_NO_FILESYSTEM;   /* (BPB_RootEntCnt must be 0) */
    fs->dirbase = LD_DWORD(fs->win.d8 + BPB_RootClus);  /* Root directory start cluster */
    szbfat = fs->n_fatent * 4;            /* (Needed FAT size) */
    }
  else {
    if (!fs->n_rootdir)
      return FR_NO_FILESYSTEM;  /* (BPB_RootEntCnt must not be 0) */
    fs->dirbase = fs->fatbase + fasize;       /* Root directory start sector */
    szbfat = (fmt == FS_FAT16) ? fs->n_fatent * 2 : fs->n_fatent * 3 / 2 + (fs->n_fatent & 1);  /* (Needed FAT size) */
    }
  if (fs->fsize < (szbfat + (_MAX_SS - 1)) / _MAX_SS) /* (BPB_FATSz must not be less than the size needed) */
    return FR_NO_FILESYSTEM;

  /* Initialize cluster allocation information */
  fs->last_clust = fs->free_clust = 0xFFFFFFFF;

  /* Get fsinfo if available */
  fs->fsi_flag = 0x80;
  if (fmt == FS_FAT32 && LD_WORD(fs->win.d8 + BPB_FSInfo) == 1 && moveWindow(fs, bsect + 1) == FR_OK) {
    fs->fsi_flag = 0;
    if (LD_WORD(fs->win.d8 + BS_55AA) == 0xAA55 &&
        LD_DWORD(fs->win.d8 + FSI_LeadSig) == 0x41615252 &&
        LD_DWORD(fs->win.d8 + FSI_StrucSig) == 0x61417272) {
      fs->free_clust = LD_DWORD(fs->win.d8 + FSI_Free_Count);
      fs->last_clust = LD_DWORD(fs->win.d8 + FSI_Nxt_Free);
      }
    }

  fs->fs_type = fmt;  /* FAT sub-type */
  fs->id = ++Fsid;    /* File system mount ID */
  fs->cdir = 0;       /* Set current directory to root */
  clear_lock (fs);

  return FR_OK;
  }
/*}}}*/
/*{{{*/
static FRESULT create_name (DIR* dp, const TCHAR** path ) {

  BYTE b, cf;
  WCHAR w, *lfn;
  UINT i, ni, si, di;
  const TCHAR *p;

  /* Create LFN in Unicode */
  for (p = *path; *p == '/' || *p == '\\'; p++) ; /* Strip duplicated separator */
  lfn = dp->lfn;
  si = di = 0;
  for (;;) {
    w = p[si++];          /* Get a character */
    if (w < ' ' || w == '/' || w == '\\')
      break;  /* Break on end of segment */
    if (di >= _MAX_LFN)       /* Reject too long name */
      return FR_INVALID_NAME;

#if !_LFN_UNICODE
    w &= 0xFF;
    if (IsDBCS1(w)) {       /* Check if it is a DBC 1st byte (always false on SBCS cfg) */
  #if _DF1S
      b = (BYTE)p[si++];      /* Get 2nd byte */
      w = (w << 8) + b;     /* Create a DBC */
      if (!IsDBCS2(b))
        return FR_INVALID_NAME; /* Reject invalid sequence */
  #endif
      }

    w = ff_convert(w, 1);     /* Convert ANSI/OEM to Unicode */
    if (!w)
      return FR_INVALID_NAME; /* Reject invalid code */
#endif

    if (w < 0x80 && chk_chr("\"*:<>\?|\x7F", w)) /* Reject illegal characters for LFN */
      return FR_INVALID_NAME;
    lfn[di++] = w;          /* Store the Unicode character */
    }

  *path = &p[si];           /* Return pointer to the next segment */
  cf = (w < ' ') ? NS_LAST : 0;   /* Set last segment flag if end of path */
  if ((di == 1 && lfn[di - 1] == '.') || (di == 2 && lfn[di - 1] == '.' && lfn[di - 2] == '.')) {
    lfn[di] = 0;
    for (i = 0; i < 11; i++)
      dp->fn[i] = (i < di) ? '.' : ' ';
    dp->fn[i] = cf | NS_DOT;    /* This is a dot entry */
    return FR_OK;
    }

  while (di) {
    /* Strip trailing spaces and dots */
    w = lfn[di - 1];
    if (w != ' ' && w != '.')
      break;
    di--;
    }
  if (!di)
    return FR_INVALID_NAME;  /* Reject nul string */

  lfn[di] = 0;            /* LFN is created */

  /* Create SFN in directory form */
  memset(dp->fn, ' ', 11);
  for (si = 0; lfn[si] == ' ' || lfn[si] == '.'; si++) ;  /* Strip leading spaces and dots */
  if (si)
    cf |= NS_LOSS | NS_LFN;
  while (di && lfn[di - 1] != '.')
    di--;  /* Find extension (di<=si: no extension) */

  b = i = 0; ni = 8;
  for (;;) {
    w = lfn[si++];          /* Get an LFN character */
    if (!w) /* Break on end of the LFN */
      break;

    if (w == ' ' || (w == '.' && si != di)) {
      /* Remove spaces and dots */
      cf |= NS_LOSS | NS_LFN;
      continue;
      }

    if (i >= ni || si == di) {    /* Extension or end of SFN */
      if (ni == 11) {
        /* Long extension */
        cf |= NS_LOSS | NS_LFN;
        break;
        }
      if (si != di)
        cf |= NS_LOSS | NS_LFN; /* Out of 8.3 format */
      if (si > di)
        break;     /* No extension */

      si = di;
      i = 8;
      ni = 11;  /* Enter extension section */
      b <<= 2;
      continue;
      }

    if (w >= 0x80) {        /* Non ASCII character */
#ifdef _EXCVT
      w = ff_convert(w, 0);   /* Unicode -> OEM code */
      if (w)
        w = ExCvt[w - 0x80]; /* Convert extended character to upper (SBCS) */
#else
      w = ff_convert(ff_wtoupper(w), 0);  /* Upper converted Unicode -> OEM code */
#endif
      cf |= NS_LFN;       /* Force create LFN entry */
      }

    if (_DF1S && w >= 0x100) {
      /* DBC (always false at SBCS cfg) */
      if (i >= ni - 1) {
        cf |= NS_LOSS | NS_LFN; i = ni; continue;
        }
      dp->fn[i++] = (BYTE)(w >> 8);
      }

    else {
      /* SBC */
      if (!w || chk_chr("+,;=[]", w)) { /* Replace illegal characters for SFN */
        w = '_';
        cf |= NS_LOSS | NS_LFN; /* Lossy conversion */
        }
      else {
        if (IsUpper(w)) {   /* ASCII large capital */
          b |= 2;
          }
        else {
          if (IsLower(w)) { /* ASCII small capital */
            b |= 1;
            w -= 0x20;
            }
          }
        }
      }
    dp->fn[i++] = (BYTE)w;
    }

  if (dp->fn[0] == DDEM)
    dp->fn[0] = RDDEM; /* If the first character collides with deleted mark, replace it with RDDEM */

  if (ni == 8) b <<= 2;
  if ((b & 0x0C) == 0x0C || (b & 0x03) == 0x03) /* Create LFN entry when there are composite capitals */
    cf |= NS_LFN;
  if (!(cf & NS_LFN)) {           /* When LFN is in 8.3 format without extended character, NT flags are created */
    if ((b & 0x03) == 0x01)
      cf |= NS_EXT; /* NT flag (Extension has only small capital) */
    if ((b & 0x0C) == 0x04)
      cf |= NS_BODY;  /* NT flag (Filename has only small capital) */
    }

  dp->fn[NSFLAG] = cf;  /* SFN is created */

  return FR_OK;
  }
/*}}}*/
/*{{{*/
static FRESULT follow_path (DIR* dp, const TCHAR* path) {

  FRESULT res;
  BYTE *dir, ns;

  if (*path == '/' || *path == '\\') {
    //  There is a heading separator
    path++;
    dp->sclust = 0;       /* Strip it and start from the root directory */
    }
  else
    // No heading separator
    dp->sclust = dp->fs->cdir;      /* Start from the current directory */

  if ((UINT)*path < ' ') {
    // Null path name is the origin directory itself
    res = dir_sdi (dp, 0);
    dp->dir = 0;
    }

  else {
    // Follow path
    for (;;) {
      res = create_name (dp, &path); /* Get a segment name of the path */
      if (res != FR_OK) break;
      res = dir_find (dp);       /* Find an object with the sagment name */
      ns = dp->fn[NSFLAG];
      if (res != FR_OK) {       /* Failed to find the object */
        if (res == FR_NO_FILE) {  /* Object is not found */
          if (ns & NS_DOT) { /* If dot entry is not exist, */
            dp->sclust = 0; dp->dir = 0;  /* it is the root directory and stay there */
            if (!(ns & NS_LAST)) continue;  /* Continue to follow if not last segment */
            res = FR_OK;          /* Ended at the root directroy. Function completed. */
            }
          else if (!(ns & NS_LAST))
            res = FR_NO_PATH;  /* Adjust error code if not last segment */
          }
        break;
        }

      if (ns & NS_LAST)
        break;      /* Last segment matched. Function completed. */
      dir = dp->dir;            /* Follow the sub-directory */
      if (!(dir[DIR_Attr] & AM_DIR))
        /* It is not a sub-directory and cannot follow */
        res = FR_NO_PATH; break;
      dp->sclust = loadCluster (dp->fs, dir);
      }
    }

  return res;
  }
/*}}}*/
/*{{{*/
static void getFileInfo (DIR* dp, FILINFO* fileInfo) {

  UINT i;
  TCHAR *p, c;
  BYTE *dir;
  WCHAR w, *lfn;

  p = fileInfo->fname;
  if (dp->sect) {
    /* Get SFN */
    dir = dp->dir;
    i = 0;
    while (i < 11) {
      /* Copy name body and extension */
      c = (TCHAR)dir[i++];
      if (c == ' ')
        continue;       /* Skip padding spaces */
      if (c == RDDEM)
        c = (TCHAR)DDEM;  /* Restore replaced DDEM character */
      if (i == 9)
        *p++ = '.';       /* Insert a . if extension is exist */
      if (IsUpper(c) && (dir[DIR_NTres] & (i >= 9 ? NS_EXT : NS_BODY)))
        c += 0x20;  /* To lower */
#if _LFN_UNICODE
      if (IsDBCS1(c) && i != 8 && i != 11 && IsDBCS2(dir[i]))
        c = c << 8 | dir[i++];
      c = ff_convert(c, 1); /* OEM -> Unicode */
      if (!c) c = '?';
#endif
      *p++ = c;
      }
    fileInfo->fattrib = dir[DIR_Attr];       /* Attribute */
    fileInfo->fsize = LD_DWORD(dir + DIR_FileSize);  /* Size */
    fileInfo->fdate = LD_WORD(dir + DIR_WrtDate);  /* Date */
    fileInfo->ftime = LD_WORD(dir + DIR_WrtTime);  /* Time */
    }

  /* Terminate SFN string by a \0 */
  *p = 0;

  if (fileInfo->lfname) {
    i = 0;
    p = fileInfo->lfname;
    if (dp->sect && fileInfo->lfsize && dp->lfn_idx != 0xFFFF) {
      /* Get LFN if available */
      lfn = dp->lfn;
      while ((w = *lfn++) != 0) {
      /* Get an LFN character */

#if !_LFN_UNICODE
        w = ff_convert (w, 0);   /* Unicode -> OEM */
        if (!w) {
          /* No LFN if it could not be converted */
          i = 0;
          break;
          }
        if (_DF1S && w >= 0x100)  /* Put 1st byte if it is a DBC (always false on SBCS cfg) */
          p[i++] = (TCHAR)(w >> 8);
#endif

        if (i >= fileInfo->lfsize - 1) {
          /* No LFN if buffer overflow */
          i = 0;
          break;
          }
        p[i++] = (TCHAR)w;
        }
      }

    /* Terminate LFN string by a \0 */
    p[i] = 0;
    }
  }
/*}}}*/

/*{{{*/
FRESULT f_mount (FATFS* fs, const TCHAR* path, BYTE opt) {

  const TCHAR* rp = path;

  int vol = get_ldnumber (&rp);
  if (vol < 0)
    return FR_INVALID_DRIVE;

  FATFS* cfs = FatFs[vol];         /* Pointer to fs object */
  if (cfs) {
    clear_lock(cfs);
    if (!ff_del_syncobj (cfs->sobj))
      return FR_INT_ERR;
    cfs->fs_type = 0;       /* Clear old fs object */
    }

  if (fs) {
    fs->fs_type = 0;        /* Clear new fs object */
    if (!ff_cre_syncobj ((BYTE)vol, &fs->sobj))
      return FR_INT_ERR;
    }

  /* Register new fs object */
  FatFs[vol] = fs;
  if (!fs || opt != 1)
    return FR_OK;  /* Do not mount now, it will be mounted later */

  FRESULT res = find_volume (&fs, &path, 0); /* Force mounted the volume */
  LEAVE_FF(fs, res);
  }
/*}}}*/

/*{{{*/
FRESULT f_open (FIL* file, const TCHAR* path, BYTE mode) {

  DIR dj;
  BYTE* dir;
  DWORD dw, cl;

  if (!file)
    return FR_INVALID_OBJECT;

  /* Clear file object */
  file->fs = 0;

  /* Get logical drive number */
  mode &= FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW;
  FRESULT res = find_volume (&dj.fs, &path, (BYTE)(mode & ~FA_READ));
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = follow_path (&dj, path); /* Follow the file path */
    dir = dj.dir;
    if (res == FR_OK) {
      if (!dir)
        /* Default directory itself */
        res = FR_INVALID_NAME;
      else
        res = chk_lock (&dj, (mode & ~FA_READ) ? 1 : 0);
      }

    /* Create or Open a file */
    if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
      if (res != FR_OK) {
        /* No file, create new */
        if (res == FR_NO_FILE)
          /* There is no file to open, create a new entry */
          res = enq_lock() ? dir_register(&dj) : FR_TOO_MANY_OPEN_FILES;
        mode |= FA_CREATE_ALWAYS;   /* File is created */
        dir = dj.dir;         /* New entry */
        }
      else {
        /* Any object is already existing */
        if (dir[DIR_Attr] & (AM_RDO | AM_DIR))
          /* Cannot overwrite it (R/O or DIR) */
          res = FR_DENIED;
        else if (mode & FA_CREATE_NEW)
          /* Cannot create as new file */
          res = FR_EXIST;
        }

      if (res == FR_OK && (mode & FA_CREATE_ALWAYS)) {
        /* Truncate it if overwrite mode */
        dw = GET_FATTIME();       /* Created time */
        ST_DWORD(dir + DIR_CrtTime, dw);
        dir[DIR_Attr] = 0;        /* Reset attribute */
        ST_DWORD(dir + DIR_FileSize, 0);/* size = 0 */
        cl = loadCluster (dj.fs, dir);    /* Get start cluster */
        storeCluster (dir, 0);       /* cluster = 0 */
        dj.fs->wflag = 1;
        if (cl) {
          /* Remove the cluster chain if exist */
          dw = dj.fs->winsect;
          res = removeChain (dj.fs, cl);
          if (res == FR_OK) {
            dj.fs->last_clust = cl - 1;
            /* Reuse the cluster hole */
            res = moveWindow (dj.fs, dw);
            }
          }
        }
      }

    else {
      /* Open an existing file */
      if (res == FR_OK) {
        /* Follow succeeded */
        if (dir[DIR_Attr] & AM_DIR)
          /* It is a directory */
          res = FR_NO_FILE;
        else if ((mode & FA_WRITE) && (dir[DIR_Attr] & AM_RDO))
          /* R/O violation */
          res = FR_DENIED;
        }
      }

    if (res == FR_OK) {
      if (mode & FA_CREATE_ALWAYS)
        /* Set file change flag if created or overwritten */
        mode |= FA__WRITTEN;
      file->dir_sect = dj.fs->winsect;  /* Pointer to the directory entry */
      file->dir_ptr = dir;
      file->lockid = inc_lock (&dj, (mode & ~FA_READ) ? 1 : 0);
      if (!file->lockid)
        res = FR_INT_ERR;
      }

    if (res == FR_OK) {
      file->flag = mode;                          /* File access mode */
      file->err = 0;                              /* Clear error flag */
      file->sclust = loadCluster (dj.fs, dir);    /* File start cluster */
      file->fsize = LD_DWORD(dir + DIR_FileSize); /* File size */
      file->fptr = 0;                             /* File pointer */
      file->dsect = 0;
      file->cltbl = 0;                              /* Normal seek mode */
      file->fs = dj.fs;                             /* Validate file object */
      file->id = file->fs->id;
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_read (FIL* file, void* buff, UINT btr, UINT* br) {

  DWORD clst, sect;
  UINT rcnt, cc;
  BYTE csect, *rbuff = (BYTE*)buff;
  *br = 0;  /* Clear read byte counter */

  FRESULT res = validate (file); /* Check validity */
  if (res != FR_OK)
    LEAVE_FF(file->fs, res);
  if (file->err)                /* Check error */
    LEAVE_FF(file->fs, (FRESULT)file->err);
  if (!(file->flag & FA_READ))   /* Check access mode */
    LEAVE_FF(file->fs, FR_DENIED);

  DWORD remain = file->fsize - file->fptr;
  if (btr > remain)
    btr = (UINT)remain;   /* Truncate btr by remaining bytes */

  for ( ;  btr; /* Repeat until all data read */ rbuff += rcnt, file->fptr += rcnt, *br += rcnt, btr -= rcnt) {
    if ((file->fptr % _MAX_SS) == 0) {
      /* On the sector boundary? */
      csect = (BYTE)(file->fptr / _MAX_SS & (file->fs->csize - 1));  /* Sector offset in the cluster */
      if (!csect) {
        /* On the cluster boundary? */
        if (file->fptr == 0)
          /* On the top of the file? */
          clst = file->sclust;  /* Follow from the origin */
        else {
          /* Middle or end of the file */
          if (file->cltbl)
            clst = clmtCluster (file, file->fptr);  /* Get cluster# from the CLMT */
          else
            clst = getFat (file->fs, file->clust);  /* Follow cluster chain on the FAT */
          }
        if (clst < 2)
          ABORT(file->fs, FR_INT_ERR);
        if (clst == 0xFFFFFFFF)
          ABORT(file->fs, FR_DISK_ERR);
        file->clust = clst;  /* Update current cluster */
        }

      sect = clusterToSector (file->fs, file->clust); /* Get current sector */
      if (!sect)
        ABORT(file->fs, FR_INT_ERR);
      sect += csect;
      cc = btr / _MAX_SS; /* When remaining bytes >= sector size, */
      if (cc) {
        /* Read maximum contiguous sectors directly */
        if (csect + cc > file->fs->csize) /* Clip at cluster boundary */
          cc = file->fs->csize - csect;
        if (disk_read (file->fs->drv, rbuff, sect, cc) != RES_OK)
          ABORT(file->fs, FR_DISK_ERR);
        if ((file->flag & FA__DIRTY) && file->dsect - sect < cc)
          memcpy(rbuff + ((file->dsect - sect) * _MAX_SS), file->buf.d8, _MAX_SS);
        rcnt = _MAX_SS * cc;     /* Number of bytes transferred */
        continue;
        }

      if (file->dsect != sect) {
        /* Load data sector if not in cache */
        if (file->flag & FA__DIRTY) {
          /* Write-back dirty sector cache */
          if (disk_write (file->fs->drv, file->buf.d8, file->dsect, 1) != RES_OK)
            ABORT(file->fs, FR_DISK_ERR);
          file->flag &= ~FA__DIRTY;
          }
        if (disk_read (file->fs->drv, file->buf.d8, sect, 1) != RES_OK)  /* Fill sector cache */
          ABORT(file->fs, FR_DISK_ERR);
        }
      file->dsect = sect;
      }

    rcnt = _MAX_SS - ((UINT)file->fptr % _MAX_SS);  /* Get partial sector data from sector buffer */
    if (rcnt > btr)
      rcnt = btr;
    memcpy (rbuff, &file->buf.d8[file->fptr % _MAX_SS], rcnt); /* Pick partial sector */
    }

  LEAVE_FF(file->fs, FR_OK);
  }
/*}}}*/
/*{{{*/
FRESULT f_write (FIL* file, const void *buff, UINT btw, UINT* bw) {

  DWORD clst, sect;
  UINT wcnt, cc;
  const BYTE *wbuff = (const BYTE*)buff;
  BYTE csect;

  *bw = 0;  /* Clear write byte counter */

  FRESULT res = validate (file);           /* Check validity */
  if (res != FR_OK)
    LEAVE_FF(file->fs, res);
  if (file->err)              /* Check error */
    LEAVE_FF(file->fs, (FRESULT)file->err);
  if (!(file->flag & FA_WRITE))       /* Check access mode */
    LEAVE_FF(file->fs, FR_DENIED);
  if (file->fptr + btw < file->fptr) btw = 0; /* File size cannot reach 4GB */

  for ( ;  btw;             /* Repeat until all data written */
    wbuff += wcnt, file->fptr += wcnt, *bw += wcnt, btw -= wcnt) {
    if ((file->fptr % _MAX_SS) == 0) { /* On the sector boundary? */
      csect = (BYTE)(file->fptr / _MAX_SS & (file->fs->csize - 1));  /* Sector offset in the cluster */
      if (!csect) {
        /* On the cluster boundary? */
        if (file->fptr == 0) {
          /* On the top of the file? */
          clst = file->sclust;
          /* Follow from the origin */
          if (clst == 0)
            /* When no cluster is allocated, */
            clst = createChain(file->fs, 0); /* Create a new cluster chain */
          }
        else {
          /* Middle or end of the file */
          if (file->cltbl)
            clst = clmtCluster (file, file->fptr);  /* Get cluster# from the CLMT */
          else
            clst = createChain (file->fs, file->clust); /* Follow or stretch cluster chain on the FAT */
          }

        if (clst == 0)
          break;   /* Could not allocate a new cluster (disk full) */
        if (clst == 1)
          ABORT(file->fs, FR_INT_ERR);
        if (clst == 0xFFFFFFFF)
          ABORT(file->fs, FR_DISK_ERR);

        file->clust = clst;     /* Update current cluster */
        if (file->sclust == 0)
          file->sclust = clst; /* Set start cluster if the first write */
        }

      if (file->flag & FA__DIRTY) {   /* Write-back sector cache */
        if (disk_write(file->fs->drv, file->buf.d8, file->dsect, 1) != RES_OK)
          ABORT(file->fs, FR_DISK_ERR);
        file->flag &= ~FA__DIRTY;
        }

      sect = clusterToSector(file->fs, file->clust); /* Get current sector */
      if (!sect)
        ABORT(file->fs, FR_INT_ERR);
      sect += csect;

      cc = btw / _MAX_SS;      /* When remaining bytes >= sector size, */
      if (cc) {           /* Write maximum contiguous sectors directly */
        if (csect + cc > file->fs->csize) /* Clip at cluster boundary */
          cc = file->fs->csize - csect;
        if (disk_write (file->fs->drv, wbuff, sect, cc) != RES_OK)
          ABORT(file->fs, FR_DISK_ERR);
        if (file->dsect - sect < cc) { /* Refill sector cache if it gets invalidated by the direct write */
          memcpy (file->buf.d8, wbuff + ((file->dsect - sect) * _MAX_SS), _MAX_SS);
          file->flag &= ~FA__DIRTY;
          }
        wcnt = _MAX_SS * cc;   /* Number of bytes transferred */
        continue;
        }

      if (file->dsect != sect) {    /* Fill sector cache with file data */
        if (file->fptr < file->fsize &&
          disk_read (file->fs->drv, file->buf.d8, sect, 1) != RES_OK)
            ABORT (file->fs, FR_DISK_ERR);
        }
      file->dsect = sect;
      }

    wcnt = _MAX_SS - ((UINT)file->fptr % _MAX_SS); /* Put partial sector into file I/O buffer */
    if (wcnt > btw)
      wcnt = btw;
    memcpy (&file->buf.d8[file->fptr % _MAX_SS], wbuff, wcnt); /* Fit partial sector */
    file->flag |= FA__DIRTY;
    }

  if (file->fptr > file->fsize)
    file->fsize = file->fptr;  /* Update file size if needed */
  file->flag |= FA__WRITTEN; /* Set file change flag */

  LEAVE_FF(file->fs, FR_OK);
  }
/*}}}*/
/*{{{*/
FRESULT f_sync (FIL* file) {

  DWORD tm;
  BYTE* dir;

  FRESULT res = validate (file);         /* Check validity of the object */
  if (res == FR_OK) {
    if (file->flag & FA__WRITTEN) {
      /* Has the file been written? ,  Write-back dirty buffer */
      if (file->flag & FA__DIRTY) {
        if (disk_write(file->fs->drv, file->buf.d8, file->dsect, 1) != RES_OK)
          LEAVE_FF(file->fs, FR_DISK_ERR);
        file->flag &= ~FA__DIRTY;
        }

      /* Update the directory entry */
      res = moveWindow (file->fs, file->dir_sect);
      if (res == FR_OK) {
        dir = file->dir_ptr;
        dir[DIR_Attr] |= AM_ARC;          /* Set archive bit */
        ST_DWORD(dir + DIR_FileSize, file->fsize);  /* Update file size */
        storeCluster (dir, file->sclust);          /* Update start cluster */
        tm = GET_FATTIME();             /* Update updated time */
        ST_DWORD(dir + DIR_WrtTime, tm);
        ST_WORD(dir + DIR_LstAccDate, 0);
        file->flag &= ~FA__WRITTEN;
        file->fs->wflag = 1;
        res = syncFs (file->fs);
        }
      }
    }

  LEAVE_FF(file->fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_close (FIL *file) {

  FRESULT res = f_sync (file);       // Flush cached data
  if (res == FR_OK) {
    res = validate (file);           // Lock volume
    if (res == FR_OK) {
      FATFS* fs = file->fs;
      res = dec_lock (file->lockid); // Decrement file open counter
      if (res == FR_OK)
        file->fs = 0;                // Invalidate file object
      unlock_fs (fs, FR_OK);       // Unlock volume
      }
    }

  return res;
  }
/*}}}*/
/*{{{*/
FRESULT f_lseek (FIL* file, DWORD ofs) {

  DWORD clst, bcs, nsect, ifptr;
  DWORD cl, pcl, ncl, tcl, dsc, tlen, ulen, *tbl;

  FRESULT res = validate (file);  /* Check validity of the object */
  if (res != FR_OK)
    LEAVE_FF(file->fs, res);
  if (file->err)                 /* Check error */
    LEAVE_FF(file->fs, (FRESULT)file->err);

  if (file->cltbl) {
    /* Fast seek */
    if (ofs == CREATE_LINKMAP) {
      /* Create CLMT */
      tbl = file->cltbl;
      tlen = *tbl++; ulen = 2;  /* Given table size and required table size */
      cl = file->sclust;      /* Top of the chain */
      if (cl) {
        do {
          /* Get a fragment */
          tcl = cl;
          ncl = 0;
          ulen += 2; /* Top, length and used items */
          do {
            pcl = cl; ncl++;
            cl = getFat (file->fs, cl);
            if (cl <= 1)
              ABORT(file->fs, FR_INT_ERR);
            if (cl == 0xFFFFFFFF)
              ABORT(file->fs, FR_DISK_ERR);
            } while (cl == pcl + 1);
          if (ulen <= tlen) {
            /* Store the length and top of the fragment */
            *tbl++ = ncl;
            *tbl++ = tcl;
            }
          } while (cl < file->fs->n_fatent);  /* Repeat until end of chain */
        }

      /* Number of items used */
      *file->cltbl = ulen;
      if (ulen <= tlen)
        /* Terminate table */
        *tbl = 0;
      else
        /* Given table size is smaller than required */
        res = FR_NOT_ENOUGH_CORE;
      }

    else {
      /* Fast seek */
      if (ofs > file->fsize)    /* Clip offset at the file size */
        ofs = file->fsize;
      file->fptr = ofs;       /* Set file pointer */
      if (ofs) {
        file->clust = clmtCluster (file, ofs - 1);
        dsc = clusterToSector (file->fs, file->clust);
        if (!dsc) ABORT(file->fs, FR_INT_ERR);
        dsc += (ofs - 1) / _MAX_SS & (file->fs->csize - 1);
        if (file->fptr % _MAX_SS && dsc != file->dsect) {  /* Refill sector cache if needed */
          if (file->flag & FA__DIRTY) {   /* Write-back dirty sector cache */
            if (disk_write(file->fs->drv, file->buf.d8, file->dsect, 1) != RES_OK)
              ABORT(file->fs, FR_DISK_ERR);
            file->flag &= ~FA__DIRTY;
            }
          if (disk_read (file->fs->drv, file->buf.d8, dsc, 1) != RES_OK) /* Load current sector */
            ABORT(file->fs, FR_DISK_ERR);
          file->dsect = dsc;
          }
        }
      }
    }

  else {
    /* Normal Seek */
    if (ofs > file->fsize && !(file->flag & FA_WRITE))
      ofs = file->fsize;

    ifptr = file->fptr;
    file->fptr = nsect = 0;
    if (ofs) {
      bcs = (DWORD)file->fs->csize * _MAX_SS;  /* Cluster size (byte) */
      if (ifptr > 0 && (ofs - 1) / bcs >= (ifptr - 1) / bcs) {
        /* When seek to same or following cluster, */
        file->fptr = (ifptr - 1) & ~(bcs - 1);  /* start from the current cluster */
        ofs -= file->fptr;
        clst = file->clust;
        }
      else {
        /* When seek to back cluster, */
        /* start from the first cluster */
        clst = file->sclust;
        if (clst == 0) {
          /* If no cluster chain, create a new chain */
          clst = createChain (file->fs, 0);
          if (clst == 1)
            ABORT(file->fs, FR_INT_ERR);
          if (clst == 0xFFFFFFFF)
            ABORT(file->fs, FR_DISK_ERR);
          file->sclust = clst;
          }
        file->clust = clst;
        }

      if (clst != 0) {
        while (ofs > bcs) {           /* Cluster following loop */
          if (file->flag & FA_WRITE) {      /* Check if in write mode or not */
            clst = createChain (file->fs, clst);  /* Force stretch if in write mode */
            if (clst == 0) {        /* When disk gets full, clip file size */
              ofs = bcs; break;
              }
            }
          else
            clst = getFat (file->fs, clst); /* Follow cluster chain if not in write mode */
          if (clst == 0xFFFFFFFF)
            ABORT(file->fs, FR_DISK_ERR);
          if (clst <= 1 || clst >= file->fs->n_fatent)
            ABORT(file->fs, FR_INT_ERR);
          file->clust = clst;
          file->fptr += bcs;
          ofs -= bcs;
          }

        file->fptr += ofs;
        if (ofs % _MAX_SS) {
          nsect = clusterToSector (file->fs, clst); /* Current sector */
          if (!nsect)
            ABORT(file->fs, FR_INT_ERR);
          nsect += ofs / _MAX_SS;
          }
        }
      }

    if (file->fptr % _MAX_SS && nsect != file->dsect) {
      /* Fill sector cache if needed */
      if (file->flag & FA__DIRTY) {
        /* Write-back dirty sector cache */
        if (disk_write (file->fs->drv, file->buf.d8, file->dsect, 1) != RES_OK)
          ABORT(file->fs, FR_DISK_ERR);
        file->flag &= ~FA__DIRTY;
        }

      if (disk_read (file->fs->drv, file->buf.d8, nsect, 1) != RES_OK)
        /* Fill sector cache */
        ABORT(file->fs, FR_DISK_ERR);
      file->dsect = nsect;
      }

    if (file->fptr > file->fsize) {
      /* Set file change flag if the file size is extended */
      file->fsize = file->fptr;
      file->flag |= FA__WRITTEN;
      }
    }

  LEAVE_FF(file->fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_truncate (FIL* file) {

  DWORD ncl;
  FRESULT res = validate (file);           /* Check validity of the object */
  if (res == FR_OK) {
    if (file->err)             /* Check error */
      res = (FRESULT)file->err;
    else {
      if (!(file->flag & FA_WRITE))   /* Check access mode */
        res = FR_DENIED;
      }
    }

  if (res == FR_OK) {
    if (file->fsize > file->fptr) {
      file->fsize = file->fptr; /* Set file size to current R/W point */
      file->flag |= FA__WRITTEN;
      if (file->fptr == 0) {  /* When set file size to zero, remove entire cluster chain */
        res = removeChain (file->fs, file->sclust);
        file->sclust = 0;
        }
      else {
        /* When truncate a part of the file, remove remaining clusters */
        ncl = getFat (file->fs, file->clust);
        res = FR_OK;
        if (ncl == 0xFFFFFFFF)
          res = FR_DISK_ERR;
        if (ncl == 1)
          res = FR_INT_ERR;
        if (res == FR_OK && ncl < file->fs->n_fatent) {
          res = putFat (file->fs, file->clust, 0x0FFFFFFF);
          if (res == FR_OK)
            res = removeChain (file->fs, ncl);
          }
        }

      if (res == FR_OK && (file->flag & FA__DIRTY)) {
        if (disk_write (file->fs->drv, file->buf.d8, file->dsect, 1) != RES_OK)
          res = FR_DISK_ERR;
        else
          file->flag &= ~FA__DIRTY;
        }
      }
    if (res != FR_OK)
      file->err = (FRESULT)res;
    }

  LEAVE_FF(file->fs, res);
  }
/*}}}*/

/*{{{*/
FRESULT f_opendir (DIR* dir, const TCHAR* path) {

  if (!dir)
    return FR_INVALID_OBJECT;

  /* Get logical drive number */
  FATFS* fs;
  FRESULT res = find_volume (&fs, &path, 0);
  if (res == FR_OK) {
    dir->fs = fs;

    BYTE sfn[12];
    WCHAR lfn[(_MAX_LFN + 1) * 2];
    dir->lfn = lfn;
    dir->fn = sfn;
    res = follow_path (dir, path); /* Follow the path to the directory */

    if (res == FR_OK) {
      /* Follow completed */
      if (dir->dir) {
        /* It is not the origin directory itself */
        if (dir->dir[DIR_Attr] & AM_DIR)
          /* The object is a sub directory */
          dir->sclust = loadCluster (fs, dir->dir);
        else
          /* The object is a file */
          res = FR_NO_PATH;
        }

      if (res == FR_OK) {
        dir->id = fs->id;
        res = dir_sdi (dir, 0); /* Rewind directory */
        if (res == FR_OK) {
          if (dir->sclust) {
            dir->lockid = inc_lock (dir, 0); /* Lock the sub directory */
            if (!dir->lockid)
              res = FR_TOO_MANY_OPEN_FILES;
            }
          else
            dir->lockid = 0; /* Root directory need not to be locked */
          }
        }
      }

    if (res == FR_NO_FILE)
      res = FR_NO_PATH;
    }

  if (res != FR_OK)
    dir->fs = 0;  /* Invalidate the directory object if function faild */

  LEAVE_FF(fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_closedir (DIR *dir) {

  FRESULT res = validate (dir);
  if (res == FR_OK) {
    FATFS* fs = dir->fs;
    if (dir->lockid)       /* Decrement sub-directory open counter */
      res = dec_lock (dir->lockid);
    if (res == FR_OK)
      dir->fs = 0;       /* Invalidate directory object */
    unlock_fs (fs, FR_OK);   /* Unlock volume */
    }

  return res;
  }
/*}}}*/
/*{{{*/
FRESULT f_readdir (DIR* dir, FILINFO* fileInfo) {

  FRESULT res = validate (dir);
  if (res == FR_OK) {
    if (!fileInfo)
      res = dir_sdi (dir, 0);     /* Rewind the directory object */
    else {
      BYTE sfn[12];
      WCHAR lfn [(_MAX_LFN + 1) * 2];
      dir->lfn = lfn;
      dir->fn = sfn;
      res = dir_read (dir, 0);
      if (res == FR_NO_FILE) {
        /* Reached end of directory */
        dir->sect = 0;
        res = FR_OK;
        }

      if (res == FR_OK) {
        /* A valid entry is found */
        getFileInfo (dir, fileInfo);    /* Get the object information */
        res = dir_next (dir, 0);    /* Increment index for next */
        if (res == FR_NO_FILE) {
          dir->sect = 0;
          res = FR_OK;
          }
        }
      }
    }

  LEAVE_FF(dir->fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_findnext (DIR* dir, FILINFO* fileInfo) {

  FRESULT res;
  for (;;) {
    res = f_readdir (dir, fileInfo);   /* Get a directory item */
    if (res != FR_OK || !fileInfo || !fileInfo->fname[0])
      break;  /* Terminate if any error or end of directory */
    if (fileInfo->lfname && pattern_matching (dir->pat, fileInfo->lfname, 0, 0))
      break; /* Test for LFN if exist */
    if (pattern_matching (dir->pat, fileInfo->fname, 0, 0))
      break; /* Test for SFN */
    }

  return res;
  }
/*}}}*/
/*{{{*/
FRESULT f_findfirst (DIR* dir, FILINFO* fileInfo, const TCHAR* path, const TCHAR* pattern) {

  dir->pat = pattern;                   /* Save pointer to pattern string */
  FRESULT res = f_opendir (dir, path);  /* Open the target directory */
  if (res == FR_OK)
    res = f_findnext (dir, fileInfo);        /* Find the first item */
  return res;
  }
/*}}}*/

/*{{{*/
FRESULT f_getcwd (TCHAR* buff, UINT len) {

  DIR dj;
  UINT i, n;
  DWORD ccl;
  TCHAR *tp;
  FILINFO fileInfo;

  *buff = 0;
  /* Get logical drive number */
  FRESULT res = find_volume(&dj.fs, (const TCHAR**)&buff, 0); /* Get current volume */
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    i = len;      /* Bottom of buffer (directory stack base) */
    dj.sclust = dj.fs->cdir;      /* Start to follow upper directory from current directory */
    while ((ccl = dj.sclust) != 0) {  /* Repeat while current directory is a sub-directory */
      res = dir_sdi(&dj, 1);      /* Get parent directory */
      if (res != FR_OK)
        break;

      res = dir_read(&dj, 0);
      if (res != FR_OK)
        break;

      dj.sclust = loadCluster (dj.fs, dj.dir);  /* Goto parent directory */
      res = dir_sdi(&dj, 0);
      if (res != FR_OK)
        break;

      do {              /* Find the entry links to the child directory */
        res = dir_read(&dj, 0);
        if (res != FR_OK)
          break;
        if (ccl == loadCluster (dj.fs, dj.dir))
          break;  /* Found the entry */
        res = dir_next(&dj, 0);
        } while (res == FR_OK);
      if (res == FR_NO_FILE)
        res = FR_INT_ERR;/* It cannot be 'not found'. */
      if (res != FR_OK)
        break;

      fileInfo.lfname = buff;
      fileInfo.lfsize = i;
      getFileInfo (&dj, &fileInfo);    /* Get the directory name and push it to the buffer */
      tp = fileInfo.fname;
      if (*buff)
        tp = buff;
      for (n = 0; tp[n]; n++) ;
      if (i < n + 3) {
        res = FR_NOT_ENOUGH_CORE;
          break;
        }
      while (n) buff[--i] = tp[--n];
      buff[--i] = '/';
      }

    tp = buff;
    if (res == FR_OK) {
      if (i == len)
        /* Root-directory */
        *tp++ = '/';
      else {
        /* Sub-directroy */
        do
          /* Add stacked path str */
          *tp++ = buff[i++];
          while (i < len);
        }
      }

    *tp = 0;
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_mkdir (const TCHAR* path) {

  DIR dj;
  BYTE *dir, n;
  DWORD dsc, dcl, pcl, tm = GET_FATTIME();

  /* Get logical drive number */
  FRESULT res = find_volume (&dj.fs, &path, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = follow_path (&dj, path);     /* Follow the file path */
    if (res == FR_OK)
      res = FR_EXIST;   /* Any object with same name is already existing */
    if (res == FR_NO_FILE && (dj.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_NO_FILE) {        /* Can create a new directory */
      dcl = createChain(dj.fs, 0);   /* Allocate a cluster for the new directory table */
      res = FR_OK;
      if (dcl == 0)
        res = FR_DENIED;    /* No space to allocate a new cluster */
      if (dcl == 1)
        res = FR_INT_ERR;
      if (dcl == 0xFFFFFFFF)
        res = FR_DISK_ERR;

      if (res == FR_OK)         /* Flush FAT */
        res = syncWindow (dj.fs);
      if (res == FR_OK) {         /* Initialize the new directory table */
        dsc = clusterToSector(dj.fs, dcl);
        dir = dj.fs->win.d8;
        memset(dir, 0, _MAX_SS);
        memset(dir + DIR_Name, ' ', 11); /* Create "." entry */
        dir[DIR_Name] = '.';
        dir[DIR_Attr] = AM_DIR;
        ST_DWORD(dir + DIR_WrtTime, tm);
        storeCluster (dir, dcl);
        memcpy(dir + SZ_DIRE, dir, SZ_DIRE);   /* Create ".." entry */

        dir[SZ_DIRE + 1] = '.';
        pcl = dj.sclust;
        if (dj.fs->fs_type == FS_FAT32 && pcl == dj.fs->dirbase)
          pcl = 0;
        storeCluster (dir + SZ_DIRE, pcl);
        for (n = dj.fs->csize; n; n--) {  /* Write dot entries and clear following sectors */
          dj.fs->winsect = dsc++;
          dj.fs->wflag = 1;
          res = syncWindow(dj.fs);
          if (res != FR_OK)
            break;
          memset(dir, 0, _MAX_SS);
          }
        }

      if (res == FR_OK)
        res = dir_register (&dj);  /* Register the object to the directoy */
      if (res != FR_OK)
        removeChain (dj.fs, dcl);     /* Could not register, remove cluster chain */
      else {
        dir = dj.dir;
        dir[DIR_Attr] = AM_DIR;       /* Attribute */
        ST_DWORD(dir + DIR_WrtTime, tm);  /* Created time */
        storeCluster (dir, dcl);         /* Table start cluster */
        dj.fs->wflag = 1;
        res = syncFs (dj.fs);
        }
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_chdir (const TCHAR* path) {

  /* Get logical drive number */
  DIR dj;
  FRESULT res = find_volume (&dj.fs, &path, 0);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = follow_path (&dj, path);

    if (res == FR_OK) {
      /* Follow completed */
      if (!dj.dir)
        dj.fs->cdir = dj.sclust;  /* Start directory itself */
      else {
        if (dj.dir[DIR_Attr] & AM_DIR)  /* Reached to the directory */
          dj.fs->cdir = loadCluster (dj.fs, dj.dir);
        else
          res = FR_NO_PATH;   /* Reached but a file */
        }
      }
    if (res == FR_NO_FILE)
      res = FR_NO_PATH;
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/

/*{{{*/
FRESULT f_getfree (const TCHAR* path, DWORD* nclst, FATFS** fatfs) {

  DWORD n, clst, sect, stat;
  UINT i;
  BYTE fat, *p;

  /* Get logical drive number */
  FRESULT res = find_volume (fatfs, &path, 0);
  FATFS* fs = *fatfs;
  if (res == FR_OK) {
    /* If free_clust is valid, return it without full cluster scan */
    if (fs->free_clust <= fs->n_fatent - 2)
      *nclst = fs->free_clust;
    else {
      /* Get number of free clusters */
      fat = fs->fs_type;
      n = 0;
      if (fat == FS_FAT12) {
        clst = 2;
        do {
          stat = getFat (fs, clst);
          if (stat == 0xFFFFFFFF) {
            res = FR_DISK_ERR;
            break;
            }
          if (stat == 1) {
            res = FR_INT_ERR;
            break;
            }
          if (stat == 0)
            n++;
          } while (++clst < fs->n_fatent);
        }

      else {
        clst = fs->n_fatent;
        sect = fs->fatbase;
        i = 0;
        p = 0;
        do {
          if (!i) {
            res = moveWindow (fs, sect++);
            if (res != FR_OK)
              break;
            p = fs->win.d8;
            i = _MAX_SS;
            }

          if (fat == FS_FAT16) {
            if (LD_WORD(p) == 0)
              n++;
            p += 2;
            i -= 2;
            }
          else {
            if ((LD_DWORD(p) & 0x0FFFFFFF) == 0)
              n++;
            p += 4;
            i -= 4;
            }
          } while (--clst);
        }

      fs->free_clust = n;
      fs->fsi_flag |= 1;
      *nclst = n;
      }
    }

  LEAVE_FF(fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_getlabel (const TCHAR* path, TCHAR* label, DWORD* vsn) {

#if _LFN_UNICODE
  WCHAR w;
#endif

  /* Get logical drive number */
  DIR dj;
  FRESULT res = find_volume (&dj.fs, &path, 0);

  /* Get volume label */
  if (res == FR_OK && label) {
    dj.sclust = 0;        /* Open root directory */
    res = dir_sdi (&dj, 0);
    if (res == FR_OK) {
      res = dir_read (&dj, 1); /* Get an entry with AM_VOL */
      if (res == FR_OK) {      /* A volume label is exist */
#if _LFN_UNICODE
        UINT i = 0;
        UINT j = 0;
        do {
          w = (i < 11) ? dj.dir[i++] : ' ';
          if (IsDBCS1(w) && i < 11 && IsDBCS2(dj.dir[i]))
            w = w << 8 | dj.dir[i++];
          label[j++] = ff_convert (w, 1);  /* OEM -> Unicode */
          } while (j < 11);
#else
        memcpy (label, dj.dir, 11);
#endif
        UINT k = 11;
        do {
          label[k] = 0;
          if (!k)
            break;
          } while (label[--k] == ' ');
        }

      if (res == FR_NO_FILE) {  /* No label, return nul string */
        label[0] = 0;
        res = FR_OK;
        }
      }
    }

  /* Get volume serial number */
  if (res == FR_OK && vsn) {
    res = moveWindow (dj.fs, dj.fs->volbase);
    if (res == FR_OK) {
      UINT i = dj.fs->fs_type == FS_FAT32 ? BS_VolID32 : BS_VolID;
      *vsn = LD_DWORD (&dj.fs->win.d8[i]);
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_setlabel (const TCHAR* label) {

  DIR dj;
  BYTE vn[11];
  UINT i, j, sl;
  WCHAR w;
  DWORD tm;

  /* Get logical drive number */
  FRESULT res = find_volume(&dj.fs, &label, 1);
  if (res)
    LEAVE_FF(dj.fs, res);

  /* Create a volume label in directory form */
  vn[0] = 0;
  for (sl = 0; label[sl]; sl++) ;       /* Get name length */
  for ( ; sl && label[sl - 1] == ' '; sl--) ; /* Remove trailing spaces */
  if (sl) { /* Create volume label in directory form */
    i = j = 0;
    do {
#if LFN_UNICODE
      w = ff_convert (ff_wtoupper (label[i++]), 0);
#else
      w = (BYTE)label[i++];
      if (IsDBCS1(w))
        w = (j < 10 && i < sl && IsDBCS2(label[i])) ? w << 8 | (BYTE)label[i++] : 0;
      w = ff_convert (ff_wtoupper (ff_convert(w, 1)), 0);
#endif
      if (!w || chk_chr("\"*+,.:;<=>\?[]|\x7F", w) || j >= (UINT)((w >= 0x100) ? 10 : 11)) /* Reject invalid characters for volume label */
        LEAVE_FF(dj.fs, FR_INVALID_NAME);
      if (w >= 0x100) vn[j++] = (BYTE)(w >> 8);
      vn[j++] = (BYTE)w;
      } while (i < sl);
    while (j < 11) vn[j++] = ' '; /* Fill remaining name field */
    if (vn[0] == DDEM)
      LEAVE_FF(dj.fs, FR_INVALID_NAME);  /* Reject illegal name (heading DDEM) */
    }

  /* Set volume label */
  dj.sclust = 0;          /* Open root directory */
  res = dir_sdi(&dj, 0);
  if (res == FR_OK) {
    res = dir_read(&dj, 1);   /* Get an entry with AM_VOL */
    if (res == FR_OK) {     /* A volume label is found */
      if (vn[0]) {
        memcpy(dj.dir, vn, 11);  /* Change the volume label name */
        tm = GET_FATTIME();
        ST_DWORD(dj.dir + DIR_WrtTime, tm);
        }
      else
        dj.dir[0] = DDEM;     /* Remove the volume label */
      dj.fs->wflag = 1;
      res = syncFs (dj.fs);
      }
    else {          /* No volume label is found or error */
      if (res == FR_NO_FILE) {
        res = FR_OK;
        if (vn[0]) {        /* Create volume label as new */
          res = dir_alloc(&dj, 1);  /* Allocate an entry for volume label */
          if (res == FR_OK) {
            memset(dj.dir, 0, SZ_DIRE);  /* Set volume label */
            memcpy(dj.dir, vn, 11);
            dj.dir[DIR_Attr] = AM_VOL;
            tm = GET_FATTIME();
            ST_DWORD(dj.dir + DIR_WrtTime, tm);
            dj.fs->wflag = 1;
            res = syncFs (dj.fs);
            }
          }
        }
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_unlink (const TCHAR* path) {

  DIR dj, sdj;
  BYTE *dir;
  DWORD dclst = 0;

  /* Get logical drive number */
  FRESULT res = find_volume (&dj.fs, &path, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = follow_path(&dj, path);   /* Follow the file path */
    if (res == FR_OK && (dj.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;      /* Cannot remove dot entry */
    if (res == FR_OK) res = chk_lock(&dj, 2); /* Cannot remove open object */
    if (res == FR_OK) {         /* The object is accessible */
      dir = dj.dir;
      if (!dir) {
        res = FR_INVALID_NAME;    /* Cannot remove the origin directory */
        }
      else {
        if (dir[DIR_Attr] & AM_RDO)
          res = FR_DENIED;    /* Cannot remove R/O object */
        }

      if (res == FR_OK) {
        dclst = loadCluster (dj.fs, dir);
        if (dclst && (dir[DIR_Attr] & AM_DIR)) {  /* Is it a sub-directory ? */
          if (dclst == dj.fs->cdir) {       /* Is it the current directory? */
            res = FR_DENIED;
            }
          else {
            memcpy(&sdj, &dj, sizeof (DIR)); /* Open the sub-directory */
            sdj.sclust = dclst;
            res = dir_sdi(&sdj, 2);
            if (res == FR_OK) {
              res = dir_read(&sdj, 0);      /* Read an item (excluding dot entries) */
              if (res == FR_OK) res = FR_DENIED;  /* Not empty? (cannot remove) */
              if (res == FR_NO_FILE) res = FR_OK; /* Empty? (can remove) */
              }
            }
          }
        }

      if (res == FR_OK) {
        res = dir_remove(&dj);    /* Remove the directory entry */
        if (res == FR_OK && dclst)  /* Remove the cluster chain if exist */
          res = removeChain(dj.fs, dclst);
        if (res == FR_OK)
          res = syncFs (dj.fs);
        }
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_stat (const TCHAR* path, FILINFO* fileInfo) {

  DIR dj;

  /* Get logical drive number */
  FRESULT res = find_volume (&dj.fs, &path, 0);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;

    res = follow_path (&dj, path); /* Follow the file path */
    if (res == FR_OK) {       /* Follow completed */
      if (dj.dir) {
        /* Found an object */
        if (fileInfo)
          getFileInfo (&dj, fileInfo);
        }
      else
        /* It is root directory */
        res = FR_INVALID_NAME;
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_chmod (const TCHAR* path, BYTE attr, BYTE mask) {

  DIR dj;
  BYTE* dir;

  /* Get logical drive number */
  FRESULT res = find_volume (&dj.fs, &path, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = follow_path (&dj, path);   /* Follow the file path */

    if (res == FR_OK && (dj.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_OK) {
      dir = dj.dir;
      if (!dir) {           /* Is it a root directory? */
        res = FR_INVALID_NAME;
        }
      else {            /* File or sub directory */
        mask &= AM_RDO|AM_HID|AM_SYS|AM_ARC;  /* Valid attribute mask */
        dir[DIR_Attr] = (attr & mask) | (dir[DIR_Attr] & (BYTE)~mask);  /* Apply attribute change */
        dj.fs->wflag = 1;
        res = syncFs (dj.fs);
        }
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_rename (const TCHAR* path_old, const TCHAR* path_new) {

  DIR djo, djn;
  BYTE buf[21], *dir;
  DWORD dw;

  /* Get logical drive number of the source object */
  FRESULT res = find_volume(&djo.fs, &path_old, 1);
  if (res == FR_OK) {
    djn.fs = djo.fs;
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    djo.lfn = lfn;
    djo.fn = sfn;
    res = follow_path(&djo, path_old);    /* Check old object */
    if (res == FR_OK && (djo.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_OK)
      res = chk_lock(&djo, 2);
    if (res == FR_OK) {           /* Old object is found */
      if (!djo.dir)            /* Is root dir? */
        res = FR_NO_FILE;
      else {
        memcpy (buf, djo.dir + DIR_Attr, 21); /* Save information about object except name */
        memcpy (&djn, &djo, sizeof (DIR));    /* Duplicate the directory object */
        if (get_ldnumber (&path_new) >= 0)   /* Snip drive number off and ignore it */
          res = follow_path (&djn, path_new);  /* and make sure if new object name is not conflicting */
        else
          res = FR_INVALID_DRIVE;
        if (res == FR_OK) res = FR_EXIST;   /* The new object name is already existing */
        if (res == FR_NO_FILE) {        /* It is a valid path and no name collision */
          res = dir_register(&djn);     /* Register the new entry */
          if (res == FR_OK) {
/* Start of critical section where any interruption can cause a cross-link */
            dir = djn.dir;          /* Copy information about object except name */
            memcpy(dir + 13, buf + 2, 19);
            dir[DIR_Attr] = buf[0] | AM_ARC;
            djo.fs->wflag = 1;
            if ((dir[DIR_Attr] & AM_DIR) && djo.sclust != djn.sclust) { /* Update .. entry in the sub-directory if needed */
              dw = clusterToSector (djo.fs, loadCluster (djo.fs, dir));
              if (!dw)
                res = FR_INT_ERR;
              else {
                res = moveWindow (djo.fs, dw);
                dir = djo.fs->win.d8 + SZ_DIRE * 1; /* Ptr to .. entry */
                if (res == FR_OK && dir[1] == '.') {
                  storeCluster (dir, djn.sclust);
                  djo.fs->wflag = 1;
                  }
                }
              }

            if (res == FR_OK) {
              res = dir_remove (&djo);   /* Remove old entry */
              if (res == FR_OK)
                res = syncFs (djo.fs);
              }
/* End of critical section */
            }
          }
        }
      }
    }

  LEAVE_FF(djo.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_utime (const TCHAR* path, const FILINFO* fileInfo) {

  DIR dj;
  BYTE *dir;

  /* Get logical drive number */
  FRESULT res = find_volume (&dj.fs, &path, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = follow_path (&dj, path); /* Follow the file path */

    if (res == FR_OK && (dj.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;

    if (res == FR_OK) {
      dir = dj.dir;
      if (!dir)
        /* Root directory */
        res = FR_INVALID_NAME;
      else {
        /* File or sub-directory */
        ST_WORD(dir + DIR_WrtTime, fileInfo->ftime);
        ST_WORD(dir + DIR_WrtDate, fileInfo->fdate);
        dj.fs->wflag = 1;
        res = syncFs (dj.fs);
        }
      }
    }

  LEAVE_FF(dj.fs, res);
  }
/*}}}*/
/*{{{*/
FRESULT f_mkfs (const TCHAR* path, BYTE sfd, UINT au) {

  int vol;
  BYTE fmt, md, sys, *tbl, pdrv;
  DWORD n_clst, vs, n, wsect;
  UINT i;
  DWORD b_vol, b_fat, b_dir, b_data;  /* LBA */
  DWORD n_vol, n_rsv, n_fat, n_dir; /* Size */
  FATFS *fs;
  DSTATUS stat;

#if _USE_TRIM
  DWORD eb[2];
#endif

  /* Check mounted drive and clear work area */
  if (sfd > 1)
    return FR_INVALID_PARAMETER;
  vol = get_ldnumber (&path);
  if (vol < 0)
    return FR_INVALID_DRIVE;

  fs = FatFs[vol];
  if (!fs)
    return FR_NOT_ENABLED;
  fs->fs_type = 0;

  pdrv = vol;  /* Physical drive */

  /* Get disk statics */
  stat = disk_initialize (pdrv);
  if (stat & STA_NOINIT)
    return FR_NOT_READY;
  if (stat & STA_PROTECT)
    return FR_WRITE_PROTECTED;

  /* Create a partition in this function */
  if (disk_ioctl (pdrv, GET_SECTOR_COUNT, &n_vol) != RES_OK || n_vol < 128)
    return FR_DISK_ERR;
   b_vol = (sfd) ? 0 : 63;   /* Volume start sector */
  n_vol -= b_vol;       /* Volume size */

  if (au & (au - 1))
    au = 0;
  if (!au) {
    /* AU auto selection */
    vs = n_vol / 2000;
    for (i = 0; vs < vst[i]; i++) ;
    au = cst[i];
    }

  /* Number of sectors per cluster */
  au /= _MAX_SS;
  if (!au)
    au = 1;
  if (au > 128)
    au = 128;

  /* Pre-compute number of clusters and FAT sub-type */
  n_clst = n_vol / au;
  fmt = FS_FAT12;
  if (n_clst >= MIN_FAT16)
    fmt = FS_FAT16;
  if (n_clst >= MIN_FAT32)
    fmt = FS_FAT32;

  /* Determine offset and size of FAT structure */
  if (fmt == FS_FAT32) {
    n_fat = ((n_clst * 4) + 8 + _MAX_SS - 1) / _MAX_SS;
    n_rsv = 32;
    n_dir = 0;
    }
  else {
    n_fat = (fmt == FS_FAT12) ? (n_clst * 3 + 1) / 2 + 3 : (n_clst * 2) + 4;
    n_fat = (n_fat + _MAX_SS - 1) / _MAX_SS;
    n_rsv = 1;
    n_dir = (DWORD)N_ROOTDIR * SZ_DIRE / _MAX_SS;
    }

  b_fat = b_vol + n_rsv;        /* FAT area start sector */
  b_dir = b_fat + n_fat * N_FATS;   /* Directory area start sector */
  b_data = b_dir + n_dir;       /* Data area start sector */
  if (n_vol < b_data + au - b_vol)
    return FR_MKFS_ABORTED;  /* Too small volume */

  /* Align data start sector to erase block boundary (for flash memory media) */
  if (disk_ioctl (pdrv, GET_BLOCK_SIZE, &n) != RES_OK || !n || n > 32768)
    n = 1;
  n = (b_data + n - 1) & ~(n - 1);  /* Next nearest erase block from current data start */
  n = (n - b_data) / N_FATS;
  if (fmt == FS_FAT32) {    /* FAT32: Move FAT offset */
    n_rsv += n;
    b_fat += n;
    }
  else /* FAT12/16: Expand FAT size */
    n_fat += n;

  /* Determine number of clusters and final check of validity of the FAT sub-type */
  n_clst = (n_vol - n_rsv - n_fat * N_FATS - n_dir) / au;
  if ((fmt == FS_FAT16 && n_clst < MIN_FAT16) || (fmt == FS_FAT32 && n_clst < MIN_FAT32))
    return FR_MKFS_ABORTED;

  /* Determine system ID in the partition table */
  if (fmt == FS_FAT32)
    sys = 0x0C;   /* FAT32X */
  else {
    if (fmt == FS_FAT12 && n_vol < 0x10000)
      sys = 0x01; /* FAT12(<65536) */
    else
      sys = (n_vol < 0x10000) ? 0x04 : 0x06;  /* FAT16(<65536) : FAT12/16(>=65536) */
    }

  if (sfd)   /* No partition table (SFD) */
    md = 0xF0;
  else {  /* Create partition table (FDISK) */
    memset (fs->win.d8, 0, _MAX_SS);
    tbl = fs->win.d8 + MBR_Table; /* Create partition table for single partition in the drive */
    tbl[1] = 1;           /* Partition start head */
    tbl[2] = 1;           /* Partition start sector */
    tbl[3] = 0;           /* Partition start cylinder */
    tbl[4] = sys;         /* System type */
    tbl[5] = 254;         /* Partition end head */
    n = (b_vol + n_vol) / 63 / 255;
    tbl[6] = (BYTE)(n >> 2 | 63); /* Partition end sector */
    tbl[7] = (BYTE)n;       /* End cylinder */
    ST_DWORD(tbl + 8, 63);      /* Partition start in LBA */
    ST_DWORD(tbl + 12, n_vol);    /* Partition size in LBA */
    ST_WORD(fs->win.d8 + BS_55AA, 0xAA55);  /* MBR signature */
    if (disk_write (pdrv, fs->win.d8, 0, 1) != RES_OK) /* Write it to the MBR */
      return FR_DISK_ERR;
    md = 0xF8;
    }

  /* Create BPB in the VBR */
  tbl = fs->win.d8;             /* Clear sector */
  memset (tbl, 0, _MAX_SS);
  memcpy (tbl, "\xEB\xFE\x90" "MSDOS5.0", 11);/* Boot jump code, OEM name */

  i = _MAX_SS;               /* Sector size */
  ST_WORD(tbl + BPB_BytsPerSec, i);

  tbl[BPB_SecPerClus] = (BYTE)au;     /* Sectors per cluster */
  ST_WORD(tbl + BPB_RsvdSecCnt, n_rsv); /* Reserved sectors */
  tbl[BPB_NumFATs] = N_FATS;        /* Number of FATs */

  i = (fmt == FS_FAT32) ? 0 : N_ROOTDIR;  /* Number of root directory entries */
  ST_WORD(tbl + BPB_RootEntCnt, i);
  if (n_vol < 0x10000) {          /* Number of total sectors */
    ST_WORD(tbl + BPB_TotSec16, n_vol);
    }
  else {
    ST_DWORD(tbl + BPB_TotSec32, n_vol);
    }
  tbl[BPB_Media] = md;          /* Media descriptor */

  ST_WORD(tbl + BPB_SecPerTrk, 63);   /* Number of sectors per track */
  ST_WORD(tbl + BPB_NumHeads, 255);   /* Number of heads */
  ST_DWORD(tbl + BPB_HiddSec, b_vol);   /* Hidden sectors */

  n = GET_FATTIME();            /* Use current time as VSN */
  if (fmt == FS_FAT32) {
    ST_DWORD(tbl + BS_VolID32, n);    /* VSN */
    ST_DWORD(tbl + BPB_FATSz32, n_fat); /* Number of sectors per FAT */
    ST_DWORD(tbl + BPB_RootClus, 2);  /* Root directory start cluster (2) */
    ST_WORD(tbl + BPB_FSInfo, 1);   /* FSINFO record offset (VBR + 1) */
    ST_WORD(tbl + BPB_BkBootSec, 6);  /* Backup boot record offset (VBR + 6) */
    tbl[BS_DrvNum32] = 0x80;      /* Drive number */
    tbl[BS_BootSig32] = 0x29;     /* Extended boot signature */
    memcpy (tbl + BS_VolLab32, "NO NAME    " "FAT32   ", 19); /* Volume label, FAT signature */
    }
  else {
    ST_DWORD(tbl + BS_VolID, n);    /* VSN */
    ST_WORD(tbl + BPB_FATSz16, n_fat);  /* Number of sectors per FAT */
    tbl[BS_DrvNum] = 0x80;        /* Drive number */
    tbl[BS_BootSig] = 0x29;       /* Extended boot signature */
    memcpy (tbl + BS_VolLab, "NO NAME    " "FAT     ", 19); /* Volume label, FAT signature */
    }

  ST_WORD(tbl + BS_55AA, 0xAA55);     /* Signature (Offset is fixed here regardless of sector size) */
  if (disk_write (pdrv, tbl, b_vol, 1) != RES_OK)  /* Write it to the VBR sector */
    return FR_DISK_ERR;
  if (fmt == FS_FAT32)          /* Write backup VBR if needed (VBR + 6) */
    disk_write (pdrv, tbl, b_vol + 6, 1);

  /* Initialize FAT area */
  wsect = b_fat;
  for (i = 0; i < N_FATS; i++) {    /* Initialize each FAT copy */
    memset (tbl, 0, _MAX_SS);      /* 1st sector of the FAT  */
    n = md;               /* Media descriptor byte */
    if (fmt != FS_FAT32) {
      n |= (fmt == FS_FAT12) ? 0x00FFFF00 : 0xFFFFFF00;
      ST_DWORD(tbl + 0, n);     /* Reserve cluster #0-1 (FAT12/16) */
      }
    else {
      n |= 0xFFFFFF00;
      ST_DWORD(tbl + 0, n);     /* Reserve cluster #0-1 (FAT32) */
      ST_DWORD(tbl + 4, 0xFFFFFFFF);
      ST_DWORD(tbl + 8, 0x0FFFFFFF);  /* Reserve cluster #2 for root directory */
      }

    if (disk_write (pdrv, tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;

    memset(tbl, 0, _MAX_SS);      /* Fill following FAT entries with zero */
    for (n = 1; n < n_fat; n++) {   /* This loop may take a time on FAT32 volume due to many single sector writes */
      if (disk_write (pdrv, tbl, wsect++, 1) != RES_OK)
        return FR_DISK_ERR;
      }
    }

  /* Initialize root directory */
  i = (fmt == FS_FAT32) ? au : (UINT)n_dir;
  do {
    if (disk_write (pdrv, tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;
    } while (--i);

#if _USE_TRIM /* Erase data area if needed */
  {
    eb[0] = wsect; eb[1] = wsect + (n_clst - ((fmt == FS_FAT32) ? 1 : 0)) * au - 1;
    disk_ioctl (pdrv, CTRL_TRIM, eb);
  }
#endif

  /* Create FSINFO if needed */
  if (fmt == FS_FAT32) {
    ST_DWORD(tbl + FSI_LeadSig, 0x41615252);
    ST_DWORD(tbl + FSI_StrucSig, 0x61417272);
    ST_DWORD(tbl + FSI_Free_Count, n_clst - 1); /* Number of free clusters */
    ST_DWORD(tbl + FSI_Nxt_Free, 2);      /* Last allocated cluster# */
    ST_WORD(tbl + BS_55AA, 0xAA55);
    disk_write(pdrv, tbl, b_vol + 1, 1);  /* Write original (VBR + 1) */
    disk_write(pdrv, tbl, b_vol + 7, 1);  /* Write backup (VBR + 7) */
    }

  return (disk_ioctl (pdrv, CTRL_SYNC, 0) == RES_OK) ? FR_OK : FR_DISK_ERR;
  }
/*}}}*/

/*{{{*/
int f_putc (TCHAR c, FIL* fp) {

  /* Initialize output buffer */
  putbuff pb;
  pb.fp = fp;
  pb.nchr = pb.idx = 0;

  /* Put a character */
  putc_bfd (&pb, c);

  UINT nw;
  if (pb.idx >= 0 && f_write (pb.fp, pb.buf, (UINT)pb.idx, &nw) == FR_OK && (UINT)pb.idx == nw)
    return pb.nchr;

  return EOF;
  }
/*}}}*/
/*{{{*/
int f_puts (const TCHAR* str, FIL* fp) {

  /* Initialize output buffer */
  putbuff pb;
  pb.fp = fp;
  pb.nchr = pb.idx = 0;

  /* Put the string */
  while (*str)
    putc_bfd(&pb, *str++);

  UINT nw;
  if (pb.idx >= 0 && f_write (pb.fp, pb.buf, (UINT)pb.idx, &nw) == FR_OK && (UINT)pb.idx == nw)
    return pb.nchr;

  return EOF;
  }
/*}}}*/
/*{{{*/
int f_printf (FIL* fp, const TCHAR* fmt, ...) {

  BYTE f, r;
  UINT nw, i, j, w;
  DWORD v;
  TCHAR c, d, s[16], *p;

  /* Initialize output buffer */
  putbuff pb;
  pb.fp = fp;
  pb.nchr = pb.idx = 0;

  va_list arp;
  va_start (arp, fmt);

  for (;;) {
    c = *fmt++;
    if (c == 0)
      break;      /* End of string */

    if (c != '%') {       /* Non escape character */
      putc_bfd (&pb, c);
      continue;
      }

    w = f = 0;
    c = *fmt++;
    if (c == '0') {
      /*{{{  flag: '0' padding*/
      f = 1;
      c = *fmt++;
      }
      /*}}}*/
    else if (c == '-') {
      /*{{{  flag: left justified*/
      f = 2;
      c = *fmt++;
      }
      /*}}}*/
    while (IsDigit(c)) {
      /*{{{  Precision*/
      w = w * 10 + c - '0';
      c = *fmt++;
      }
      /*}}}*/
    if (c == 'l' || c == 'L')  {
      /*{{{  Prefix: Size is long int*/
      f |= 4;
      c = *fmt++;
      }
      /*}}}*/
    if (!c)
      break;

    d = c;
    if (IsLower (d))
      d -= 0x20;

    switch (d) {        /* Type is... */
      /*{{{*/
      case 'S' :          /* String */
        p = va_arg(arp, TCHAR*);
        for (j = 0; p[j]; j++) ;
        if (!(f & 2)) {
          while (j++ < w)
            putc_bfd(&pb, ' ');
          }
        while (*p)
          putc_bfd (&pb, *p++);
        while (j++ < w)
          putc_bfd (&pb, ' ');
        continue;
      /*}}}*/
      /*{{{*/
      case 'C' :          /* Character */
        putc_bfd(&pb, (TCHAR)va_arg(arp, int));
        continue;
      /*}}}*/
      /*{{{*/
      case 'B' :          /* Binary */
        r = 2;
        break;
      /*}}}*/
      /*{{{*/
      case 'O' :          /* Octal */
        r = 8;
        break;
      /*}}}*/
      case 'D' :          /* Signed decimal */
      /*{{{*/
      case 'U' :          /* Unsigned decimal */
        r = 10;
        break;
      /*}}}*/
      /*{{{*/
      case 'X' :          /* Hexdecimal */
        r = 16;
        break;
      /*}}}*/
      /*{{{*/
      default:          /* Unknown type (pass-through) */
        putc_bfd(&pb, c);
        continue;
      /*}}}*/
      }

    // Get an argument and put it in numeral
    v = (f & 4) ? (DWORD)va_arg(arp, long) : ((d == 'D') ? (DWORD)(long)va_arg(arp, int) : (DWORD)va_arg(arp, unsigned int));
    if (d == 'D' && (v & 0x80000000)) {
      v = 0 - v;
      f |= 8;
      }

    i = 0;
    do {
      d = (TCHAR)(v % r); v /= r;
      if (d > 9)
        d += (c == 'x') ? 0x27 : 0x07;
      s[i++] = d + '0';
      } while (v && i < sizeof s / sizeof s[0]);

    if (f & 8)
      s[i++] = '-';
    j = i;
    d = (f & 1) ? '0' : ' ';

    while (!(f & 2) && j++ < w)
      putc_bfd (&pb, d);

    do
      putc_bfd (&pb, s[--i]);
      while (i);

    while (j++ < w)
      putc_bfd (&pb, d);
    }

  va_end (arp);

  if (pb.idx >= 0 && f_write (pb.fp, pb.buf, (UINT)pb.idx, &nw) == FR_OK && (UINT)pb.idx == nw)
    return pb.nchr;

  return EOF;
  }
/*}}}*/
/*{{{*/
TCHAR* f_gets (TCHAR* buff, int len, FIL* fp) {

  int n = 0;
  TCHAR c, *p = buff;
  BYTE s[2];
  UINT rc;

  while (n < len - 1) {
    /* Read characters until buffer gets filled */
#if _LFN_UNICODE
#if _STRF_ENCODE == 3
    /* Read a character in UTF-8 */
    f_read(fp, s, 1, &rc);
    if (rc != 1) break;
    c = s[0];
    if (c >= 0x80) {
      if (c < 0xC0)
        continue; /* Skip stray trailer */
      if (c < 0xE0) {     /* Two-byte sequence */
        f_read (fp, s, 1, &rc);
        if (rc != 1)
          break;
        c = (c & 0x1F) << 6 | (s[0] & 0x3F);
        if (c < 0x80)
          c = '?';
        }
      else {
        if (c < 0xF0) {   /* Three-byte sequence */
          f_read (fp, s, 2, &rc);
          if (rc != 2)
            break;
          c = c << 12 | (s[0] & 0x3F) << 6 | (s[1] & 0x3F);
          if (c < 0x800)
            c = '?';
          }
        else       /* Reject four-byte sequence */
          c = '?';
        }
      }
#elif _STRF_ENCODE == 2   /* Read a character in UTF-16BE */
    f_read(fp, s, 2, &rc);
    if (rc != 2)
      break;
    c = s[1] + (s[0] << 8);
#elif _STRF_ENCODE == 1   /* Read a character in UTF-16LE */
    f_read(fp, s, 2, &rc);
    if (rc != 2)
      break;
    c = s[0] + (s[1] << 8);
#else           /* Read a character in ANSI/OEM */
    f_read(fp, s, 1, &rc);
    if (rc != 1)
      break;
    c = s[0];
    if (IsDBCS1(c)) {
      f_read (fp, s, 1, &rc);
      if (rc != 1)
        break;
      c = (c << 8) + s[0];
      }
    c = ff_convert(c, 1); /* OEM -> Unicode */
    if (!c) c = '?';
#endif
#else           /* Read a character without conversion */
    f_read(fp, s, 1, &rc);
    if (rc != 1)
      break;
    c = s[0];
#endif
    if (c == '\r')
      continue; /* Strip '\r' */
    *p++ = c;
    n++;
    if (c == '\n')
      break;   /* Break on EOL */
    }

  *p = 0;
  return n ? buff : 0;      /* When no data read (eof or error), return with error. */
  }
/*}}}*/
