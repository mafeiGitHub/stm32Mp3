//{{{  fatFs.cpp copyright
// FatFs - FAT file system module  R0.11                 (C)ChaN, 2015
// FatFs module is a free software that opened under license policy of following conditions.
// Copyright (C) 2015, ChaN, all right reserved.
// 1. Redistributions of source code must retain the above copyright notice,
//    this condition and the following disclaimer.
// This software is provided by the copyright holder and contributors "AS IS"
// and any warranties related to this software are DISCLAIMED.
// The copyright owner or contributors be NOT LIABLE for any damages caused by use of this software.
//}}}
//{{{  includes
#include "string.h"
#include "stdlib.h"
#include "stdarg.h"

#include "fatFs.h"
#include "diskio.h"

#include "cLcd.h"
//}}}
//{{{  defines
#define USE_TRIM    0
#define N_FATS      1  // Number of FATs (1 or 2)
#define N_ROOTDIR 512  // Number of root directory entries for FAT12/16

//{{{  name status flags defines
#define NSFLAG   11    // Index of name status byte in fn[]

#define NS_LOSS  0x01  // Out of 8.3 format
#define NS_LFN   0x02  // Force to create LFN entry
#define NS_LAST  0x04  // Last segment
#define NS_BODY  0x08  // Lower case flag (body)
#define NS_EXT   0x10  // Lower case flag (ext)
#define NS_DOT   0x20  // Dot entry
//}}}
//{{{  BS, BPB offsets
#define BS_jmpBoot       0  // x86 jump instruction (3)
#define BS_OEMName       3  // OEM name (8)
#define BPB_BytsPerSec  11  // Sector size [byte] (2)
#define BPB_SecPerClus  13  // Cluster size [sector] (1)
#define BPB_RsvdSecCnt  14  // Size of reserved area [sector] (2)
#define BPB_NumFATs     16  // Number of FAT copies (1)
#define BPB_RootEntCnt  17  // Number of root directory entries for FAT12/16 (2)
#define BPB_TotSec16    19  // Volume size [sector] (2)
#define BPB_Media       21  // Media descriptor (1)
#define BPB_FATSz16     22  // FAT size [sector] (2)
#define BPB_SecPerTrk   24  // Track size [sector] (2)
#define BPB_NumHeads    26  // Number of heads (2)
#define BPB_HiddSec     28  // Number of special hidden sectors (4)
#define BPB_TotSec32    32  // Volume size [sector] (4)
#define BS_DrvNum       36  // Physical drive number (2)
#define BS_BootSig      38  // Extended boot signature (1)
#define BS_VolID        39  // Volume serial number (4)
#define BS_VolLab       43  // Volume label (8)
#define BS_FilSysType   54  // File system type (1)
#define BPB_FATSz32     36  // FAT size [sector] (4)
#define BPB_ExtFlags    40  // Extended flags (2)
#define BPB_FSVer       42  // File system version (2)
#define BPB_RootClus    44  // Root directory first cluster (4)
#define BPB_FSInfo      48  // Offset of FSINFO sector (2)
#define BPB_BkBootSec   50  // Offset of backup boot sector (2)
#define BS_DrvNum32     64  // Physical drive number (2)
#define BS_BootSig32    66  // Extended boot signature (1)
#define BS_VolID32      67  // Volume serial number (4)
#define BS_VolLab32     71  // Volume label (8)
#define BS_FilSysType32 82  // File system type (1)
//}}}
//{{{  FSI offsets
#define FSI_LeadSig      0  // FSI: Leading signature (4)
#define FSI_StrucSig   484  // FSI: Structure signature (4)
#define FSI_Free_Count 488  // FSI: Number of free clusters (4)
#define FSI_Nxt_Free   492  // FSI: Last allocated cluster (4)
//}}}
//{{{  FAT sub type (FATFS.fsType)
#define FS_FAT12         1
#define FS_FAT16         2
#define FS_FAT32         3
//}}}

#define MIN_FAT16     4086U  // Minimum number of clusters as FAT16
#define MIN_FAT32    65526U  // Minimum number of clusters as FAT32
#define MBR_Table      446  // MBR: Partition table offset (2)
#define SZ_PTE          16  // MBR: Size of a partition table entry
#define BS_55AA        510  // Signature word (2)

//  DIR offsets
#define DIR_Name          0 // Short file name (11)
#define DIR_Attr         11 // Attribute (1)
#define DIR_NTres        12 // Lower case flag (1)
#define DIR_CrtTimeTenth 13 // Created time sub-second (1)
#define DIR_CrtTime      14 // Created time (2)
#define DIR_CrtDate      16 // Created date (2)
#define DIR_LstAccDate   18 // Last accessed date (2)
#define DIR_FstClusHI    20 // Higher 16-bit of first cluster (2)
#define DIR_WrtTime      22 // Modified time (2)
#define DIR_WrtDate      24 // Modified date (2)
#define DIR_FstClusLO    26 // Lower 16-bit of first cluster (2)
#define DIR_FileSize     28 // File size (4)

// LDIR offsets
#define LDIR_Ord          0 // LFN entry order and LLE flag (1)
#define LDIR_Attr        11 // LFN attribute (1)
#define LDIR_Type        12 // LFN type (1)
#define LDIR_Chksum      13 // Sum of corresponding SFN entry
#define LDIR_FstClusLO   26 // Must be zero (0)
#define SZ_DIRE          32 // Size of a directory entry

#define LLEF           0x40 // Last long entry flag in LDIR_Ord
#define DDEM           0xE5 // Deleted directory entry mark at DIR_Name[0]
#define RDDEM          0x05 // Replacement of the character collides with DDEM
//}}}
//{{{  macros
#define IS_UPPER(c)  (((c) >= 'A') && ((c) <= 'Z'))
#define IS_LOWER(c)  (((c) >= 'a') && ((c) <= 'z'))
#define IS_DIGIT(c)  (((c) >= '0') && ((c) <= '9'))

#define LD_WORD(ptr)  (WORD)(((WORD)*((BYTE*)(ptr)+1)<<8)|(WORD)*(BYTE*)(ptr))
#define LD_DWORD(ptr) (DWORD)(((DWORD)*((BYTE*)(ptr)+3)<<24)|((DWORD)*((BYTE*)(ptr)+2)<<16)|((WORD)*((BYTE*)(ptr)+1)<<8)|*(BYTE*)(ptr))

#define ST_WORD(ptr,val)  *(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8)
#define ST_DWORD(ptr,val) *(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8); *((BYTE*)(ptr)+2)=(BYTE)((DWORD)(val)>>16); *((BYTE*)(ptr)+3)=(BYTE)((DWORD)(val)>>24)

#define ABORT(result) { mResult = result; mFatFs->unlock (mResult); return mResult; }
//}}}
//{{{  static const
// Offset of LFN characters in the directory entry
static const BYTE kLongFileNameOffset[] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };

static const WORD kVst[] = {  1024,   512,  256,  128,   64,    32,   16,    8,    4,    2,   0 };
static const WORD kCst[] = { 32768, 16384, 8192, 4096, 2048, 16384, 8192, 4096, 2048, 1024, 512 };

//{{{
//  CP1252(0x80-0xFF) to Unicode conversion table
static const WCHAR kTable[] = {
  0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
  0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
  0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178,
  0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
  0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
  0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
  0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
  0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
  0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
  0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
  0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
  0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
  0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
  0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
  0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
  };
//}}}
//{{{
static const WCHAR kTableLower[] = {
  0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
  0xA1, 0x00A2, 0x00A3, 0x00A5, 0x00AC, 0x00AF,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0x0FF,
  0x101, 0x103, 0x105, 0x107, 0x109, 0x10B, 0x10D, 0x10F,
  0x111, 0x113, 0x115, 0x117, 0x119, 0x11B, 0x11D, 0x11F,
  0x121, 0x123, 0x125, 0x127, 0x129, 0x12B, 0x12D, 0x12F,
  0x131, 0x133, 0x135, 0x137, 0x13A, 0x13C, 0x13E,
  0x140, 0x142, 0x144, 0x146, 0x148, 0x14B, 0x14D, 0x14F,
  0x151, 0x153, 0x155, 0x157, 0x159, 0x15B, 0x15D, 0x15F,
  0x161, 0x163, 0x165, 0x167, 0x169, 0x16B, 0x16D, 0x16F,
  0x171, 0x173, 0x175, 0x177, 0x17A, 0x17C, 0x17E,
  0x192, 0x3B1, 0x3B2, 0x3B3, 0x3B4, 0x3B5, 0x3B6, 0x3B7, 0x3B8, 0x3B9, 0x3BA, 0x3BB, 0x3BC, 0x3BD, 0x3BE, 0x3BF,
  0x3C0, 0x3C1, 0x3C3, 0x3C4, 0x3C5, 0x3C6, 0x3C7, 0x3C8, 0x3C9, 0x3CA,
  0x430, 0x431, 0x432, 0x433, 0x434, 0x435, 0x436, 0x437, 0x438, 0x439, 0x43A, 0x43B, 0x43C, 0x43D, 0x43E, 0x43F,
  0x440, 0x441, 0x442, 0x443, 0x444, 0x445, 0x446, 0x447, 0x448, 0x449, 0x44A, 0x44B, 0x44C, 0x44D, 0x44E, 0x44F,
  0x451, 0x452, 0x453, 0x454, 0x455, 0x456, 0x457, 0x458, 0x459, 0x45A, 0x45B, 0x45C, 0x45E, 0x45F,
   0x2170, 0x2171, 0x2172, 0x2173, 0x2174, 0x2175, 0x2176, 0x2177, 0x2178, 0x2179, 0x217A, 0x217B, 0x217C, 0x217D, 0x217E, 0x217F,
   0xFF41, 0xFF42, 0xFF43, 0xFF44, 0xFF45, 0xFF46, 0xFF47, 0xFF48, 0xFF49, 0xFF4A, 0xFF4B, 0xFF4C, 0xFF4D, 0xFF4E, 0xFF4F,
   0xFF50, 0xFF51, 0xFF52, 0xFF53, 0xFF54, 0xFF55, 0xFF56, 0xFF57, 0xFF58, 0xFF59, 0xFF5A, 0
   };
//}}}
//{{{
static const WCHAR kTableUpper[] = {
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x21,
  0xFFE0, 0xFFE1, 0xFFE5, 0xFFE2, 0xFFE3,
  0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
  0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
  0x178, 0x100, 0x102, 0x104, 0x106, 0x108, 0x10A, 0x10C, 0x10E,
  0x110, 0x112, 0x114, 0x116, 0x118, 0x11A, 0x11C, 0x11E,
  0x120, 0x122, 0x124, 0x126, 0x128, 0x12A, 0x12C, 0x12E,
  0x130, 0x132, 0x134, 0x136, 0x139, 0x13B, 0x13D, 0x13F,
  0x141, 0x143, 0x145, 0x147, 0x14A, 0x14C, 0x14E,
  0x150, 0x152, 0x154, 0x156, 0x158, 0x15A, 0x15C, 0x15E,
  0x160, 0x162, 0x164, 0x166, 0x168, 0x16A, 0x16C, 0x16E,
  0x170, 0x172, 0x174, 0x176, 0x179, 0x17B, 0x17D, 0x191,
  0x391, 0x392, 0x393, 0x394, 0x395, 0x396, 0x397, 0x398, 0x399, 0x39A, 0x39B, 0x39C, 0x39D, 0x39E, 0x39F,
  0x3A0, 0x3A1, 0x3A3, 0x3A4, 0x3A5, 0x3A6, 0x3A7, 0x3A8, 0x3A9, 0x3AA,
  0x410, 0x411, 0x412, 0x413, 0x414, 0x415, 0x416, 0x417, 0x418, 0x419, 0x41A, 0x41B, 0x41C, 0x41D, 0x41E, 0x41F,
  0x420, 0x421, 0x422, 0x423, 0x424, 0x425, 0x426, 0x427, 0x428, 0x429, 0x42A, 0x42B, 0x42C, 0x42D, 0x42E, 0x42F,
  0x401, 0x402, 0x403, 0x404, 0x405, 0x406, 0x407, 0x408, 0x409, 0x40A, 0x40B, 0x40C, 0x40E, 0x40F,
  0x2160, 0x2161, 0x2162, 0x2163, 0x2164, 0x2165, 0x2166, 0x2167, 0x2168, 0x2169, 0x216A, 0x216B, 0x216C, 0x216D, 0x216E, 0x216F,
  0xFF21, 0xFF22, 0xFF23, 0xFF24, 0xFF25, 0xFF26, 0xFF27, 0xFF28, 0xFF29, 0xFF2A, 0xFF2B, 0xFF2C, 0xFF2D, 0xFF2E, 0xFF2F,
  0xFF30, 0xFF31, 0xFF32, 0xFF33, 0xFF34, 0xFF35, 0xFF36, 0xFF37, 0xFF38, 0xFF39, 0xFF3A, 0
  };
//}}}

//{{{
// Upper conversion table for extended characters
static const BYTE kExCvt[] = {
  0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
  0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0xAd,0x9B,0x8C,0x9D,0xAE,0x9F,
  0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
  0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
  0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
  0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
  0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
  0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0x9F
  };
//}}}
//}}}
//{{{  static utils
//{{{
static DWORD getFatTime() {
// year, mon, day, time - 1.04.2016
  return ((2016 - 1980) << 25) | (4 << 21) | (1 << 16);
  }
//}}}

//{{{
static WCHAR wideToUpperCase (WCHAR chr) {

  int i;
  for (i = 0; kTableLower[i] && chr != kTableLower[i]; i++);
  return kTableLower[i] ? kTableUpper[i] : chr;
  }
//}}}
//{{{
static WCHAR convertToFromUnicode (WCHAR chr, UINT direction /* 0: Unicode to OEMCP, 1: OEMCP to Unicode */ ) {
// Converted character, Returns zero on error

  WCHAR c;
  if (chr < 0x80)
    // ASCII
    c = chr;
  else if (direction)
    // OEMCP to Unicode
    c = (chr >= 0x100) ? 0 : kTable[chr - 0x80];
  else {
    // Unicode to OEMCP
    for (c = 0; c < 0x80; c++) {
      if (chr == kTable[c])
        break;
      }
    c = (c + 0x80) & 0xFF;
    }

  return c;
  }
//}}}

//{{{
static WCHAR get_achar (const char** ptr) {

  WCHAR chr = (BYTE)*(*ptr)++;
  if (IS_LOWER (chr))
    chr -= 0x20; // To upper ASCII char

  if (chr >= 0x80)
    chr = kExCvt[chr - 0x80]; // To upper SBCS extended char

  return chr;
  }
//}}}
//{{{
static int matchPattern (const char* pat, const char* nam, int skip, int inf) {

  while (skip--) {
    // Pre-skip name chars
    if (!get_achar (&nam))
      return 0; // Branch mismatched if less name chars
    }
  if (!*pat && inf)
    return 1;  // short circuit

  WCHAR nc;
  do {
    const char* pp = pat;
    const char* np = nam;  // Top of pattern and name to match
    for (;;) {
      if (*pp == '?' || *pp == '*') {
        // Wildcard?
        int nm = 0;
        int nx = 0;
        do {
          // Analyze the wildcard chars
          if (*pp++ == '?')
            nm++;
          else
            nx = 1;
          } while (*pp == '?' || *pp == '*');

        if (matchPattern (pp, np, nm, nx))
          return 1; // Test new branch (recurs upto number of wildcard blocks in the pattern)

        nc = *np;
        break;  // Branch mismatched
        }

      WCHAR pc = get_achar (&pp);  // Get a pattern char
      nc = get_achar (&np);  // Get a name char
      if (pc != nc)
        break;  // Branch mismatched?
      if (!pc)
        return 1;  // Branch matched? (matched at end of both strings)
      }

    get_achar (&nam);     // nam++
    } while (inf && nc);  // Retry until end of name if infinite search is specified

  return 0;
  }
//}}}

//{{{
static int compareLongFileName (WCHAR* lfnbuf, BYTE* dir) {

  UINT i = ((dir[LDIR_Ord] & ~LLEF) - 1) * 13; /* Get offset in the LongFileName buffer */
  UINT s = 0;
  WCHAR wc = 1;
  do {
    WCHAR uc = LD_WORD (dir + kLongFileNameOffset[s]);  /* Pick an LongFileName character from the entry */
    if (wc) { /* Last character has not been processed */
      wc = wideToUpperCase (uc);   /* Convert it to upper case */
      if (i >= MAX_LFN || wc != wideToUpperCase (lfnbuf[i++]))  /* Compare it */
        return 0;       /* Not matched */
      }
    else if (uc != 0xFFFF)
      return 0; /* Check filler */
    } while (++s < 13);  /* Repeat until all characters in the entry are checked */

  if ((dir[LDIR_Ord] & LLEF) && wc && lfnbuf[i])  /* Last segment matched but different length */
    return 0;

  return 1;  /* The part of LFN matched */
  }
//}}}
//{{{
static int pickLongFileName (WCHAR* lfnbuf, BYTE* dir) {

  UINT i = ((dir[LDIR_Ord] & 0x3F) - 1) * 13;  /* Offset in the LongFileName buffer */
  UINT s = 0;
  WCHAR wc = 1;
  do {
    WCHAR uc = LD_WORD (dir + kLongFileNameOffset[s]);    /* Pick an LongFileName character from the entry */
    if (wc) { /* Last character has not been processed */
      if (i >= MAX_LFN) return 0;  /* Buffer overflow? */
      lfnbuf[i++] = wc = uc;      /* Store it */
      }
    else if (uc != 0xFFFF)
      return 0;   /* Check filler */
    } while (++s < 13);           /* Read all character in the entry */

  if (dir[LDIR_Ord] & LLEF) {       /* Put terminator if it is the last LongFileName part */
    if (i >= MAX_LFN)
      return 0;    /* Buffer overflow? */
    lfnbuf[i] = 0;
    }

  return 1;
  }
//}}}
//{{{
static void fitLongFileName (const WCHAR* lfnbuf, BYTE* dir, BYTE ord, BYTE sum) {

  dir[LDIR_Chksum] = sum;   /* Set check sum */
  dir[LDIR_Attr] = AM_LFN;  /* Set attribute. LFN entry */
  dir[LDIR_Type] = 0;
  ST_WORD (dir + LDIR_FstClusLO, 0);

  UINT i = (ord - 1) * 13;  /* Get offset in the LFN buffer */
  UINT s = 0;
  WCHAR wc = 0;
  do {
    if (wc != 0xFFFF)
      wc = lfnbuf[i++];  /* Get an effective character */
    ST_WORD (dir + kLongFileNameOffset[s], wc);  /* Put it */
    if (!wc)
      wc = 0xFFFF;  /* Padding characters following last character */
    } while (++s < 13);

  if (wc == 0xFFFF || !lfnbuf[i])
    ord |= LLEF;  /* Bottom LongFileName part is the start of LongFileName sequence */

  dir[LDIR_Ord] = ord;      /* Set the LongFileName order */
  }
//}}}
//{{{
static BYTE sumShortFileName (const BYTE* dir) {

  UINT n = 11;
  BYTE sum = 0;
  do {
    sum = (sum >> 1) + (sum << 7) + *dir++;
    } while (--n);

  return sum;
  }
//}}}

//{{{
static void generateNumberedName (BYTE* dst, const BYTE* src, const WCHAR* lfn, UINT seq) {

  BYTE ns[8], c;
  UINT i, j;
  WCHAR wc;
  DWORD sr;

  memcpy (dst, src, 11);

  if (seq > 5) {
    // On many collisions, generate a hash number instead of sequential number
    sr = seq;
    while (*lfn) {
      // Create a CRC
      wc = *lfn++;
      for (i = 0; i < 16; i++) {
        sr = (sr << 1) + (wc & 1);
        wc >>= 1;
        if (sr & 0x10000)
          sr ^= 0x11021;
        }
      }
    seq = (UINT)sr;
    }

  // itoa (hexdecimal)
  i = 7;
  do {
    c = (seq % 16) + '0';
    if (c > '9')
      c += 7;
    ns[i--] = c;
    seq /= 16;
    } while (seq);

  ns[i] = '~';

  // Append the number
  for (j = 0; j < i && dst[j] != ' '; j++);
  do {
    dst[j++] = (i < 8) ? ns[i++] : ' ';
    } while (j < 8);
  }
//}}}

//{{{
static void storeCluster (BYTE* dir, DWORD cluster) {

  ST_WORD (dir + DIR_FstClusLO, cluster);
  ST_WORD (dir + DIR_FstClusHI, cluster >> 16);
  }
//}}}
//}}}

// cFatFs
//{{{  static member vars
cFatFs* cFatFs::mFatFs = nullptr;
cFatFs::cFileLock cFatFs::mFileLock[FS_LOCK];
//}}}
//{{{
cFatFs* cFatFs::create() {

  if (!mFatFs)
    mFatFs = new cFatFs();
  return mFatFs;
  }
//}}}
//{{{
FRESULT cFatFs::mount() {

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 0);
  if (mResult == FR_OK) {
    // Open root directory
    directory.mStartCluster = 0;
    mResult = directory.setIndex (0);
    if (mResult == FR_OK) {
      // Get an entry with AM_VOL
      mResult = directory.read (1);
      if (mResult == FR_OK) {
        // volume label exists
        memcpy (mLabel, directory.mDirShortFileName, 11);
        UINT k = 11;
        do {
          mLabel[k] = 0;
          if (!k)
            break;
          } while (mLabel[--k] == ' ');
        }

      if (mResult == FR_NO_FILE) {
        // No label, return empty string
        mLabel[0] = 0;
        mResult = FR_OK;
        }
      }

    // Get volume serial number
    mResult = moveWindow (mVolBase);
    if (mResult == FR_OK) {
      UINT i = mFsType == FS_FAT32 ? BS_VolID32 : BS_VolID;
      mVolumeSerialNumber = LD_DWORD (&mWindowBuffer[i]);
      }

    // if mFreeClusters is valid, return it without full cluster scan
    if (mFreeClusters > mNumFatEntries - 2) {
      //{{{  scan number of free clusters
      DWORD n, clst, sect, stat;
      UINT i;
      BYTE *p;

      BYTE fat = mFsType;
      n = 0;
      if (fat == FS_FAT12) {
        clst = 2;
        do {
          stat = getFat (clst);
          if (stat == 0xFFFFFFFF) {
            mResult = FR_DISK_ERR;
            break;
            }
          if (stat == 1) {
            mResult = FR_INT_ERR;
            break;
            }
          if (stat == 0)
            n++;
          } while (++clst < mNumFatEntries);
        }

      else {
        clst = mNumFatEntries;
        sect = mFatBase;
        i = 0;
        p = 0;
        do {
          if (!i) {
            mResult = moveWindow (sect++);
            if (!mResult == FR_OK)
              break;
            p = mWindowBuffer;
            i = SECTOR_SIZE;
            }

          if (fat == FS_FAT16) {
            if (LD_WORD (p) == 0)
              n++;
            p += 2;
            i -= 2;
            }
          else {
            if ((LD_DWORD (p) & 0x0FFFFFFF) == 0)
              n++;
            p += 4;
            i -= 4;
            }
          } while (--clst);
        }

      mFreeClusters = n;
      mFsiFlag |= 1;
      }
      //}}}
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::getCurrentDirectory (char* buff, UINT len) {

  UINT i, n;
  DWORD ccl;
  char *tp;

  *buff = 0;
  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 0);
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    i = len;   // Bottom of buffer (directory stack base)
    directory.mStartCluster = mCurDirSector;  // Start to follow upper directory from current directory
    while ((ccl = directory.mStartCluster) != 0) {
      // Repeat while current directory is a sub-directory, Get parent directory
      mResult = directory.setIndex (1);
      if (!mResult == FR_OK)
        break;

      mResult = directory.read (0);
      if (!mResult == FR_OK)
        break;

      // Goto parent directory
      directory.mStartCluster = loadCluster (directory.mDirShortFileName);
      mResult = directory.setIndex (0);
      if (!mResult == FR_OK)
        break;

      do {
        // Find the entry links to the child directory
        mResult = directory.read (0);
        if (!mResult == FR_OK)
          break;
        if (ccl == loadCluster (directory.mDirShortFileName))
          break;  // Found the entry
        mResult = directory.next (0);
        } while (mResult == FR_OK);
      if (mResult == FR_NO_FILE)
        mResult = FR_INT_ERR; // It cannot be 'not found'
      if (!mResult == FR_OK)
        break;

      // Get the directory name and push it to the buffer
      cFileInfo fileInfo;
      strcpy (fileInfo.mLongFileName, buff);
      fileInfo.mLongFileNameSize = i;
      directory.getFileInfo (fileInfo);
      tp = fileInfo.mShortFileName;
      if (*buff)
        tp = buff;
      for (n = 0; tp[n]; n++);
      if (i < n + 3) {
        mResult = FR_NOT_ENOUGH_CORE;
          break;
        }
      while (n)
        buff[--i] = tp[--n];
      buff[--i] = '/';
      }

    tp = buff;
    if (mResult == FR_OK) {
      if (i == len) // Root-directory
        *tp++ = '/';
      else {
        // Sub-directroy
        do
          // Add stacked path str
          *tp++ = buff[i++];
          while (i < len);
        }
      }

    *tp = 0;
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::changeDirectory (const char* path) {

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 0);
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    if (directory.followPath (path)) {
      if (!directory.mDirShortFileName)
        // start directory itself
        mCurDirSector = directory.mStartCluster;
      else if (directory.mDirShortFileName[DIR_Attr] & AM_DIR)
        // reached directory
        mCurDirSector = loadCluster (directory.mDirShortFileName);
      else
        // reached file
        mResult = FR_NO_PATH;
      }

    if (mResult == FR_NO_FILE)
      mResult = FR_NO_PATH;
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::makeSubDirectory (const char* path) {

  BYTE *dir, n;
  DWORD dsc, dcl, pcl;

  DWORD tm = getFatTime();

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 1);
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    if (directory.followPath (path))
      mResult = FR_EXIST;   // Any object with same name is already existing
    if (mResult == FR_NO_FILE && (directory.mShortFileName[NSFLAG] & NS_DOT))
      mResult = FR_INVALID_NAME;

    if (mResult == FR_NO_FILE) {
      /* Can create a new directory */
      dcl = createChain (0);   /* Allocate a cluster for the new directory table */
      mResult = FR_OK;
      if (dcl == 0)
        mResult = FR_DENIED;    /* No space to allocate a new cluster */
      if (dcl == 1)
        mResult = FR_INT_ERR;
      if (dcl == 0xFFFFFFFF)
        mResult = FR_DISK_ERR;

      if (mResult == FR_OK)  /* Flush FAT */
        mResult = syncWindow();

      if (mResult == FR_OK) {
        //{{{  Initialize the new directory table */
        dsc = clusterToSector (dcl);
        dir = mWindowBuffer;
        memset (dir, 0, SECTOR_SIZE);
        memset (dir + DIR_Name, ' ', 11); /* Create "." entry */
        dir[DIR_Name] = '.';
        dir[DIR_Attr] = AM_DIR;
        ST_DWORD (dir + DIR_WrtTime, tm);
        storeCluster (dir, dcl);
        memcpy (dir + SZ_DIRE, dir, SZ_DIRE);   /* Create ".." entry */

        dir[SZ_DIRE + 1] = '.';
        pcl = directory.mStartCluster;
        if (mFsType == FS_FAT32 && pcl == mDirBase)
          pcl = 0;
        storeCluster (dir + SZ_DIRE, pcl);
        for (n = mSectorsPerCluster; n; n--) {  /* Write dot entries and clear following sectors */
          mWindowSector = dsc++;
          mWindowFlag = 1;
          mResult = syncWindow();
          if (!mResult == FR_OK)
            break;
          memset (dir, 0, SECTOR_SIZE);
          }
        }
        //}}}

      if (mResult == FR_OK)
        mResult = directory.registerNewEntry();
      if (!mResult == FR_OK)
        removeChain (dcl);     // Could not register, remove cluster chain
      else {
        dir = directory.mDirShortFileName;
        dir[DIR_Attr] = AM_DIR;            // Attribute
        ST_DWORD (dir + DIR_WrtTime, tm);  // Created time
        storeCluster (dir, dcl);           // Table start cluster
        mWindowFlag = 1;
        mResult = syncFs();
        }
      }
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::rename (const char* path_old, const char* path_new) {

  BYTE buf[21], *dir;
  DWORD dw;

  cDirectory oldDirectory;
  mResult = findVolume (&oldDirectory.mFatFs, 1);
  if (mResult == FR_OK) {
    cDirectory newDirectory;
    newDirectory.mFatFs = oldDirectory.mFatFs;

    WCHAR longFileName [(MAX_LFN + 1) * 2];
    oldDirectory.mLongFileName = longFileName;
    if (oldDirectory.followPath (path_old) && (oldDirectory.mShortFileName[NSFLAG] & NS_DOT))
      mResult = FR_INVALID_NAME;
    if (mResult == FR_OK)
      mResult = checkFileLock (&oldDirectory, 2);
    if (mResult == FR_OK) {
      // old object is found
      if (!oldDirectory.mDirShortFileName) // root dir?
        mResult = FR_NO_FILE;
      else {
        // Save information about object except name
        memcpy (buf, oldDirectory.mDirShortFileName + DIR_Attr, 21);
        // Duplicate the directory object
        memcpy (&newDirectory, &oldDirectory, sizeof (cDirectory));
        if (newDirectory.followPath (path_new)) // new object name already exists
          mResult = FR_EXIST;
        if (mResult == FR_NO_FILE) {
          // valid path, no name collision
          mResult = newDirectory.registerNewEntry();
          if (mResult == FR_OK) {

  // Start of critical section where any interruption can cause a cross-link
            // Copy information about object except name
            dir = newDirectory.mDirShortFileName;
            memcpy (dir + 13, buf + 2, 19);
            dir[DIR_Attr] = buf[0] | AM_ARC;
            mWindowFlag = 1;
            if ((dir[DIR_Attr] & AM_DIR) && oldDirectory.mStartCluster != newDirectory.mStartCluster) {
              // Update .. entry in the sub-directory if needed
              dw = clusterToSector (loadCluster (dir));
              if (!dw)
                mResult = FR_INT_ERR;
              else {
                mResult = moveWindow (dw);
                dir = mWindowBuffer + SZ_DIRE * 1; // Ptr to .. entry
                if ((mResult == FR_OK) && dir[1] == '.') {
                  storeCluster (dir, newDirectory.mStartCluster);
                  mWindowFlag = 1;
                  }
                }
              }

            if (mResult == FR_OK) {
              mResult = oldDirectory.remove();
              if (mResult == FR_OK)
                mResult = syncFs();
              }
  // End of critical section
            }
          }
        }
      }
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::changeAttributes (const char* path, BYTE attr, BYTE mask) {

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 1);
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    if (directory.followPath (path) && (directory.mShortFileName[NSFLAG] & NS_DOT))
      mResult = FR_INVALID_NAME;

    if (mResult == FR_OK) {
      BYTE* dir = directory.mDirShortFileName;
      if (!dir) // root directory
        mResult = FR_INVALID_NAME;
      else {
        // File or sub directory
        mask &= AM_RDO | AM_HID | AM_SYS | AM_ARC;                      // Valid attribute mask
        dir[DIR_Attr] = (attr & mask) | (dir[DIR_Attr] & (BYTE)~mask);  // Apply attribute change
        mWindowFlag = 1;
        mResult = syncFs();
        }
      }
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::stat (const char* path, cFileInfo& fileInfo) {

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 0);
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    if (directory.followPath (path)) {
      if (directory.mDirShortFileName)
        directory.getFileInfo (fileInfo);
      else // root directory
        mResult = FR_INVALID_NAME;
      }
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::utime (const char* path, const cFileInfo& fileInfo) {

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 1);
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    if (directory.followPath (path) && (directory.mShortFileName[NSFLAG] & NS_DOT))
      mResult = FR_INVALID_NAME;

    if (mResult == FR_OK) {
      BYTE* dir = directory.mDirShortFileName;
      if (!dir) // root dir
        mResult = FR_INVALID_NAME;
      else {
        // file or subDir
        ST_WORD (dir + DIR_WrtTime, fileInfo.mTime);
        ST_WORD (dir + DIR_WrtDate, fileInfo.mDate);
        mWindowFlag = 1;
        mResult = syncFs();
        }
      }
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::unlink (const char* path) {

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 1);
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    if (directory.followPath (path) && (directory.mShortFileName[NSFLAG] & NS_DOT)) // Cannot remove dot entry
      mResult = FR_INVALID_NAME;
    if (mResult == FR_OK)  // Cannot remove open object
      mResult = checkFileLock (&directory, 2);

    if (mResult == FR_OK) {
      // The object is accessible
      BYTE* dir = directory.mDirShortFileName;
      if (!dir) // Cannot remove the origin directory
        mResult = FR_INVALID_NAME;
      else if (dir[DIR_Attr] & AM_RDO) // Cannot remove R/O object
        mResult = FR_DENIED;

      DWORD dclst = 0;
      if (mResult == FR_OK) {
        dclst = loadCluster (dir);
        if (dclst && (dir[DIR_Attr] & AM_DIR)) {
          if (dclst == mCurDirSector) // current directory
            mResult = FR_DENIED;
          else {
            // open subDirectory
            cDirectory subDirectory;
            memcpy (&subDirectory, &directory, sizeof (cDirectory));
            subDirectory.mStartCluster = dclst;
            mResult = subDirectory.setIndex (2);
            if (mResult == FR_OK) {
              mResult = subDirectory.read (0);
              if (mResult == FR_OK)
                mResult = FR_DENIED;  // Not empty (cannot remove)
              if (mResult == FR_NO_FILE)
                mResult = FR_OK; // Empty (can remove)
              }
            }
          }
        }

      if (mResult == FR_OK) {
        mResult = directory.remove();  // Remove the directory entry
        if ((mResult == FR_OK) && dclst) // Remove the cluster chain if exist
          mResult = removeChain (dclst);
        if (mResult == FR_OK)
          mResult = syncFs();
        }
      }
    }

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::setLabel (const char* label) {

  cDirectory directory;
  mResult = findVolume (&directory.mFatFs, 1);
  if (!mResult == FR_OK) {
    unlock (mResult);
    return mResult;
    }

  // Create a volume label in directory form
  BYTE volumeName[11];
  volumeName[0] = 0;

  // Get name length, Remove trailing spaces
  UINT nameSize;
  for (nameSize = 0; label[nameSize]; nameSize++) ;
  for ( ; nameSize && label[nameSize - 1] == ' '; nameSize--) ;

  if (nameSize) {
    // Create volume label in directory form
    UINT i = 0;
    UINT j = 0;
    do {
      WCHAR w = (BYTE)label[i++];
      w = convertToFromUnicode (wideToUpperCase (convertToFromUnicode(w, 1)), 0);
      if (!w || strchr ("\"*+,.:;<=>\?[]|\x7F", w) || j >= (UINT)((w >= 0x100) ? 10 : 11)) {
     //{{{  reject invalid characters for volume label
     unlock (FR_INVALID_NAME);
     return FR_INVALID_NAME;
     }
     //}}}
      if (w >= 0x100)
        volumeName[j++] = (BYTE)(w >> 8);
      volumeName[j++] = (BYTE)w;
      } while (i < nameSize);

    // Fill remaining name field
    while (j < 11) volumeName[j++] = ' ';
    if (volumeName[0] == DDEM) {
      // Reject illegal name (heading DDEM)
      unlock (FR_INVALID_NAME);
      return FR_INVALID_NAME;
      }
    }

  // Open root directory
  directory.mStartCluster = 0;
  mResult = directory.setIndex (0);
  if (mResult == FR_OK) {
    //{{{  Get an entry with AM_VOL
    mResult = directory.read (1);
    if (mResult == FR_OK) {
      //  volume label found
      if (volumeName[0]) {
        // change volume label name
        memcpy (directory.mDirShortFileName, volumeName, 11);
        DWORD tm = getFatTime();
        ST_DWORD (directory.mDirShortFileName + DIR_WrtTime, tm);
        }
      else // Remove the volume label
        directory.mDirShortFileName[0] = DDEM;

      mWindowFlag = 1;
      mResult = syncFs();
      }

    else {
      // no volume label found or error
      if (mResult == FR_NO_FILE) {
        mResult = FR_OK;

        if (volumeName[0]) {
          // create volume label as new, Allocate an entry for volume label
          mResult = directory.allocate (1);
          if (mResult == FR_OK) {
            // set volume label
            memset (directory.mDirShortFileName, 0, SZ_DIRE);
            memcpy (directory.mDirShortFileName, volumeName, 11);
            directory.mDirShortFileName[DIR_Attr] = AM_VOL;
            DWORD tm = getFatTime();
            ST_DWORD (directory.mDirShortFileName + DIR_WrtTime, tm);
            mWindowFlag = 1;
            mResult = syncFs();
            }
          }
        }
      }
    }
    //}}}

  unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFatFs::makeFileSystem (BYTE sfd, UINT au) {

  UINT i;
  BYTE fmt, md, sys, *tbl;
  DWORD n_clst, vs, n, wsect;
  DWORD b_vol, b_fat, b_dir, b_data;  // LBA
  DWORD n_vol, n_rsv, n_fat, n_dir;   // Size

#if USE_TRIM
  DWORD eb[2];
#endif

  if (sfd > 1)
    return FR_INVALID_PARAMETER;

  mFsType = 0;

  // Get disk statics
  DSTATUS stat = diskInitialize();
  if (stat & STA_NOINIT)
    return FR_NOT_READY;
  if (stat & STA_PROTECT)
    return FR_WRITE_PROTECTED;

  // Create a partition in this function
  if (diskIoctl (GET_SECTOR_COUNT, &n_vol) != RES_OK || n_vol < 128)
    return FR_DISK_ERR;
  b_vol = (sfd) ? 0 : 63; // Volume start sector
  n_vol -= b_vol;         // Volume size

  if (au & (au - 1))
    au = 0;
  if (!au) {
    // AU auto selection
    vs = n_vol / 2000;
    for (i = 0; vs < kVst[i]; i++) ;
    au = kCst[i];
    }

  // Number of sectors per cluster
  au /= SECTOR_SIZE;
  if (!au)
    au = 1;
  if (au > 128)
    au = 128;

  // Pre-compute number of clusters and FAT sub-type
  n_clst = n_vol / au;
  fmt = FS_FAT12;
  if (n_clst >= MIN_FAT16)
    fmt = FS_FAT16;
  if (n_clst >= MIN_FAT32)
    fmt = FS_FAT32;

  // Determine offset and size of FAT structure
  if (fmt == FS_FAT32) {
    n_fat = ((n_clst * 4) + 8 + SECTOR_SIZE - 1) / SECTOR_SIZE;
    n_rsv = 32;
    n_dir = 0;
    }
  else {
    n_fat = (fmt == FS_FAT12) ? (n_clst * 3 + 1) / 2 + 3 : (n_clst * 2) + 4;
    n_fat = (n_fat + SECTOR_SIZE - 1) / SECTOR_SIZE;
    n_rsv = 1;
    n_dir = (DWORD)N_ROOTDIR * SZ_DIRE / SECTOR_SIZE;
    }

  b_fat = b_vol + n_rsv;           // FAT area start sector
  b_dir = b_fat + n_fat * N_FATS;  // Directory area start sector
  b_data = b_dir + n_dir;          // Data area start sector
  if (n_vol < b_data + au - b_vol)
    return FR_MKFS_ABORTED;  // Too small volume

  // Align data start sector to erase block boundary (for flash memory media)
  if (diskIoctl (GET_BLOCK_SIZE, &n) != RES_OK || !n || n > 32768)
    n = 1;
  n = (b_data + n - 1) & ~(n - 1);  // Next nearest erase block from current data start
  n = (n - b_data) / N_FATS;
  if (fmt == FS_FAT32) {
    // FAT32: Move FAT offset
    n_rsv += n;
    b_fat += n;
    }
  else // FAT12/16: Expand FAT size
    n_fat += n;

  // Determine number of clusters and final check of validity of the FAT sub-type
  n_clst = (n_vol - n_rsv - n_fat * N_FATS - n_dir) / au;
  if ((fmt == FS_FAT16 && n_clst < MIN_FAT16) || (fmt == FS_FAT32 && n_clst < MIN_FAT32))
    return FR_MKFS_ABORTED;

  // Determine system ID in the partition table
  if (fmt == FS_FAT32)
    sys = 0x0C;   // FAT32X
  else {
    if (fmt == FS_FAT12 && n_vol < 0x10000)
      sys = 0x01; // FAT12(<65536)
    else
      sys = (n_vol < 0x10000) ? 0x04 : 0x06;  // FAT16(<65536) : FAT12/16(>=65536)
    }

  if (sfd)
    // No partition table SFD
    md = 0xF0;
  else {
    // Create partition table FDISK
    memset (mWindowBuffer, 0, SECTOR_SIZE);

    // Create partition table for single partition in the drive
    tbl = mWindowBuffer + MBR_Table;
    tbl[1] = 1;                                // Partition start head
    tbl[2] = 1;                                // Partition start sector
    tbl[3] = 0;                                // Partition start cylinder
    tbl[4] = sys;                              // System type
    tbl[5] = 254;                              // Partition end head

    n = (b_vol + n_vol) / 63 / 255;
    tbl[6] = (BYTE)(n >> 2 | 63);              // Partition end sector
    tbl[7] = (BYTE)n;                          // End cylinder

    ST_DWORD (tbl + 8, 63);                    // Partition start in LBA
    ST_DWORD (tbl + 12, n_vol);                // Partition size in LBA
    ST_WORD (mWindowBuffer + BS_55AA, 0xAA55); // MBR signature

    // Write it to the MBR
    if (diskWrite (mWindowBuffer, 0, 1) != RES_OK)
      return FR_DISK_ERR;
    md = 0xF8;
    }

  // Create BPB in the VBR
  tbl = mWindowBuffer;

  // Clear sector
  memset (tbl, 0, SECTOR_SIZE);
  memcpy (tbl, "\xEB\xFE\x90" "MSDOS5.0", 11); // Boot jump code, OEM name

  i = SECTOR_SIZE;
  ST_WORD (tbl + BPB_BytsPerSec, i);

  tbl[BPB_SecPerClus] = (BYTE)au;        // Sectors per cluster
  ST_WORD (tbl + BPB_RsvdSecCnt, n_rsv); // Reserved sectors
  tbl[BPB_NumFATs] = N_FATS;             // Number of FATs

  i = (fmt == FS_FAT32) ? 0 : N_ROOTDIR; // Number of root directory entries
  ST_WORD (tbl + BPB_RootEntCnt, i);
  if (n_vol < 0x10000) {                 // Number of total sectors
    ST_WORD (tbl + BPB_TotSec16, n_vol);
    }
  else {
    ST_DWORD (tbl + BPB_TotSec32, n_vol);
    }
  tbl[BPB_Media] = md;                   // Media descriptor

  ST_WORD (tbl + BPB_SecPerTrk, 63);     // Number of sectors per track
  ST_WORD (tbl + BPB_NumHeads, 255);     // Number of heads
  ST_DWORD (tbl + BPB_HiddSec, b_vol);   // Hidden sectors

  n = getFatTime();                      // Use current time as VSN
  if (fmt == FS_FAT32) {
    ST_DWORD (tbl + BS_VolID32, n);      // VSN
    ST_DWORD (tbl + BPB_FATSz32, n_fat); // Number of sectors per FAT
    ST_DWORD (tbl + BPB_RootClus, 2);    // Root directory start cluster (2)
    ST_WORD (tbl + BPB_FSInfo, 1);       // FSINFO record offset (VBR + 1)
    ST_WORD (tbl + BPB_BkBootSec, 6);    // Backup boot record offset (VBR + 6)
    tbl[BS_DrvNum32] = 0x80;             // Drive number
    tbl[BS_BootSig32] = 0x29;            // Extended boot signature
    memcpy (tbl + BS_VolLab32, "NO NAME    " "FAT32   ", 19); // Volume label, FAT signature
    }
  else {
    ST_DWORD (tbl + BS_VolID, n);        // VSN
    ST_WORD (tbl + BPB_FATSz16, n_fat);  // Number of sectors per FAT
    tbl[BS_DrvNum] = 0x80;               // Drive number
    tbl[BS_BootSig] = 0x29;              // Extended boot signature
    memcpy (tbl + BS_VolLab, "NO NAME    " "FAT     ", 19); // Volume label, FAT signature
    }

  ST_WORD (tbl + BS_55AA, 0xAA55);        // Signature (Offset is fixed here regardless of sector size)
  if (diskWrite (tbl, b_vol, 1) != RES_OK)  // Write it to the VBR sector
    return FR_DISK_ERR;
  if (fmt == FS_FAT32)                    // Write backup VBR if needed (VBR + 6)
    diskWrite (tbl, b_vol + 6, 1);

  // Initialize FAT area
  wsect = b_fat;
  for (i = 0; i < N_FATS; i++) {
    // Initialize each FAT copy
    memset (tbl, 0, SECTOR_SIZE);     // 1st sector of the FAT
    n = md;                           // Media descriptor byte
    if (fmt != FS_FAT32) {
      n |= (fmt == FS_FAT12) ? 0x00FFFF00 : 0xFFFFFF00;
      ST_DWORD (tbl + 0, n);          // Reserve cluster #0-1 (FAT12/16)
      }
    else {
      n |= 0xFFFFFF00;
      ST_DWORD (tbl + 0, n);          // Reserve cluster #0-1 (FAT32)
      ST_DWORD (tbl + 4, 0xFFFFFFFF);
      ST_DWORD (tbl + 8, 0x0FFFFFFF); // Reserve cluster #2 for root directory
      }

    if (diskWrite (tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;

    // Fill following FAT entries with zero
    memset (tbl, 0, SECTOR_SIZE);

    // This loop may take a time on FAT32 volume due to many single sector writes
    for (n = 1; n < n_fat; n++)
      if (diskWrite (tbl, wsect++, 1) != RES_OK)
        return FR_DISK_ERR;
    }

  // Initialize root directory
  i = (fmt == FS_FAT32) ? au : (UINT)n_dir;
  do {
    if (diskWrite (tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;
    } while (--i);

#if USE_TRIM // Erase data area if needed
  {
    eb[0] = wsect;
    eb[1] = wsect + (n_clst - ((fmt == FS_FAT32) ? 1 : 0)) * au - 1;
    diskIoctl (vol, CTRL_TRIM, eb);
  }
#endif

  // Create FSINFO if needed
  if (fmt == FS_FAT32) {
    ST_DWORD (tbl + FSI_LeadSig, 0x41615252);
    ST_DWORD (tbl + FSI_StrucSig, 0x61417272);
    ST_DWORD (tbl + FSI_Free_Count, n_clst - 1); // Number of free clusters
    ST_DWORD (tbl + FSI_Nxt_Free, 2);            // Last allocated cluster#
    ST_WORD (tbl + BS_55AA, 0xAA55);
    diskWrite (tbl, b_vol + 1, 1);               // Write original (VBR + 1)
    diskWrite (tbl, b_vol + 7, 1);               // Write backup (VBR + 7)
    }

  return (diskIoctl (CTRL_SYNC, 0) == RES_OK) ? FR_OK : FR_DISK_ERR;
  }
//}}}
//{{{  cFatFs private members
//{{{
cFatFs::cFatFs() {

  osMutexDef (fatfs);
  mMutex = osMutexCreate (osMutex (fatfs));

  mWindowBuffer = (BYTE*)malloc (SECTOR_SIZE);
  }
//}}}

//{{{
FRESULT cFatFs::findVolume (cFatFs** fs, BYTE wmode) {

  BYTE fmt, *pt;
  DWORD bsect, fasize, tsect, sysect, nclst, szbfat, br[4];
  WORD nrsv;
  UINT i;

  // lock access to file system
  if (!lock()) {
    *fs = 0;
    return FR_TIMEOUT;
    }
  *fs = this;

  if (mFsType) {
    // check volume mounted
    DSTATUS status = diskStatus();
    if (!(status & STA_NOINIT)) {
      // and the physical drive is kept initialized
      if (wmode && (status & STA_PROTECT)) // Check write protection if needed
        return FR_WRITE_PROTECTED;
      return FR_OK;
      }
    }

  // mount volume, analyze BPB and initialize the fs
  int vol = 0;
  mFsType = 0;

  // init physical drive
  DSTATUS status = diskInitialize();
  if (status & STA_NOINIT)
    return FR_NOT_READY;
  if (wmode && (status & STA_PROTECT))
    return FR_WRITE_PROTECTED;

  // find FAT partition, supports only generic partitioning, FDISK and SFD
  bsect = 0;
  fmt = checkFs (bsect); // Load sector 0 and check if it is an FAT boot sector as SFD
  if (fmt == 1 || (!fmt && vol)) {
    // Not an FAT boot sector or forced partition number
    for (i = 0; i < 4; i++) {
      // Get partition offset
      pt = mWindowBuffer + MBR_Table + i * SZ_PTE;
      br[i] = pt[4] ? LD_DWORD (&pt[8]) : 0;
      }

    // Partition number: 0:auto, 1-4:forced
    i = vol;
    if (i)
      i--;
    do {
      // Find a FAT volume
      bsect = br[i];
      fmt = bsect ? checkFs (bsect) : 2; // Check the partition
      } while (!vol && fmt && ++i < 4);
    }

  if (fmt == 3)
    return FR_DISK_ERR; // An error occured in the disk I/O layer
  if (fmt)
    return FR_NO_FILESYSTEM; // No FAT volume is found

  // FAT volume found, code initializes the file system
  if (LD_WORD (mWindowBuffer + BPB_BytsPerSec) != SECTOR_SIZE) // (BPB_BytsPerSec must be equal to the physical sector size)
    return FR_NO_FILESYSTEM;

  // Number of sectors per FAT
  fasize = LD_WORD (mWindowBuffer + BPB_FATSz16);
  if (!fasize)
    fasize = LD_DWORD (mWindowBuffer + BPB_FATSz32);
  mSectorsPerFAT = fasize;

  // Number of FAT copies
  mNumFatCopies = mWindowBuffer[BPB_NumFATs];
  if (mNumFatCopies != 1 && mNumFatCopies != 2) // (Must be 1 or 2)
    return FR_NO_FILESYSTEM;
  fasize *= mNumFatCopies; // Number of sectors for FAT area

  // Number of sectors per cluster
  mSectorsPerCluster = mWindowBuffer[BPB_SecPerClus];
  if (!mSectorsPerCluster || (mSectorsPerCluster & (mSectorsPerCluster - 1))) // (Must be power of 2)
    return FR_NO_FILESYSTEM;

  // Number of root directory entries
  mNumRootdir = LD_WORD (mWindowBuffer + BPB_RootEntCnt);
  if (mNumRootdir % (SECTOR_SIZE / SZ_DIRE)) // (Must be sector aligned)
    return FR_NO_FILESYSTEM;

  // Number of sectors on the volume
  tsect = LD_WORD (mWindowBuffer + BPB_TotSec16);
  if (!tsect)
    tsect = LD_DWORD (mWindowBuffer + BPB_TotSec32);

  // Number of reserved sectors
  nrsv = LD_WORD (mWindowBuffer + BPB_RsvdSecCnt);
  if (!nrsv)
    return FR_NO_FILESYSTEM; // Must not be 0

  // Determine the FAT sub type
  sysect = nrsv + fasize + mNumRootdir / (SECTOR_SIZE / SZ_DIRE); // RSV + FAT + DIR
  if (tsect < sysect)
    return FR_NO_FILESYSTEM; // Invalid volume size

  // Number of clusters
  nclst = (tsect - sysect) / mSectorsPerCluster;
  if (!nclst)
    return FR_NO_FILESYSTEM; // Invalid volume size
  fmt = FS_FAT12;
  if (nclst >= MIN_FAT16)
    fmt = FS_FAT16;
  if (nclst >= MIN_FAT32)
    fmt = FS_FAT32;

  // Boundaries and Limits
  mNumFatEntries = nclst + 2; // Number of FAT entries
  mVolBase = bsect;           // Volume start sector
  mFatBase = bsect + nrsv;    // FAT start sector
  mDataBase = bsect + sysect; // Data start sector
  if (fmt == FS_FAT32) {
    if (mNumRootdir)
      return FR_NO_FILESYSTEM;       // (BPB_RootEntCnt must be 0)
    mDirBase = LD_DWORD (mWindowBuffer + BPB_RootClus);  // Root directory start cluster
    szbfat = mNumFatEntries * 4;   // (Needed FAT size)
    }
  else {
    if (!mNumRootdir)
      return FR_NO_FILESYSTEM;  // (BPB_RootEntCnt must not be 0)
    mDirBase = mFatBase + fasize;  // Root directory start sector
    szbfat = (fmt == FS_FAT16) ? mNumFatEntries * 2 : mNumFatEntries * 3 / 2 + (mNumFatEntries & 1);  /* (Needed FAT size) */
    }

  // (BPB_FATSz must not be less than the size needed)
  if (mSectorsPerFAT < (szbfat + (SECTOR_SIZE - 1)) / SECTOR_SIZE)
    return FR_NO_FILESYSTEM;

  // Initialize cluster allocation information
  mLastCluster = mFreeClusters = 0xFFFFFFFF;

  // Get fsinfo if available
  mFsiFlag = 0x80;
  if (fmt == FS_FAT32 && LD_WORD (mWindowBuffer + BPB_FSInfo) == 1 && moveWindow (bsect + 1) == FR_OK) {
    mFsiFlag = 0;
    if (LD_WORD (mWindowBuffer + BS_55AA) == 0xAA55 &&
        LD_DWORD (mWindowBuffer + FSI_LeadSig) == 0x41615252 &&
        LD_DWORD (mWindowBuffer + FSI_StrucSig) == 0x61417272) {
      mFreeClusters = LD_DWORD (mWindowBuffer + FSI_Free_Count);
      mLastCluster = LD_DWORD (mWindowBuffer + FSI_Nxt_Free);
      }
    }

  // FAT sub-type
  mFsType = fmt;

  // Set current directory to root
  mCurDirSector = 0;
  clearFileLock();

  return FR_OK;
  }
//}}}

//{{{
FRESULT cFatFs::syncWindow() {

  FRESULT result = FR_OK;

  if (mWindowFlag) {
    // Write back the sector if it is dirty
    DWORD wsect = mWindowSector;  /* Current sector number */
    if (diskWrite (mWindowBuffer, wsect, 1) != RES_OK)
      result = FR_DISK_ERR;
    else {
      mWindowFlag = 0;
      if (wsect - mFatBase < mSectorsPerFAT) {
        // Is it in the FAT area? */
        for (UINT nf = mNumFatCopies; nf >= 2; nf--) {
          // Reflect the change to all FAT copies */
          wsect += mSectorsPerFAT;
          diskWrite (mWindowBuffer, wsect, 1);
          }
        }
      }
    }

  return result;
  }
//}}}
//{{{
FRESULT cFatFs::moveWindow (DWORD sector) {

  FRESULT result = FR_OK;
  if (sector != mWindowSector) {
    /* Window offset changed?  Write-back changes */
    result = syncWindow();
    if (result == FR_OK) {
      /* Fill sector window with new data */
      if (diskRead (mWindowBuffer, sector, 1) != RES_OK) {
        /* Invalidate window if data is not reliable */
        sector = 0xFFFFFFFF;
        result = FR_DISK_ERR;
        }
      mWindowSector = sector;
      }
    }

  return result;
  }
//}}}

//{{{
BYTE cFatFs::checkFs (DWORD sector) {

  // Invalidate window
  mWindowFlag = 0;
  mWindowSector = 0xFFFFFFFF;

  // Load boot record
  if (moveWindow (sector) != FR_OK)
    return 3;

  // Check boot record signature (always placed at offset 510 even if the sector size is >512)
  if (LD_WORD (&mWindowBuffer[BS_55AA]) != 0xAA55)
    return 2;

  // Check "FAT" string
  if ((LD_DWORD (&mWindowBuffer[BS_FilSysType]) & 0xFFFFFF) == 0x544146)
    return 0;

  // Check "FAT" string
  if ((LD_DWORD (&mWindowBuffer[BS_FilSysType32]) & 0xFFFFFF) == 0x544146)
    return 0;

  return 1;
  }
//}}}
//{{{
FRESULT cFatFs::syncFs() {

  FRESULT result = syncWindow();
  if (result == FR_OK) {
    /* Update FSINFO sector if needed */
    if (mFsType == FS_FAT32 && mFsiFlag == 1) {
      /* Create FSINFO structure */
      memset (mWindowBuffer, 0, SECTOR_SIZE);
      ST_WORD (mWindowBuffer + BS_55AA, 0xAA55);
      ST_DWORD (mWindowBuffer + FSI_LeadSig, 0x41615252);
      ST_DWORD (mWindowBuffer + FSI_StrucSig, 0x61417272);
      ST_DWORD (mWindowBuffer + FSI_Free_Count, mFreeClusters);
      ST_DWORD (mWindowBuffer + FSI_Nxt_Free, mLastCluster);
      /* Write it into the FSINFO sector */
      mWindowSector = mVolBase + 1;
      diskWrite (mWindowBuffer, mWindowSector, 1);
      mFsiFlag = 0;
      }

    /* Make sure that no pending write process in the physical drive */
    if (diskIoctl (CTRL_SYNC, 0) != RES_OK)
      result = FR_DISK_ERR;
    }

  return result;
  }
//}}}

//{{{
DWORD cFatFs::getFat (DWORD cluster) {

  UINT wc, bc;
  BYTE *p;
  DWORD val;

  if (cluster < 2 || cluster >= mNumFatEntries)  /* Check range */
    val = 1;  /* Internal error */
  else {
    val = 0xFFFFFFFF; /* Default value falls on disk error */

    switch (mFsType) {
      case FS_FAT12 :
        bc = (UINT)cluster; bc += bc / 2;
        if (moveWindow (mFatBase + (bc / SECTOR_SIZE)) != FR_OK)
          break;

        wc = mWindowBuffer[bc++ % SECTOR_SIZE];
        if (moveWindow (mFatBase + (bc / SECTOR_SIZE)) != FR_OK)
          break;

        wc |= mWindowBuffer[bc % SECTOR_SIZE] << 8;
        val = cluster & 1 ? wc >> 4 : (wc & 0xFFF);
        break;

      case FS_FAT16 :
        if (moveWindow (mFatBase + (cluster / (SECTOR_SIZE / 2))) != FR_OK)
          break;
        p = &mWindowBuffer[cluster * 2 % SECTOR_SIZE];
        val = LD_WORD (p);
        break;

      case FS_FAT32 :
        if (moveWindow (mFatBase + (cluster / (SECTOR_SIZE / 4))) != FR_OK)
          break;
        p = &mWindowBuffer[cluster * 4 % SECTOR_SIZE];
        val = LD_DWORD (p) & 0x0FFFFFFF;
        break;

      default:
        val = 1;  /* Internal error */
      }
    }

  return val;
  }
//}}}
//{{{
FRESULT cFatFs::putFat (DWORD cluster, DWORD val) {

  UINT bc;
  BYTE *p;
  FRESULT result;

  if (cluster < 2 || cluster >= mNumFatEntries)  /* Check range */
    result = FR_INT_ERR;
  else {
    switch (mFsType) {
      case FS_FAT12 :
        bc = (UINT)cluster; bc += bc / 2;
        result = moveWindow (mFatBase + (bc / SECTOR_SIZE));
        if (result != FR_OK)
          break;
        p = &mWindowBuffer[bc++ % SECTOR_SIZE];
        *p = (cluster & 1) ? ((*p & 0x0F) | ((BYTE)val << 4)) : (BYTE)val;
        mWindowFlag = 1;
        result = moveWindow (mFatBase + (bc / SECTOR_SIZE));
        if (result != FR_OK)
          break;
        p = &mWindowBuffer[bc % SECTOR_SIZE];
        *p = (cluster & 1) ? (BYTE)(val >> 4) : ((*p & 0xF0) | ((BYTE)(val >> 8) & 0x0F));
        mWindowFlag = 1;
        break;

      case FS_FAT16 :
        result = moveWindow (mFatBase + (cluster / (SECTOR_SIZE / 2)));
        if (result != FR_OK)
          break;
        p = &mWindowBuffer[cluster * 2 % SECTOR_SIZE];
        ST_WORD (p, (WORD)val);
        mWindowFlag = 1;
        break;

      case FS_FAT32 :
        result = moveWindow (mFatBase + (cluster / (SECTOR_SIZE / 4)));
        if (result != FR_OK)
          break;
        p = &mWindowBuffer[cluster * 4 % SECTOR_SIZE];
        val |= LD_DWORD (p) & 0xF0000000;
        ST_DWORD (p, val);
        mWindowFlag = 1;
        break;

      default :
        result = FR_INT_ERR;
      }
    }

  return result;
  }
//}}}

//{{{
DWORD cFatFs::clusterToSector (DWORD cluster) {

  cluster -= 2;
  return (cluster >= mNumFatEntries - 2) ? 0 : cluster * mSectorsPerCluster + mDataBase;
  }
//}}}
//{{{
DWORD cFatFs::loadCluster (BYTE* dir) {

  DWORD cluster = LD_WORD (dir + DIR_FstClusLO);
  if (mFsType == FS_FAT32)
    cluster |= (DWORD)LD_WORD (dir + DIR_FstClusHI) << 16;

  return cluster;
  }
//}}}

//{{{
DWORD cFatFs::createChain (DWORD cluster) {

  DWORD scl;
  if (cluster == 0) {
    // Create a new chain
    scl = mLastCluster;  // Get suggested start point
    if (!scl || scl >= mNumFatEntries)
      scl = 1;
    }
  else {
    // Stretch the current chain, Check the cluster status
    DWORD cs = getFat (cluster);
    if (cs < 2)  // Invalid value
      return 1;
    if (cs == 0xFFFFFFFF) // A disk error occurred
      return cs;
    if (cs < mNumFatEntries) // It is already followed by next cluster
      return cs;
    scl = cluster;
    }

 // Start cluster
 DWORD ncl = scl;
  for (;;) {
    // Next cluster
    ncl++;
    if (ncl >= mNumFatEntries) {
      // Check wrap around
      ncl = 2;
      if (ncl > scl)
        return 0;  // No free cluster
      }

    // Get the cluster status
    DWORD cs = getFat (ncl);
    if (cs == 0) // Found a free cluster
      break;
    if (cs == 0xFFFFFFFF || cs == 1)// An error occurred
      return cs;
    if (ncl == scl) // No free cluster
      return 0;
    }

  // Mark the new cluster "last link"
  FRESULT result = putFat (ncl, 0x0FFFFFFF);
  if (result == FR_OK && cluster != 0) // Link it to the previous one if needed
    result = putFat (cluster, ncl);

  if (result == FR_OK) {
    // Update FSINFO
    mLastCluster = ncl;
    if (mFreeClusters != 0xFFFFFFFF) {
      mFreeClusters--;
      mFsiFlag |= 1;
      }
    }
  else
    ncl = (result == FR_DISK_ERR) ? 0xFFFFFFFF : 1;

  // Return new cluster number or error code
  return ncl;
  }
//}}}
//{{{
FRESULT cFatFs::removeChain (DWORD cluster) {

#if USE_TRIM
  DWORD scl = cluster, ecl = cluster, rt[2];
#endif

  FRESULT result;
  if (cluster < 2 || cluster >= mNumFatEntries)
    /* Check range */
    result = FR_INT_ERR;
  else {
    result = FR_OK;
    while (cluster < mNumFatEntries) {
      /* Not a last link? */
      DWORD nxt = getFat (cluster); /* Get cluster status */
      if (nxt == 0)
        break;        /* Empty cluster? */

      if (nxt == 1) {
        /* Internal error? */
        result = FR_INT_ERR;
        break;
        }

      if (nxt == 0xFFFFFFFF) {
        /* Disk error? */
        result = FR_DISK_ERR;
        break;
        }

      result = putFat (cluster, 0);     /* Mark the cluster "empty" */
      if (result != FR_OK) break;
      if (mFreeClusters != 0xFFFFFFFF) {
        /* Update FSINFO */
        mFreeClusters++;
        mFsiFlag |= 1;
        }

#if USE_TRIM
      if (ecl + 1 == nxt)
        /* Is next cluster contiguous? */
        ecl = nxt;
      else {
        /* End of contiguous clusters */
        rt[0] = clust2sect(fs, scl);                           /* Start sector */
        rt[1] = clust2sect(fs, ecl) + mSectorsPerCluster - 1;  /* End sector */
        diskIoctl (CTRL_TRIM, rt);                             /* Erase the block */
        scl = ecl = nxt;
        }
#endif

      cluster = nxt; /* Next cluster */
      }
    }

  return result;
  }
//}}}

//{{{
bool cFatFs::lock() {
// lock fatfs, also sd access lock

  if (osMutexWait (mMutex, 1000) != osOK)
    mResult = FR_TIMEOUT;
  else if (diskStatus() & STA_NOINIT)
    mResult = FR_DISK_ERR;
  else
    mResult = FR_OK;

  return mResult == FR_OK;
  }
//}}}
//{{{
void cFatFs::unlock (FRESULT result) {

  if (result != FR_NOT_ENABLED &&
      result != FR_INVALID_DRIVE &&
      result != FR_INVALID_OBJECT &&
      result != FR_TIMEOUT)
    osMutexRelease (mMutex);
  }
//}}}

//{{{
int cFatFs::enquireFileLock() {

  UINT i;
  for (i = 0; i < FS_LOCK && mFileLock[i].mFatFs; i++) ;
  return (i == FS_LOCK) ? 0 : 1;
  }
//}}}
//{{{
FRESULT cFatFs::checkFileLock (cDirectory* directory, int acc) {

  // Search file semaphore table
  UINT i, be;
  for (i = be = 0; i < FS_LOCK; i++) {
    if (mFileLock[i].mFatFs) {
      // Existing entry
      if (mFileLock[i].mFatFs == directory->mFatFs &&
          mFileLock[i].clu == directory->mStartCluster &&
          mFileLock[i].idx == directory->mIndex)
         break;
      }
    else // Blank entry
      be = 1;
    }

  if (i == FS_LOCK)
    // The object is not opened
    return (be || acc == 2) ? FR_OK : FR_TOO_MANY_OPEN_FILES; // Is there a blank entry for new object

  // The object has been opened. Reject any open against writing file and all write mode open
  return (acc || mFileLock[i].ctr == 0x100) ? FR_LOCKED : FR_OK;
  }
//}}}
//{{{
UINT cFatFs::incFileLock (cDirectory* directory, int acc) {

  UINT i;
  for (i = 0; i < FS_LOCK; i++)
    // Find the object
    if (mFileLock[i].mFatFs == directory->mFatFs &&
        mFileLock[i].clu == directory->mStartCluster &&
        mFileLock[i].idx == directory->mIndex)
      break;

  if (i == FS_LOCK) {
    // Not opened, register it as new
    for (i = 0; i < FS_LOCK && mFileLock[i].mFatFs; i++) ;
    if (i == FS_LOCK)
      return 0;  // No free entry to register (int err)

    mFileLock[i].mFatFs = directory->mFatFs;
    mFileLock[i].clu = directory->mStartCluster;
    mFileLock[i].idx = directory->mIndex;
    mFileLock[i].ctr = 0;
    }

  if (acc && mFileLock[i].ctr) // Access violation (int err)
    return 0;

  // Set semaphore value
  mFileLock[i].ctr = acc ? 0x100 : mFileLock[i].ctr + 1;

  return i + 1;
  }
//}}}
//{{{
FRESULT cFatFs::decFileLock (UINT i) {

  if (--i < FS_LOCK) {
    // Shift index number origin from 0
    WORD n = mFileLock[i].ctr;
    if (n == 0x100)
      n = 0;    // If write mode open, delete the entry
    if (n)
      n--;         // Decrement read mode open count

    mFileLock[i].ctr = n;
    if (!n)
      mFileLock[i].mFatFs = 0;  // Delete the entry if open count gets zero

    return FR_OK;
    }
  else
    return FR_INT_ERR;     // Invalid index nunber
  }
//}}}
//{{{
void cFatFs::clearFileLock() {

  for (UINT i = 0; i < FS_LOCK; i++)
    if (mFileLock[i].mFatFs == this)
      mFileLock[i].mFatFs = 0;
  }
//}}}
//}}}

// cDirectory
//{{{
cDirectory::cDirectory (std::string path) {

  mResult = cFatFs::get()->findVolume (&mFatFs, 0);
  if (mResult == FR_OK) {
    WCHAR longFileName[(MAX_LFN + 1) * 2];
    mLongFileName = longFileName;
    if (followPath (path.c_str())) {
      if (mDirShortFileName) {
        // not itself */
        if (mDirShortFileName[DIR_Attr] & AM_DIR) // subDir
          mStartCluster = mFatFs->loadCluster (mDirShortFileName);
        else // file
          mResult = FR_NO_PATH;
        }

      if (mResult == FR_OK) {
        // Rewind directory
        mResult = setIndex (0);
        if (mResult == FR_OK) {
          if (mStartCluster) {
            // lock subDir
            mLockId = mFatFs->incFileLock (this, 0);
            if (!mLockId)
              mResult = FR_TOO_MANY_OPEN_FILES;
            }
          else // root directory not locked
            mLockId = 0;
          }
        }
      }

    if (mResult == FR_NO_FILE)
      mResult = FR_NO_PATH;
    }

  if (!mResult == FR_OK)
    mFatFs = 0;
  cFatFs::get()->unlock (mResult);
  }
//}}}
//{{{
cDirectory::~cDirectory() {

  // close directory
  if (mFatFs->lock()) {
    if (mLockId)
      // Decrement sub-directory open counter
      mResult = mFatFs->decFileLock (mLockId);

    if (mResult == FR_OK)
      // Invalidate directory object
      mFatFs = 0;

    cFatFs::get()->unlock (FR_OK);
    }
  }
//}}}
//{{{
FRESULT cDirectory::find (cFileInfo& fileInfo) {

  if (mFatFs->lock()) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    mLongFileName = longFileName;
    mResult = read (0);
    if (mResult == FR_NO_FILE) {
      // Reached end of directory
      mSector = 0;
      mResult = FR_OK;
      }

    if (mResult == FR_OK) {
      // valid entry is found, get the object information */
      getFileInfo (fileInfo);

      // Increment index for next */
      mResult = next (0);
      if (mResult == FR_NO_FILE) {
        mSector = 0;
        mResult = FR_OK;
        }
      }
    }

  mFatFs->unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cDirectory::findMatch (cFileInfo& fileInfo, const char* path, const char* pattern) {

  mPattern = pattern;

  mResult = findNext (fileInfo);

  return mResult;
  }
//}}}
//{{{
FRESULT cDirectory::findNext (cFileInfo& fileInfo) {

  for (;;) {
    mResult = find (fileInfo);
    if (!mResult == FR_OK || !fileInfo.mShortFileName[0])
      break;

    // match longFileName
    if (matchPattern (mPattern, fileInfo.mLongFileName, 0, 0))
      break;

    // match shortFileName
    if (matchPattern (mPattern, fileInfo.mShortFileName, 0, 0))
      break;
    }

  return mResult;
  }
//}}}
//{{{  cDirectory private members
//{{{
bool cDirectory::followPath (const char* path) {

  BYTE *dir1, ns;

  if (*path == '/' || *path == '\\') {
    //  There is a heading separator
    path++;
    mStartCluster = 0;       /* Strip it and start from the root directory */
    }
  else
    // No heading separator
    mStartCluster = mFatFs->mCurDirSector;      /* Start from the current directory */

  if ((UINT)*path < ' ') {
    // Null path name is the origin directory itself
    mResult = setIndex (0);
    mDirShortFileName = 0;
    }

  else {
    for (;;) {
      mResult = createName (&path); /* Get a segment name of the path */
      if (!mResult == FR_OK)
        break;
      mResult = find();       /* Find an object with the sagment name */
      ns = mShortFileName[NSFLAG];
      if (!mResult == FR_OK) {
        /* Failed to find the object */
        if (mResult == FR_NO_FILE) {
          /* Object is not found */
          if (ns & NS_DOT) {
            /* If dot entry is not exist, */
            mStartCluster = 0;
            mDirShortFileName = 0;  /* it is the root directory and stay there */
            if (!(ns & NS_LAST))
              continue;  /* Continue to follow if not last segment */
            mResult = FR_OK;  /* Ended at the root directroy. Function completed. */
            }
          else if (!(ns & NS_LAST))
            mResult = FR_NO_PATH;  /* Adirectoryust error code if not last segment */
          }
        break;
        }

      if (ns & NS_LAST)
        break;      /* Last segment matched. Function completed. */

      dir1 = mDirShortFileName;
      if (!(dir1[DIR_Attr] & AM_DIR)) {
        /* It is not a sub-directory and cannot follow */
        mResult = FR_NO_PATH;
        break;
        }

      mStartCluster = mFatFs->loadCluster(dir1);
      }
    }

  return mResult == FR_OK;
  }
//}}}

//{{{
FRESULT cDirectory::createName (const char** path) {

  BYTE b, cf;
  UINT i, ni;

  // Create longFileName in Unicode
  const char* p;
  for (p = *path; *p == '/' || *p == '\\'; p++) ; /* Strip duplicated separator */

  WCHAR* longFileName = mLongFileName;
  UINT si = 0;
  UINT di = 0;
  WCHAR w;

  for (;;) {
    w = p[si++]; // Get a character
    if (w < ' ' || w == '/' || w == '\\')
      break;  // Break on end of segment
    if (di >= MAX_LFN) // Reject too long name
      return FR_INVALID_NAME;

    //* Convert ANSI/OEM to Unicode
    w &= 0xFF;
    w = convertToFromUnicode (w, 1);
    if (!w)
      return FR_INVALID_NAME; // Reject invalid code */

    if (w < 0x80 && strchr ("\"*:<>\?|\x7F", w)) // Reject illegal characters for LFN */
      return FR_INVALID_NAME;
    longFileName[di++] = w;   // Store the Unicode character */
    }

  *path = &p[si];  // Return pointer to the next segment */
  cf = (w < ' ') ? NS_LAST : 0;   // Set last segment flag if end of path */
  if ((di == 1 && longFileName[di - 1] == '.') || (di == 2 && longFileName[di - 1] == '.' && longFileName[di - 2] == '.')) {
    longFileName[di] = 0;
    for (i = 0; i < 11; i++)
      mShortFileName[i] = (i < di) ? '.' : ' ';
    mShortFileName[i] = cf | NS_DOT;   // This is a dot entry */
    return FR_OK;
    }

  while (di) {
    //{{{
    // Strip trailing spaces and dots */
    w = longFileName[di - 1];
    if (w != ' ' && w != '.')
      break;
    di--;
    }
    //}}}
  if (!di)
    return FR_INVALID_NAME;  /* Reject nul string */

  // longFileName is created
  longFileName[di] = 0;

  // Create shortFileName in directory form
  memset (mShortFileName, ' ', 11);
  for (si = 0; longFileName[si] == ' ' || longFileName[si] == '.'; si++) ;  /* Strip leading spaces and dots */
  if (si)
    cf |= NS_LOSS | NS_LFN;
  while (di && longFileName[di - 1] != '.')
    di--;  /* Find extension (di<=si: no extension) */

  b = i = 0; ni = 8;
  for (;;) {
    w = longFileName[si++];  // Get an longFileName character */
    if (!w) // Break on end of the longFileName
      break;
    if (w == ' ' || (w == '.' && si != di)) {
     //{{{  Remove spaces and dots
     cf |= NS_LOSS | NS_LFN;
     continue;
     }
     //}}}
    if (i >= ni || si == di) {
      //{{{  Extension or end of shortFileName
      if (ni == 11) {
        /* Long extension */
        cf |= NS_LOSS | NS_LFN;
        break;
        }
      if (si != di)
        cf |= NS_LOSS | NS_LFN; /* Out of 8.3 format */
      if (si > di)
        break;  // No extension */

      si = di;
      i = 8;
      ni = 11;  // Enter extension section */
      b <<= 2;
      continue;
      }
      //}}}
    if (w >= 0x80) {
      //{{{  Non ASCII character,  Unicode -> OEM code */
      w = convertToFromUnicode (w, 0);
      if (w)
        w = kExCvt[w - 0x80]; /* Convert extended character to upper (SBCS) */

      // Force create LFN entry
      cf |= NS_LFN;
      }
      //}}}
    if (!w || strchr ("+,;=[]", w)) {
      //{{{
      // Replace illegal characters for shortFileName
      w = '_';
      cf |= NS_LOSS | NS_LFN; /* Lossy conversion */
      }
      //}}}
    else if (IS_UPPER (w))
      //{{{  ASCII large capital
      b |= 2;
      //}}}
    else if (IS_LOWER (w)) {
      //{{{  ASCII small capital
      b |= 1;
      w -= 0x20;
      }
      //}}}
    mShortFileName[i++] = (BYTE)w;
    }

  if (mShortFileName[0] == DDEM)
    mShortFileName[0] = RDDEM; /* If the first character collides with deleted mark, replace it with RDDEM */

  if (ni == 8) b <<= 2;
  if ((b & 0x0C) == 0x0C || (b & 0x03) == 0x03)
    // Create LFN entry when there are composite capitals */
    cf |= NS_LFN;
  if (!(cf & NS_LFN)) {
    // When LFN is in 8.3 format without extended character, NT flags are created */
    if ((b & 0x03) == 0x01)
      cf |= NS_EXT; /* NT flag (Extension has only small capital) */
    if ((b & 0x0C) == 0x04)
      cf |= NS_BODY;  /* NT flag (Filename has only small capital) */
    }

  // shortFileName created
  mShortFileName[NSFLAG] = cf;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::setIndex (UINT index) {

  // Current index
  mIndex = (WORD)index;

  // Table start cluster (0:root)
  DWORD cluster = mStartCluster;
  if (cluster == 1 || cluster >= mFatFs->mNumFatEntries)  /* Check start cluster range */
    return FR_INT_ERR;
  if (!cluster && mFatFs->mFsType == FS_FAT32) /* Replace cluster# 0 with root cluster# if in FAT32 */
    cluster = mFatFs->mDirBase;

  DWORD sect1;
  if (cluster == 0) {
    // static table (root-directory in FAT12/16)
    if (index >= mFatFs->mNumRootdir) // index out of range
      return FR_INT_ERR;
    sect1 = mFatFs->mDirBase;
    }
  else {
    // dynamic table (root-directory in FAT32 or sub-directory)
    UINT ic = SECTOR_SIZE / SZ_DIRE * mFatFs->mSectorsPerCluster;  // entries per cluster
    while (index >= ic) {
      // follow cluster chain,  get next cluster
      cluster = mFatFs->getFat (cluster);
      if (cluster == 0xFFFFFFFF)
        return FR_DISK_ERR;
      if (cluster < 2 || cluster >= mFatFs->mNumFatEntries) // reached end of table or internal error
        return FR_INT_ERR;
      index -= ic;
      }
    sect1 = mFatFs->clusterToSector (cluster);
    }

  mCluster = cluster;
  if (!sect1)
    return FR_INT_ERR;

  // Sector# of the directory entry
  mSector = sect1 + index / (SECTOR_SIZE / SZ_DIRE);

  // Ptr to the entry in the sector
  mDirShortFileName = mFatFs->mWindowBuffer + (index % (SECTOR_SIZE / SZ_DIRE)) * SZ_DIRE;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::next (int stretch) {

  UINT i = mIndex + 1;
  if (!(i & 0xFFFF) || !mSector) /* Report EOT when index has reached 65535 */
    return FR_NO_FILE;

  if (!(i % (SECTOR_SIZE / SZ_DIRE))) {
    mSector++;
    if (!mCluster) {
      /* Static table */
      if (i >= mFatFs->mNumRootdir) /* Report EOT if it reached end of static table */
        return FR_NO_FILE;
      }
    else {
      /* Dynamic table */
      if (((i / (SECTOR_SIZE / SZ_DIRE)) & (mFatFs->mSectorsPerCluster - 1)) == 0) {
        /* Cluster changed? */
        DWORD cluster = mFatFs->getFat (mCluster);        /* Get next cluster */
        if (cluster <= 1)
          return FR_INT_ERR;
        if (cluster == 0xFFFFFFFF)
          return FR_DISK_ERR;
        if (cluster >= mFatFs->mNumFatEntries) {
          /* If it reached end of dynamic table, */
          if (!stretch)
            return FR_NO_FILE;      /* If do not stretch, report EOT */
          cluster = mFatFs->createChain (mCluster);   /* Stretch cluster chain */
          if (cluster == 0)
            return FR_DENIED;      /* No free cluster */
          if (cluster == 1)
            return FR_INT_ERR;
          if (cluster == 0xFFFFFFFF)
            return FR_DISK_ERR;

          /* Clean-up stretched table */
          if (mFatFs->syncWindow())
            return FR_DISK_ERR;/* Flush disk access window */

          /* Clear window buffer */
          memset (mFatFs->mWindowBuffer, 0, SECTOR_SIZE);

          /* Cluster start sector */
          mFatFs->mWindowSector = mFatFs->clusterToSector (cluster);

          UINT c;
          for (c = 0; c < mFatFs->mSectorsPerCluster; c++) {
            /* Fill the new cluster with 0 */
            mFatFs->mWindowFlag = 1;
            if (mFatFs->syncWindow())
              return FR_DISK_ERR;
            mFatFs->mWindowSector++;
            }
          mFatFs->mWindowSector -= c;           /* Rewind window offset */
          }

        /* Initialize data for new cluster */
        mCluster = cluster;
        mSector = mFatFs->clusterToSector (cluster);
        }
      }
    }

  /* Current index */
  mIndex = (WORD)i;

  /* Current entry in the window */
  mDirShortFileName = mFatFs->mWindowBuffer + (i % (SECTOR_SIZE / SZ_DIRE)) * SZ_DIRE;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::registerNewEntry() {

  UINT n, nent;

  WCHAR* longFileName = mLongFileName;
  BYTE sn[12];
  memcpy (sn, mShortFileName, 12);

  if (sn[NSFLAG] & NS_DOT)   /* Cannot create dot entry */
    return FR_INVALID_NAME;

  if (sn[NSFLAG] & NS_LOSS) {
    // When longFileName is out of 8.3 format, generate a numbered name
    mShortFileName[NSFLAG] = 0;

    // Find only shortFileName
    mLongFileName = 0;
    for (n = 1; n < 100; n++) {
      generateNumberedName (mShortFileName, sn, longFileName, n);

      // Check if the name collides with existing shortFileName
      mResult = find();
      if (!mResult == FR_OK)
        break;
      }
    if (n == 100)
      return FR_DENIED;   /* Abort if too many collisions */
    if (mResult != FR_NO_FILE)
      return mResult;  /* Abort if the mResult is other than 'not collided' */

    mShortFileName[NSFLAG] = sn[NSFLAG];
    mLongFileName = longFileName;
    }

  if (sn[NSFLAG] & NS_LFN) {
    // When longFileName is to be created, allocate entries for an shortFileName + longFileNames
    for (n = 0; longFileName[n]; n++) ;
    nent = (n + 25) / 13;
    }
  else // Otherwise allocate an entry for an shortFileName
    nent = 1;

  mResult = allocate (nent);
  if (mResult == FR_OK && --nent) {
    /* Set longFileName entry if needed */
    mResult = setIndex (mIndex - nent);
    if (mResult == FR_OK) {
      BYTE sum = sumShortFileName (mShortFileName);
      do {
        // Store longFileName entries in bottom first
        mResult = mFatFs->moveWindow (mSector);
        if (!mResult == FR_OK)
          break;
        fitLongFileName (mLongFileName, mDirShortFileName, (BYTE)nent, sum);
        mFatFs->mWindowFlag = 1;
        mResult = next (0);
        } while ((mResult == FR_OK) && --nent);
      }
    }

  if (mResult == FR_OK) {
    // Set shortFileName entry
    mResult = mFatFs->moveWindow (mSector);
    if (mResult == FR_OK) {
      memset (mDirShortFileName, 0, SZ_DIRE);  // Clean the entry
      memcpy (mDirShortFileName, mShortFileName, 11);      // Put shortFileName
      mDirShortFileName[DIR_NTres] = mShortFileName[NSFLAG] & (NS_BODY | NS_EXT); // Put NT flag
      mFatFs->mWindowFlag = 1;
      }
    }

  return mResult;
  }
//}}}
//{{{
FRESULT cDirectory::allocate (UINT nent) {

  UINT n;
  mResult = setIndex (0);
  if (mResult == FR_OK) {
    n = 0;
    do {
      mResult = mFatFs->moveWindow (mSector);
      if (!mResult == FR_OK)
        break;
      if (mDirShortFileName[0] == DDEM || mDirShortFileName[0] == 0) {
        // free entry
        if (++n == nent)
          break; /* A block of contiguous free entries is found */
        }
      else
        n = 0;          /* Not a blank entry. Restart to search */
      mResult = next (1);    /* Next entry with table stretch enabled */
      } while (mResult == FR_OK);
    }

  if (mResult == FR_NO_FILE)
    mResult = FR_DENIED; /* No directory entry to allocate */

  return mResult;
  }
//}}}
//{{{
FRESULT cDirectory::find() {

  BYTE c, *dir1;
  BYTE a, sum;

  mResult = setIndex (0);     /* Rewind directory object */
  if (!mResult == FR_OK)
    return mResult;

  BYTE ord = sum = 0xFF;
  mLongFileNameIndex = 0xFFFF; /* Reset longFileName sequence */
  do {
    mResult = mFatFs->moveWindow (mSector);
    if (!mResult == FR_OK)
      break;

    dir1 = mDirShortFileName;          /* Ptr to the directory entry of current index */
    c = dir1[DIR_Name];
    if (c == 0) {
      mResult = FR_NO_FILE;
      break;
      }  /* Reached to end of table */

    a = dir1[DIR_Attr] & AM_MASK;
    if (c == DDEM || ((a & AM_VOL) && a != AM_LFN)) { /* An entry without valid data */
      ord = 0xFF;
      mLongFileNameIndex = 0xFFFF; /* Reset longFileName sequence */
      }
    else {
      if (a == AM_LFN) {      /* An LFN entry is found */
        if (mLongFileName) {
          if (c & LLEF) {   /* Is it start of LFN sequence? */
            sum = dir1[LDIR_Chksum];
            c &= ~LLEF;
            ord = c;  /* longFileName start order */
            mLongFileNameIndex = mIndex;  /* Start index of longFileName */
            }
          /* Check validity of the longFileName entry and compare it with given name */
          ord = (c == ord && sum == dir1[LDIR_Chksum] && compareLongFileName (mLongFileName, dir1)) ? ord - 1 : 0xFF;
          }
        }
      else {
        /* An shortFileName entry is found */
        if (!ord && sum == sumShortFileName (dir1))
          break; /* longFileName matched? */
        if (!(mShortFileName[NSFLAG] & NS_LOSS) && !memcmp (dir1, mShortFileName, 11))
          break;  /* shortFileName matched? */
        ord = 0xFF;
        mLongFileNameIndex = 0xFFFF; /* Reset longFileName sequence */
        }
      }

    mResult = next (0);    /* Next entry */
    } while (mResult == FR_OK);

  return mResult;
  }
//}}}
//{{{
FRESULT cDirectory::read (int vol) {

  BYTE a, c, *dir1;
  BYTE ord = 0xFF;
  BYTE sum = 0xFF;

  mResult = FR_NO_FILE;
  while (mSector) {
    mResult = mFatFs->moveWindow (mSector);
    if (!mResult == FR_OK)
      break;
    dir1 = mDirShortFileName;          /* Ptr to the directory entry of current index */
    c = dir1[DIR_Name];
    if (c == 0) {
      mResult = FR_NO_FILE;
      break;
      }  /* Reached to end of table */

    a = dir1[DIR_Attr] & AM_MASK;
    if (c == DDEM || (int)((a & ~AM_ARC) == AM_VOL) != vol)
      // entry without valid data
      ord = 0xFF;
    else {
      if (a == AM_LFN) {      /* An longFileName entry is found */
        if (c & LLEF) {     /* Is it start of longFileName sequence? */
          sum = dir1[LDIR_Chksum];
          c &= ~LLEF; ord = c;
          mLongFileNameIndex = mIndex;
          }
        /* Check longFileName validity and capture it */
        ord = (c == ord && sum == dir1[LDIR_Chksum] && pickLongFileName (mLongFileName, dir1)) ? ord - 1 : 0xFF;
        }
      else {
        /* An shortFileName entry is found */
        if (ord || sum != sumShortFileName (dir1)) /* Is there a valid longFileName? */
          mLongFileNameIndex = 0xFFFF;   /* It has no longFileName. */
        break;
        }
      }

    mResult = next (0);        /* Next entry */
    if (!mResult == FR_OK)
      break;
    }

  if (!mResult == FR_OK)
    mSector = 0;

  return mResult;
  }
//}}}
//{{{
FRESULT cDirectory::remove() {

  // shortFileName index
  UINT i = mIndex;

  mResult = setIndex ((mLongFileNameIndex == 0xFFFF) ? i : mLongFileNameIndex); /* Goto the shortFileName or top of the longFileName entries */
  if (mResult == FR_OK) {
    do {
      mResult = mFatFs->moveWindow (mSector);
      if (!mResult == FR_OK)
        break;
      memset (mDirShortFileName, 0, SZ_DIRE); /* Clear and mark the entry "deleted" */

      *mDirShortFileName = DDEM;
      mFatFs->mWindowFlag = 1;
      if (mIndex >= i)
        break;  /* When reached shortFileName, all entries of the object has been deleted. */

      mResult = next (0);    /* Next entry */
      } while (mResult == FR_OK);

    if (mResult == FR_NO_FILE)
      mResult = FR_INT_ERR;
    }

  return mResult;
  }
//}}}

//{{{
void cDirectory::getFileInfo (cFileInfo& fileInfo) {

  char* fileInfoPtr = fileInfo.mShortFileName;
  if (mSector) {
    // copy dir shortFileName+info to fileInfo
    UINT i = 0;
    BYTE* dirShortFileNamePtr = mDirShortFileName;
    while (i < 11) {
      // Copy name body and extension
      char c = (char)dirShortFileNamePtr[i++];
      if (c == ' ')
        continue;
      if (c == RDDEM) // Restore replaced DDEM character
        c = (char)DDEM;
      if (i == 9) // Insert a . if extension is exist
        *fileInfoPtr++ = '.';
      if (IS_UPPER (c) && (dirShortFileNamePtr[DIR_NTres] & (i >= 9 ? NS_EXT : NS_BODY))) // To lower
        c += 0x20;
      *fileInfoPtr++ = c;
      }

    fileInfo.mAttribute = dirShortFileNamePtr[DIR_Attr];                // Attribute
    fileInfo.mFileSize = LD_DWORD (dirShortFileNamePtr + DIR_FileSize); // Size
    fileInfo.mDate = LD_WORD (dirShortFileNamePtr + DIR_WrtDate);       // Date
    fileInfo.mTime = LD_WORD (dirShortFileNamePtr + DIR_WrtTime);       // Time
    }

  // terminate fileInfo.mShortFileName
  *fileInfoPtr = 0;

  UINT i = 0;
  fileInfoPtr = fileInfo.mLongFileName;
  if (mSector && fileInfo.mLongFileNameSize && mLongFileNameIndex != 0xFFFF) {
    // get longFileName if available
    WCHAR w;
    WCHAR* longFileNamePtr = mLongFileName;
    while ((w = *longFileNamePtr++) != 0) {
      // Get an longFileName character
      w = convertToFromUnicode (w, 0);  // Unicode -> OEM
      if (!w) {
        // No longFileName if it could not be converted
        i = 0;
        break;
        }
      if (i >= fileInfo.mLongFileNameSize - 1) {
        // No longFileName if buffer overflow
        i = 0;
        break;
        }
      fileInfoPtr[i++] = (char)w;
      }
    }

  // terminate fileInfo.mLongFileName string
  fileInfoPtr[i] = 0;
  }
//}}}
//}}}

// cFile
//{{{
cFile::cFile (std::string path, BYTE mode) {

  fileBuffer = (BYTE*)malloc (SECTOR_SIZE);

  cDirectory directory;
  mode &= FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW;
  mResult = cFatFs::get()->findVolume (&directory.mFatFs, (BYTE)(mode & ~FA_READ));
  if (mResult == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    directory.followPath (path.c_str());
    BYTE* dir = directory.mDirShortFileName;
    if (mResult == FR_OK) {
      if (!dir) // Default directory itself
        mResult = FR_INVALID_NAME;
      else
        mResult = mFatFs->checkFileLock (&directory, (mode & ~FA_READ) ? 1 : 0);
      }

    // Create or Open a file
    if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
      if (!mResult == FR_OK) {
        //{{{  No file, create new
        if (mResult == FR_NO_FILE)
          // There is no file to open, create a new entry
          mResult = mFatFs->enquireFileLock() ? directory.registerNewEntry() : FR_TOO_MANY_OPEN_FILES;

        mode |= FA_CREATE_ALWAYS;           // File is created
        dir = directory.mDirShortFileName;  // New entry
        }
        //}}}
      else if (dir[DIR_Attr] & (AM_RDO | AM_DIR)) // Cannot overwrite it (R/O or DIR)
        mResult = FR_DENIED;
      else if (mode & FA_CREATE_NEW) // Cannot create as new file
        mResult = FR_EXIST;
      if ((mResult == FR_OK) && (mode & FA_CREATE_ALWAYS)) {
        //{{{  Truncate it if overwrite mode
        DWORD dw = getFatTime();               // Created time
        ST_DWORD (dir + DIR_CrtTime, dw);
        dir[DIR_Attr] = 0;                     // result attribute
        ST_DWORD (dir + DIR_FileSize, 0);      // size = 0
        DWORD cl = mFatFs->loadCluster (dir); // Get start cluster
        storeCluster (dir, 0);                 // cluster = 0
        mFatFs->mWindowFlag = 1;
        if (cl) {
          // Remove the cluster chain if exist
          dw = mFatFs->mWindowSector;
          mResult = mFatFs->removeChain (cl);
          if (mResult == FR_OK) {
            mFatFs->mLastCluster = cl - 1;
            // Reuse the cluster hole
            mResult = mFatFs->moveWindow (dw);
            }
          }
        }
        //}}}
      }

    else {
      //{{{  open existing file
      if (mResult == FR_OK) {
        if (dir[DIR_Attr] & AM_DIR) // It is a directory
          mResult = FR_NO_FILE;
        else if ((mode & FA_WRITE) && (dir[DIR_Attr] & AM_RDO)) // R/O violation
          mResult = FR_DENIED;
        }
      }
      //}}}

    if (mResult == FR_OK) {
      //{{{  Set file change flag if created or overwritten
      if (mode & FA_CREATE_ALWAYS)
        mode |= FA__WRITTEN;

      mDirSectorNum = mFatFs->mWindowSector;  /* Pointer to the directory entry */
      mDirPtr = dir;

      mLockId = mFatFs->incFileLock (&directory, (mode & ~FA_READ) ? 1 : 0);
      if (!mLockId)
        mResult = FR_INT_ERR;
      }
      //}}}

    if (mResult == FR_OK) {
      mFlag = mode;                               // File access mode
      mStartCluster = mFatFs->loadCluster (dir);  // File start cluster
      mFileSize = LD_DWORD (dir + DIR_FileSize);  // File size
      mPosition = 0;                              // File position
      mCachedSector = 0;
      mClusterTable = 0;                          // Normal seek mode
      mFatFs = directory.mFatFs;                  // Validate file object
      }
    }

  cFatFs::get()->unlock (mResult);
  }
//}}}
//{{{
cFile::~cFile() {

  // close file
  mResult = sync();
  if (mResult == FR_OK) {
    if (mFatFs->lock()) {
      // Decrement file open counter
      mResult = mFatFs->decFileLock (mLockId);
      if (mResult == FR_OK) // Invalidate file object
        mFatFs = 0;

      cFatFs::get()->unlock (FR_OK);
      }
    }

  free (fileBuffer);
  }
//}}}
//{{{
FRESULT cFile::read (void* readBuffer, int bytesToRead, int& bytesRead) {

  bytesRead = 0;
  if (!mFatFs->lock()) {
    //{{{  error
    cFatFs::get()->unlock (mResult);
    return mResult;
    }
    //}}}
  if (!(mFlag & FA_READ)) {
    //{{{  error
    mFatFs->unlock (FR_DENIED);
    return FR_DENIED;
    }
    //}}}

  // truncate bytesToRead by fileSize
  if (bytesToRead > int(mFileSize - mPosition))
    bytesToRead = mFileSize - mPosition;

  auto readBufferPtr = (BYTE*)readBuffer;
  int readCount;
  for (; bytesToRead; readBufferPtr += readCount, mPosition += readCount, bytesRead += readCount, bytesToRead -= readCount) {
    // repeat until all bytesToRead read
    if ((mPosition % SECTOR_SIZE) == 0) {
      // on sector boundary, sector offset in cluster
      BYTE csect = (BYTE)(mPosition / SECTOR_SIZE & (mFatFs->mSectorsPerCluster - 1));
      if (!csect) {
      //{{{  on cluster boundary
      DWORD readCluster;
      if (mPosition == 0) // at the top of the file, Follow from the origin
        readCluster = mStartCluster;
      else if (mClusterTable) // get cluster# from the CLMT
        readCluster = clmtCluster (mPosition);
      else // follow cluster chain on the FAT
        readCluster = mFatFs->getFat (mCluster);

      if (readCluster < 2)
        ABORT (FR_INT_ERR);
      if (readCluster == 0xFFFFFFFF)
        ABORT (FR_DISK_ERR);

      // Update current cluster
      mCluster = readCluster;
      }
      //}}}

      // get current sector
      DWORD readSector = mFatFs->clusterToSector (mCluster);
      if (!readSector)
        ABORT (FR_INT_ERR);

      readSector += csect;
      UINT contiguousClusters = bytesToRead / SECTOR_SIZE;
      if (contiguousClusters) {
        //{{{  read contiguousClusters sectors directly into readBuffer
        if (csect + contiguousClusters > mFatFs->mSectorsPerCluster) // Clip at cluster boundary
          contiguousClusters = mFatFs->mSectorsPerCluster - csect;

        if (diskRead (readBufferPtr, readSector, contiguousClusters) != RES_OK)
          ABORT (FR_DISK_ERR);

        // overwrite any dirty sector into readBuffer
        if ((mFlag & FA__DIRTY) && mCachedSector - readSector < contiguousClusters)
          memcpy (readBufferPtr + ((mCachedSector - readSector) * SECTOR_SIZE), fileBuffer, SECTOR_SIZE);

        // Number of bytes transferred
        readCount = SECTOR_SIZE * contiguousClusters;
        continue;
        }
        //}}}
      if (readSector != mCachedSector) {
        //{{{  load readSector, not cached in fileBuffer
        if (mFlag & FA__DIRTY) {
          // writeback dirty sector cache
          if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
            ABORT (FR_DISK_ERR);
          mFlag &= ~FA__DIRTY;
          }

        // read sector cache into fileBuffer cache
        if (diskRead (fileBuffer, readSector, 1) != RES_OK)
          ABORT (FR_DISK_ERR);
        mCachedSector = readSector;
        }
        //}}}
      }

    // copy partialSector from fileBuffer cache, can be top or tail
    readCount = SECTOR_SIZE - ((UINT)mPosition % SECTOR_SIZE);
    if (readCount > bytesToRead)
      readCount = bytesToRead;
    memcpy (readBufferPtr, &fileBuffer[mPosition % SECTOR_SIZE], readCount);
    }

  mFatFs->unlock (FR_OK);
  return FR_OK;
  }
//}}}
//{{{
FRESULT cFile::write (const void *buff, UINT btw, UINT& bw) {

  DWORD clst, sect;
  UINT wcnt, cc;
  BYTE csect;

  bw = 0;
  const BYTE *wbuff = (const BYTE*)buff;

  if (!mFatFs->lock()) {
    //{{{  error
    cFatFs::get()->unlock (mResult);
    return mResult;
    }
    //}}}
  if (!(mFlag & FA_WRITE)) {
    //{{{  return FR_DENIED
    mFatFs->unlock (FR_DENIED);
    return FR_DENIED;
    }
    //}}}

  // File size cannot reach 4GB
  if (mPosition + btw < mPosition)
    btw = 0;

  for ( ;  btw; wbuff += wcnt, mPosition += wcnt, bw += wcnt, btw -= wcnt) {
    // Repeat until all data written
    if ((mPosition % SECTOR_SIZE) == 0) {
      //{{{  On the sector boundary? , Sector offset in the cluster
      csect = (BYTE)(mPosition / SECTOR_SIZE & (mFatFs->mSectorsPerCluster - 1));
      if (!csect) {
        // On the cluster boundary?
        if (mPosition == 0) {
          // On the top of the file?
          clst = mStartCluster;
          // Follow from the origin
          if (clst == 0)
            // When no cluster is allocated,
            clst = mFatFs->createChain (0); // Create a new cluster chain
          }
        else {
          // Middle or end of the file
          if (mClusterTable) // Get cluster# from the CLMT
            clst = clmtCluster (mPosition);
          else // Follow or stretch cluster chain on the FAT
            clst = mFatFs->createChain (mCluster);
          }

        if (clst == 0) // Could not allocate a new cluster (disk full)
          break;
        if (clst == 1)
          ABORT (FR_INT_ERR);
        if (clst == 0xFFFFFFFF)
          ABORT (FR_DISK_ERR);

        // Update current cluster
        mCluster = clst;
        if (mStartCluster == 0) // Set start cluster if the first write
          mStartCluster = clst;
        }

      if (mFlag & FA__DIRTY) {
        //{{{  Write-back sector cache
        if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
          ABORT (FR_DISK_ERR);
        mFlag &= ~FA__DIRTY;
        }
        //}}}

      // Get current sector
      sect = mFatFs->clusterToSector (mCluster);
      if (!sect)
        ABORT (FR_INT_ERR);
      sect += csect;

      // When remaining bytes >= sector size
      cc = btw / SECTOR_SIZE;
      if (cc) {
        //{{{
        // Write maximum contiguous sectors directly
        if (csect + cc > mFatFs->mSectorsPerCluster) // Clip at cluster boundary
          cc = mFatFs->mSectorsPerCluster - csect;
        if (diskWrite (wbuff, sect, cc) != RES_OK)
          ABORT (FR_DISK_ERR);
        if (mCachedSector - sect < cc) {
          // Refill sector cache if it gets invalidated by the direct write
          memcpy (fileBuffer, wbuff + ((mCachedSector - sect) * SECTOR_SIZE), SECTOR_SIZE);
          mFlag &= ~FA__DIRTY;
          }

        // Number of bytes transferred
        wcnt = SECTOR_SIZE * cc;
        continue;
        }
        //}}}

      if (mCachedSector != sect) {
        //{{{  Fill sector cache with file data
        if (mPosition < mFileSize && diskRead (fileBuffer, sect, 1) != RES_OK)
          ABORT (FR_DISK_ERR);
        }
        //}}}
      mCachedSector = sect;
      }
      //}}}

    // Put partial sector into file I/O buffer
    wcnt = SECTOR_SIZE - ((UINT)mPosition % SECTOR_SIZE);
    if (wcnt > btw)
      wcnt = btw;

    // Fit partial sector
    memcpy (&fileBuffer[mPosition % SECTOR_SIZE], wbuff, wcnt);
    mFlag |= FA__DIRTY;
    }

  // Update file size if needed
  if (mPosition > mFileSize)
    mFileSize = mPosition;

  // Set file change flag
  mFlag |= FA__WRITTEN;

  mFatFs->unlock (FR_OK);
  return FR_OK;
  }
//}}}
//{{{
FRESULT cFile::seek (DWORD position) {

  if (mFatFs->lock()) {
    if (mClusterTable) {
      // fast seek
      if (position == CREATE_LINKMAP) {
        //{{{  create CLMT
        DWORD* tbl = mClusterTable;
        DWORD tlen = *tbl++;
        DWORD ulen = 2;  // Given table size and required table size */

        // Top of the chain */
        DWORD cl = mStartCluster;
        if (cl) {
          do {
            // Get a fragment */
            DWORD tcl = cl;
            DWORD ncl = 0;
            ulen += 2; // Top, length and used items */

            DWORD pcl;
            do {
              pcl = cl;
              ncl++;
              cl = mFatFs->getFat (cl);
              if (cl <= 1)
                ABORT (FR_INT_ERR);
              if (cl == 0xFFFFFFFF)
                ABORT (FR_DISK_ERR);
              } while (cl == pcl + 1);

            if (ulen <= tlen) {
              // Store the length and top of the fragment */
              *tbl++ = ncl;
              *tbl++ = tcl;
              }
            } while (cl < mFatFs->mNumFatEntries);  // Repeat until end of chain
          }

        // Number of items used
        *mClusterTable = ulen;
        if (ulen <= tlen) // Terminate table
          *tbl = 0;
        else // Given table size is smaller than required
          mResult = FR_NOT_ENOUGH_CORE;
        }
        //}}}
      else {
        //{{{  fast seek
        if (position > mFileSize) // clip position to file size
          position = mFileSize;

        mPosition = position;
        if (position) {
          mCluster = clmtCluster (position - 1);
          DWORD sector = mFatFs->clusterToSector (mCluster);
          if (!sector)
            ABORT (FR_INT_ERR);
          sector += (position - 1) / SECTOR_SIZE & (mFatFs->mSectorsPerCluster - 1);

          if ((mPosition % SECTOR_SIZE) && (sector != mCachedSector)) {
            // Refill sector cache if needed
            if (mFlag & FA__DIRTY) {
              // Write-back dirty sector cache
              if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
                ABORT (FR_DISK_ERR);
              mFlag &= ~FA__DIRTY;
              }

            // Load current sector
            if (diskRead (fileBuffer, sector, 1) != RES_OK)
              ABORT (FR_DISK_ERR);
            mCachedSector = sector;
            }
          }
        }
        //}}}
      }

    else {
      //{{{  normal seek
      if (position > mFileSize && !(mFlag & FA_WRITE))
        position = mFileSize;

      DWORD iPosition = mPosition;
      mPosition = 0;
      DWORD sector = 0;

      if (position) {
        DWORD cluster;
        DWORD bytesPerCluster = (DWORD)mFatFs->mSectorsPerCluster * SECTOR_SIZE;
        if ((iPosition > 0) && ((position - 1) / bytesPerCluster >= (iPosition - 1) / bytesPerCluster)) {
          //{{{  seek to same or following cluster
          // start from the current cluster
          mPosition = (iPosition - 1) & ~(bytesPerCluster - 1);
          position -= mPosition;
          cluster = mCluster;
          }
          //}}}
        else {
          //{{{  seek to back cluster, start from the first cluster
          cluster = mStartCluster;
          if (cluster == 0) {
            // If no cluster chain, create a new chain
            cluster = mFatFs->createChain (0);
            if (cluster == 1)
              ABORT (FR_INT_ERR);
            if (cluster == 0xFFFFFFFF)
              ABORT (FR_DISK_ERR);

            mStartCluster = cluster;
            }

          mCluster = cluster;
          }
          //}}}

        if (cluster != 0) {
          while (position > bytesPerCluster) {
            //{{{  cluster following loop
            if (mFlag & FA_WRITE) {
              // Check if in write mode or not, Force stretch if in write mode
              cluster = mFatFs->createChain (cluster);
              if (cluster == 0) {
                // When disk gets full, clip file size
                position = bytesPerCluster;
                break;
                }
              }
            else // Follow cluster chain if not in write mode
              cluster = mFatFs->getFat (cluster);

            if (cluster == 0xFFFFFFFF)
              ABORT (FR_DISK_ERR);
            if (cluster <= 1 || cluster >= mFatFs->mNumFatEntries)
              ABORT (FR_INT_ERR);
            mCluster = cluster;

            mPosition += bytesPerCluster;
            position -= bytesPerCluster;
            }
            //}}}
          mPosition += position;
          if (position % SECTOR_SIZE) {
            sector = mFatFs->clusterToSector (cluster);
            if (!sector)
              ABORT (FR_INT_ERR);
            sector += position / SECTOR_SIZE;
            }
          }
        }

      if (mPosition % SECTOR_SIZE && sector != mCachedSector) {
        if (mFlag & FA__DIRTY) {
           // writeBack dirty sector cache
          if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
            ABORT (FR_DISK_ERR);
          mFlag &= ~FA__DIRTY;
          }

        // Fill sector cache
        if (diskRead (fileBuffer, sector, 1) != RES_OK)
          ABORT (FR_DISK_ERR);
        mCachedSector = sector;
        }

      if (mPosition > mFileSize) {
        // Set file change flag if the file size is extended
        mFileSize = mPosition;
        mFlag |= FA__WRITTEN;
        }
      }
      //}}}
    }

  mFatFs->unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFile::truncate() {

  DWORD ncl;
  if (mFatFs->lock()) {
    if (!(mFlag & FA_WRITE))
      mResult = FR_DENIED;
    }

  if (mResult == FR_OK) {
    if (mFileSize > mPosition) {
      /* Set file size to current R/W point */
      mFileSize = mPosition;
      mFlag |= FA__WRITTEN;
      if (mPosition == 0) {
        //{{{  When set file size to zero, remove entire cluster chain
        mResult = mFatFs->removeChain (mStartCluster);
        mStartCluster = 0;
        }
        //}}}
      else {
        //{{{  When truncate a part of the file, remove remaining clusters
        ncl = mFatFs->getFat (mCluster);
        mResult = FR_OK;
        if (ncl == 0xFFFFFFFF)
          mResult = FR_DISK_ERR;
        if (ncl == 1)
          mResult = FR_INT_ERR;
        if (mResult == FR_OK && ncl < mFatFs->mNumFatEntries) {
          mResult = mFatFs->putFat (mCluster, 0x0FFFFFFF);
          if (mResult == FR_OK)
            mResult = mFatFs->removeChain (ncl);
          }
        }
        //}}}

      if (mResult == FR_OK && (mFlag & FA__DIRTY)) {
        if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
          mResult = FR_DISK_ERR;
        else
          mFlag &= ~FA__DIRTY;
        }
      }
    }

  mFatFs->unlock (mResult);
  return mResult;
  }
//}}}
//{{{
FRESULT cFile::sync() {

  if (mFatFs->lock()) {
    if (mFlag & FA__WRITTEN) {
      if (mFlag & FA__DIRTY) {
        if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK) {
          mResult = FR_DISK_ERR;
          mFatFs->unlock (mResult);
          return mResult;
          }
        mFlag &= ~FA__DIRTY;
        }

      // update directory entry
      mResult = mFatFs->moveWindow (mDirSectorNum);
      if (mResult == FR_OK) {
        BYTE* dir = mDirPtr;
        dir[DIR_Attr] |= AM_ARC;                  // Set archive bit
        ST_DWORD (dir + DIR_FileSize, mFileSize); // Update file size
        storeCluster (dir, mStartCluster);        // Update start cluster

        DWORD tm = getFatTime();                  // Update updated time
        ST_DWORD (dir + DIR_WrtTime, tm);
        ST_WORD (dir + DIR_LstAccDate, 0);
        mFlag &= ~FA__WRITTEN;

        mFatFs->mWindowFlag = 1;
        mResult = cFatFs::get()->syncFs();
        }
      }
    }

  mFatFs->unlock (mResult);
  return mResult;
  }
//}}}

//{{{
class cPutBuffer {
public:
  //{{{
  void putChBuffered (char c) {

    if (c == '\n')
      /* LF -> CRLF conversion */
      putChBuffered ('\r');

    /* Buffer write index (-1:error) */
    int i = idx;
    if (i < 0)
      return;

    // Write a character without conversion
    buf[i++] = (BYTE)c;

    if (i >= (int)(sizeof buf) - 3) {
      // Write buffered characters to the file
      UINT bw;
      file->write (buf, (UINT)i, bw);
      i = (bw == (UINT)i) ? 0 : -1;
      }

    idx = i;
    nchr++;
    }
  //}}}

  cFile* file;
  int idx;
  int nchr;
  BYTE buf[64];
  };
//}}}
//{{{
int cFile::putCh (char c) {

  // Initialize output buffer
  cPutBuffer putBuffer;
  putBuffer.file = this;
  putBuffer.nchr = 0;
  putBuffer.idx = 0;

  // Put character
  putBuffer.putChBuffered (c);

  UINT nw;
  if (putBuffer.idx >= 0 && write (putBuffer.buf, (UINT)putBuffer.idx, nw) == FR_OK && (UINT)putBuffer.idx == nw)
    return putBuffer.nchr;

  return -1;
  }
//}}}
//{{{
int cFile::putStr (const char* str) {

  // Initialize output buffer
  cPutBuffer putBuffer;
  putBuffer.file = this;
  putBuffer.nchr = putBuffer.idx = 0;

  // Put string
  while (*str)
    putBuffer.putChBuffered (*str++);

  UINT nw;
  if (putBuffer.idx >= 0 && write (putBuffer.buf, (UINT)putBuffer.idx, nw) == FR_OK && (UINT)putBuffer.idx == nw)
    return putBuffer.nchr;

  return -1;
  }
//}}}
//{{{
int cFile::printf (const char* fmt, ...) {

  BYTE f, r;
  UINT nw, i, j, w;
  DWORD v;
  char c, d, s[16], *p;

  // Initialize output buffer
  cPutBuffer putBuffer;
  putBuffer.file = this;
  putBuffer.nchr = putBuffer.idx = 0;

  va_list arp;
  va_start (arp, fmt);

  for (;;) {
    c = *fmt++;
    if (c == 0)
      break;      /* End of string */

    if (c != '%') {
      // Non escape character */
      putBuffer.putChBuffered (c);
      continue;
      }

    w = f = 0;
    c = *fmt++;
    if (c == '0') {
      //{{{  flag: '0' padding
      f = 1;
      c = *fmt++;
      }
      //}}}
    else if (c == '-') {
      //{{{  flag: left justified
      f = 2;
      c = *fmt++;
      }
      //}}}
    while (IS_DIGIT (c)) {
      //{{{  Precision
      w = w * 10 + c - '0';
      c = *fmt++;
      }
      //}}}
    if (c == 'l' || c == 'L')  {
      //{{{  Prefix: Size is long int
      f |= 4;
      c = *fmt++;
      }
      //}}}
    if (!c)
      break;

    d = c;
    if (IS_LOWER (d))
      d -= 0x20;

    switch (d) {
      //{{{
      case 'S' : // String
        p = va_arg(arp, char*);
        for (j = 0; p[j]; j++) ;
        if (!(f & 2)) {
          while (j++ < w)
            putBuffer.putChBuffered (' ');
          }
        while (*p)
          putBuffer.putChBuffered (*p++);
        while (j++ < w)
          putBuffer.putChBuffered (' ');
        continue;
      //}}}
      //{{{
      case 'C' : // Character
        putBuffer.putChBuffered ((char)va_arg(arp, int));
        continue;
      //}}}
      //{{{
      case 'B' : // Binary
        r = 2;
        break;
      //}}}
      //{{{
      case 'O' : // Octal
        r = 8;
        break;
      //}}}
      case 'D' :      // Signed decimal
      //{{{
      case 'U' : // Unsigned decimal
        r = 10;
        break;
      //}}}
      //{{{
      case 'X' : // Hexdecimal
        r = 16;
        break;
      //}}}
      //{{{
      default:   // Unknown type (pass-through)
        putBuffer.putChBuffered (c);
        continue;
      //}}}
      }

    // Get an argument and put it in numeral
    v = (f & 4) ? (DWORD)va_arg(arp, long) : ((d == 'D') ? (DWORD)(long)va_arg(arp, int) : (DWORD)va_arg(arp, unsigned int));
    if (d == 'D' && (v & 0x80000000)) {
      v = 0 - v;
      f |= 8;
      }

    i = 0;
    do {
      d = (char)(v % r); v /= r;
      if (d > 9)
        d += (c == 'x') ? 0x27 : 0x07;
      s[i++] = d + '0';
      } while (v && i < sizeof s / sizeof s[0]);

    if (f & 8)
      s[i++] = '-';
    j = i;
    d = (f & 1) ? '0' : ' ';

    while (!(f & 2) && j++ < w)
      putBuffer.putChBuffered (d);

    do {
      putBuffer.putChBuffered (s[--i]);
      } while (i);

    while (j++ < w)
      putBuffer.putChBuffered (d);
    }

  va_end (arp);

  if (putBuffer.idx >= 0 && putBuffer.file->write (putBuffer.buf, (UINT)putBuffer.idx, nw) == FR_OK && (UINT)putBuffer.idx == nw)
    return putBuffer.nchr;

  return -1;
  }
//}}}
//{{{
char* cFile::gets (char* buff, int len) {

  int n = 0;
  char c, *p = buff;
  BYTE s[2];
  int rc;

  while (n < len - 1) {
    // Read characters until buffer gets filled
    read (s, 1, rc);
    if (rc != 1)
      break;
    c = s[0];
    if (c == '\r')
      continue; // Strip '\r'
    *p++ = c;
    n++;
    if (c == '\n')
      break;   // Break on EOL
    }

  *p = 0;

  // When no data read (eof or error), return with error
  return n ? buff : 0;
  }
//}}}

// cFile private
//{{{
DWORD cFile::clmtCluster (DWORD ofs) {

  // Top of CLMT
  DWORD* tbl = mClusterTable + 1;

  // Cluster order from top of the file
  DWORD cl = ofs / SECTOR_SIZE / mFatFs->mSectorsPerCluster;
  for (;;) {
    // Number of cluters in the fragment
    DWORD ncl = *tbl++;
    if (!ncl)
      return 0;   // End of table? (error)

    if (cl < ncl)
      break;  // In this fragment?

    cl -= ncl;
    tbl++;     // Next fragment
    }

  // Return the cluster number
  return cl + *tbl;
  }
//}}}
