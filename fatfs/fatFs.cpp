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

#define ABORT(result) { mError = (BYTE)(result); mFs->unlock (result); return result; }
//}}}
//{{{  static const
// Offset of LFN characters in the directory entry
static const BYTE LfnOfs[] = { 1,3,5,7,9,14,16,18,20,22,24,28,30 };

static const WORD vst[] = {  1024,   512,  256,  128,   64,    32,   16,    8,    4,    2,   0 };
static const WORD cst[] = { 32768, 16384, 8192, 4096, 2048, 16384, 8192, 4096, 2048, 1024, 512 };

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
static int compareLfn (WCHAR* lfnbuf, BYTE* dir) {

  UINT i = ((dir[LDIR_Ord] & ~LLEF) - 1) * 13; /* Get offset in the LFN buffer */
  UINT s = 0;
  WCHAR wc = 1;
  do {
    WCHAR uc = LD_WORD (dir + LfnOfs[s]);  /* Pick an LFN character from the entry */
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
static int pickLfn (WCHAR* lfnbuf, BYTE* dir) {

  UINT i = ((dir[LDIR_Ord] & 0x3F) - 1) * 13;  /* Offset in the LFN buffer */
  UINT s = 0;
  WCHAR wc = 1;
  do {
    WCHAR uc = LD_WORD (dir + LfnOfs[s]);    /* Pick an LFN character from the entry */
    if (wc) { /* Last character has not been processed */
      if (i >= MAX_LFN) return 0;  /* Buffer overflow? */
      lfnbuf[i++] = wc = uc;      /* Store it */
      }
    else if (uc != 0xFFFF)
      return 0;   /* Check filler */
    } while (++s < 13);           /* Read all character in the entry */

  if (dir[LDIR_Ord] & LLEF) {       /* Put terminator if it is the last LFN part */
    if (i >= MAX_LFN)
      return 0;    /* Buffer overflow? */
    lfnbuf[i] = 0;
    }

  return 1;
  }
//}}}
//{{{
static void fitLfn (const WCHAR* lfnbuf, BYTE* dir, BYTE ord, BYTE sum) {

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
    ST_WORD (dir+LfnOfs[s], wc);  /* Put it */
    if (!wc)
      wc = 0xFFFF;  /* Padding characters following last character */
    } while (++s < 13);

  if (wc == 0xFFFF || !lfnbuf[i])
    ord |= LLEF;  /* Bottom LFN part is the start of LFN sequence */

  dir[LDIR_Ord] = ord;      /* Set the LFN order */
  }
//}}}
//{{{
static BYTE sumSfn (const BYTE* dir) {

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
cFatFs::cFileSem cFatFs::mFiles[FS_LOCK];
//}}}
//{{{
cFatFs::cFatFs() {

  osSemaphoreDef (fatfs);
  mSemaphore = osSemaphoreCreate (osSemaphore (fatfs), 1);

  mWindowBuffer = (BYTE*)malloc (SECTOR_SIZE);
  }
//}}}
//{{{
FRESULT cFatFs::getFree (DWORD& numClusters, DWORD& clusterSize) {

  cFatFs* dummyFs;
  FRESULT result = findVolume (&dummyFs, 0);
  if (result == FR_OK) {
    clusterSize = mSectorsPerCluster;

    // if mFreeClusters is valid, return it without full cluster scan
    if (mFreeClusters <= mNumFatEntries - 2)
      numClusters = mFreeClusters;
    else {
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
            result = FR_DISK_ERR;
            break;
            }
          if (stat == 1) {
            result = FR_INT_ERR;
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
            result = moveWindow (sect++);
            if (result != FR_OK)
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

      numClusters = n;
      }
      //}}}
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::getCwd (char* buff, UINT len) {

  UINT i, n;
  DWORD ccl;
  char *tp;

  *buff = 0;
  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 0);
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    i = len;   // Bottom of buffer (directory stack base)
    directory.mStartCluster = mCurDirSector;  // Start to follow upper directory from current directory
    while ((ccl = directory.mStartCluster) != 0) {
      // Repeat while current directory is a sub-directory, Get parent directory
      result = directory.setIndex (1);
      if (result != FR_OK)
        break;

      result = directory.read (0);
      if (result != FR_OK)
        break;

      // Goto parent directory
      directory.mStartCluster = loadCluster (directory.mDirShortFileName);
      result = directory.setIndex (0);
      if (result != FR_OK)
        break;

      do {
        // Find the entry links to the child directory
        result = directory.read (0);
        if (result != FR_OK)
          break;
        if (ccl == loadCluster (directory.mDirShortFileName))
          break;  // Found the entry
        result = directory.next (0);
        } while (result == FR_OK);
      if (result == FR_NO_FILE)
        result = FR_INT_ERR; // It cannot be 'not found'
      if (result != FR_OK)
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
        result = FR_NOT_ENOUGH_CORE;
          break;
        }
      while (n)
        buff[--i] = tp[--n];
      buff[--i] = '/';
      }

    tp = buff;
    if (result == FR_OK) {
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

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::getLabel (char* label, DWORD& vsn) {

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 0);
  if (result == FR_OK && label) {
    // Open root directory
    directory.mStartCluster = 0;
    result = directory.setIndex (0);
    if (result == FR_OK) {
      // Get an entry with AM_VOL
      result = directory.read (1);
      if (result == FR_OK) {
        // volume label exists
        memcpy (label, directory.mDirShortFileName, 11);
        UINT k = 11;
        do {
          label[k] = 0;
          if (!k)
            break;
          } while (label[--k] == ' ');
        }

      if (result == FR_NO_FILE) {
        // No label, return empty string
        label[0] = 0;
        result = FR_OK;
        }
      }
    }

  // Get volume serial number
  if (result == FR_OK && vsn) {
    result = moveWindow (mVolBase);
    if (result == FR_OK) {
      UINT i = mFsType == FS_FAT32 ? BS_VolID32 : BS_VolID;
      vsn = LD_DWORD (&mWindowBuffer[i]);
      }
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::setLabel (const char* label) {

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 1);
  if (result) {
    unlock (result);
    return result;
    }

  // Create a volume label in directory form
  BYTE volumeName[11];
  volumeName[0] = 0;

  // Get name length
  UINT nameSize;
  for (nameSize = 0; label[nameSize]; nameSize++) ;

  // Remove trailing spaces
  for ( ; nameSize && label[nameSize - 1] == ' '; nameSize--) ;

  if (nameSize) {
    // Create volume label in directory form
    UINT i = 0;
    UINT j = 0;
    do {
      WCHAR w = (BYTE)label[i++];
      w = convertToFromUnicode (wideToUpperCase (convertToFromUnicode(w, 1)), 0);
      if (!w || strchr ("\"*+,.:;<=>\?[]|\x7F", w) || j >= (UINT)((w >= 0x100) ? 10 : 11)) {
        //{{{
        // Reject invalid characters for volume label
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
  result = directory.setIndex (0);
  if (result == FR_OK) {
    //{{{  Get an entry with AM_VOL
    result = directory.read (1);
    if (result == FR_OK) {
      //{{{  volume label found
      if (volumeName[0]) {
        // Change the volume label name
        memcpy (directory.mDirShortFileName, volumeName, 11);
        DWORD tm = getFatTime();
        ST_DWORD (directory.mDirShortFileName + DIR_WrtTime, tm);
        }
      else // Remove the volume label
        directory.mDirShortFileName[0] = DDEM;
      mWindowFlag = 1;
      result = syncFs();
      }
      //}}}
    else {
      //{{{  No volume label is found or error
      if (result == FR_NO_FILE) {
        result = FR_OK;
        if (volumeName[0]) {
          // Create volume label as new, Allocate an entry for volume label
          result = directory.allocate (1);
          if (result == FR_OK) {
            // Set volume label
            memset (directory.mDirShortFileName, 0, SZ_DIRE);
            memcpy (directory.mDirShortFileName, volumeName, 11);
            directory.mDirShortFileName[DIR_Attr] = AM_VOL;
            DWORD tm = getFatTime();
            ST_DWORD (directory.mDirShortFileName + DIR_WrtTime, tm);
            mWindowFlag = 1;
            result = syncFs();
            }
          }
        }
      }
      //}}}
    }
    //}}}

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::mkDir (const char* path) {

  BYTE *dir, n;
  DWORD dsc, dcl, pcl;

  DWORD tm = getFatTime();

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 1);
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    result = directory.followPath (path);
    if (result == FR_OK)
      result = FR_EXIST;   /* Any object with same name is already existing */
    if (result == FR_NO_FILE && (directory.mShortFileName[NSFLAG] & NS_DOT))
      result = FR_INVALID_NAME;
    if (result == FR_NO_FILE) {
      /* Can create a new directory */
      dcl = createChain (0);   /* Allocate a cluster for the new directory table */
      result = FR_OK;
      if (dcl == 0)
        result = FR_DENIED;    /* No space to allocate a new cluster */
      if (dcl == 1)
        result = FR_INT_ERR;
      if (dcl == 0xFFFFFFFF)
        result = FR_DISK_ERR;

      if (result == FR_OK)  /* Flush FAT */
        result = syncWindow();

      if (result == FR_OK) {
        /* Initialize the new directory table */
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
          result = syncWindow();
          if (result != FR_OK)
            break;
          memset (dir, 0, SECTOR_SIZE);
          }
        }

      if (result == FR_OK)
        result = directory.registerNewEntry();
      if (result != FR_OK)
        removeChain (dcl);     /* Could not register, remove cluster chain */
      else {
        dir = directory.mDirShortFileName;
        dir[DIR_Attr] = AM_DIR;       /* Attribute */
        ST_DWORD (dir + DIR_WrtTime, tm);  /* Created time */
        storeCluster (dir, dcl);         /* Table start cluster */
        mWindowFlag = 1;
        result = syncFs();
        }
      }
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::chDir (const char* path) {

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 0);
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    result = directory.followPath (path);

    if (result == FR_OK) {
      /* Follow completed */
      if (!directory.mDirShortFileName)
        mCurDirSector = directory.mStartCluster;  /* Start directory itself */
      else {
        if (directory.mDirShortFileName[DIR_Attr] & AM_DIR)  /* Reached to the directory */
          mCurDirSector = loadCluster (directory.mDirShortFileName);
        else
          result = FR_NO_PATH;   /* Reached but a file */
        }
      }
    if (result == FR_NO_FILE)
      result = FR_NO_PATH;
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::stat (const char* path, cFileInfo& fileInfo) {

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 0);
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    result = directory.followPath (path);
    if (result == FR_OK) {
      if (directory.mDirShortFileName)
        directory.getFileInfo (fileInfo);
      else // root directory
        result = FR_INVALID_NAME;
      }
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::rename (const char* path_old, const char* path_new) {

  BYTE buf[21], *dir;
  DWORD dw;

  cDirectory oldDirectory;
  FRESULT result = findVolume (&oldDirectory.mFs, 1);
  if (result == FR_OK) {
    cDirectory newDirectory;
    newDirectory.mFs = oldDirectory.mFs;

    WCHAR longFileName [(MAX_LFN + 1) * 2];
    oldDirectory.mLongFileName = longFileName;
    result = oldDirectory.followPath (path_old);
    if (result == FR_OK && (oldDirectory.mShortFileName[NSFLAG] & NS_DOT))
      result = FR_INVALID_NAME;
    if (result == FR_OK)
      result = checkFileLock (&oldDirectory, 2);
    if (result == FR_OK) {
      // old object is found
      if (!oldDirectory.mDirShortFileName) // root dir?
        result = FR_NO_FILE;
      else {
        // Save information about object except name
        memcpy (buf, oldDirectory.mDirShortFileName + DIR_Attr, 21);
        // Duplicate the directory object
        memcpy (&newDirectory, &oldDirectory, sizeof (cDirectory));
        result = newDirectory.followPath (path_new);
        if (result == FR_OK) // new object name already exists
          result = FR_EXIST;
        if (result == FR_NO_FILE) {
          // valid path, no name collision
          result = newDirectory.registerNewEntry();
          if (result == FR_OK) {

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
                result = FR_INT_ERR;
              else {
                result = moveWindow (dw);
                dir = mWindowBuffer + SZ_DIRE * 1; // Ptr to .. entry
                if (result == FR_OK && dir[1] == '.') {
                  storeCluster (dir, newDirectory.mStartCluster);
                  mWindowFlag = 1;
                  }
                }
              }

            if (result == FR_OK) {
              result = oldDirectory.remove();
              if (result == FR_OK)
                result = syncFs();
              }
  // End of critical section
            }
          }
        }
      }
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::chMod (const char* path, BYTE attr, BYTE mask) {

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 1);
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    result = directory.followPath (path);
    if (result == FR_OK && (directory.mShortFileName[NSFLAG] & NS_DOT))
      result = FR_INVALID_NAME;
    if (result == FR_OK) {
      BYTE* dir = directory.mDirShortFileName;
      if (!dir)
        // root directory
        result = FR_INVALID_NAME;
      else {
        // File or sub directory
        mask &= AM_RDO|AM_HID|AM_SYS|AM_ARC;  /* Valid attribute mask */
        dir[DIR_Attr] = (attr & mask) | (dir[DIR_Attr] & (BYTE)~mask);  /* Apply attribute change */
        mWindowFlag = 1;
        result = syncFs();
        }
      }
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::utime (const char* path, const cFileInfo& fileInfo) {

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 1);
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    result = directory.followPath (path);
    if (result == FR_OK && (directory.mShortFileName[NSFLAG] & NS_DOT))
      result = FR_INVALID_NAME;
    if (result == FR_OK) {
      BYTE* dir = directory.mDirShortFileName;
      if (!dir)
        // root dir
        result = FR_INVALID_NAME;
      else {
        // file or subDir
        ST_WORD (dir + DIR_WrtTime, fileInfo.mTime);
        ST_WORD (dir + DIR_WrtDate, fileInfo.mDate);
        mWindowFlag = 1;
        result = syncFs();
        }
      }
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::unlink (const char* path) {

  cDirectory directory;
  FRESULT result = findVolume (&directory.mFs, 1);
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    result = directory.followPath (path);
    if (result == FR_OK && (directory.mShortFileName[NSFLAG] & NS_DOT)) // Cannot remove dot entry */
      result = FR_INVALID_NAME;
    if (result == FR_OK)  // Cannot remove open object */
      result = checkFileLock (&directory, 2);
    if (result == FR_OK) {
      // The object is accessible
      BYTE* dir = directory.mDirShortFileName;
      if (!dir)  // Cannot remove the origin directory */
        result = FR_INVALID_NAME;
      else if (dir[DIR_Attr] & AM_RDO)  // Cannot remove R/O object */
        result = FR_DENIED;

      DWORD dclst = 0;
      if (result == FR_OK) {
        dclst = loadCluster (dir);
        if (dclst && (dir[DIR_Attr] & AM_DIR)) {
          if (dclst == mCurDirSector) // current directory
            result = FR_DENIED;
          else {
            // Open the sub-directory
            cDirectory subDirectory;
            memcpy (&subDirectory, &directory, sizeof (cDirectory));
            subDirectory.mStartCluster = dclst;
            result = subDirectory.setIndex (2);
            if (result == FR_OK) {
              result = subDirectory.read (0);
              if (result == FR_OK)
                result = FR_DENIED;  // Not empty (cannot remove)
              if (result == FR_NO_FILE)
                result = FR_OK; // Empty (can remove)
              }
            }
          }
        }

      if (result == FR_OK) {
        result = directory.remove();  // Remove the directory entry
        if (result == FR_OK && dclst)  // Remove the cluster chain if exist
          result = removeChain (dclst);
        if (result == FR_OK)
          result = syncFs();
        }
      }
    }

  unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFatFs::mkfs (const char* path, BYTE sfd, UINT au) {

  BYTE fmt, md, sys, *tbl;
  DWORD n_clst, vs, n, wsect;
  UINT i;
  DWORD b_vol, b_fat, b_dir, b_data;  /* LBA */
  DWORD n_vol, n_rsv, n_fat, n_dir; /* Size */

#if USE_TRIM
  DWORD eb[2];
#endif

  if (sfd > 1)
    return FR_INVALID_PARAMETER;

  mFsType = 0;

  /* Get disk statics */
  DSTATUS stat = diskInitialize();
  if (stat & STA_NOINIT)
    return FR_NOT_READY;
  if (stat & STA_PROTECT)
    return FR_WRITE_PROTECTED;

  /* Create a partition in this function */
  if (diskIoctl (GET_SECTOR_COUNT, &n_vol) != RES_OK || n_vol < 128)
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
  au /= SECTOR_SIZE;
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

  b_fat = b_vol + n_rsv;           /* FAT area start sector */
  b_dir = b_fat + n_fat * N_FATS;  /* Directory area start sector */
  b_data = b_dir + n_dir;          /* Data area start sector */
  if (n_vol < b_data + au - b_vol)
    return FR_MKFS_ABORTED;  /* Too small volume */

  /* Align data start sector to erase block boundary (for flash memory media) */
  if (diskIoctl (GET_BLOCK_SIZE, &n) != RES_OK || !n || n > 32768)
    n = 1;
  n = (b_data + n - 1) & ~(n - 1);  /* Next nearest erase block from current data start */
  n = (n - b_data) / N_FATS;
  if (fmt == FS_FAT32) {
    /* FAT32: Move FAT offset */
    n_rsv += n;
    b_fat += n;
    }
  else
    /* FAT12/16: Expand FAT size */
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

  if (sfd)
    /* No partition table (SFD) */
    md = 0xF0;
  else {
    /* Create partition table (FDISK) */
    memset (mWindowBuffer, 0, SECTOR_SIZE);
    tbl = mWindowBuffer + MBR_Table;  /* Create partition table for single partition in the drive */
    tbl[1] = 1;                    /* Partition start head */
    tbl[2] = 1;                    /* Partition start sector */
    tbl[3] = 0;                    /* Partition start cylinder */
    tbl[4] = sys;                  /* System type */
    tbl[5] = 254;                  /* Partition end head */
    n = (b_vol + n_vol) / 63 / 255;
    tbl[6] = (BYTE)(n >> 2 | 63);  /* Partition end sector */
    tbl[7] = (BYTE)n;              /* End cylinder */
    ST_DWORD (tbl + 8, 63);         /* Partition start in LBA */
    ST_DWORD (tbl + 12, n_vol);     /* Partition size in LBA */
    ST_WORD (mWindowBuffer + BS_55AA, 0xAA55);  /* MBR signature */
    if (diskWrite (mWindowBuffer, 0, 1) != RES_OK) /* Write it to the MBR */
      return FR_DISK_ERR;
    md = 0xF8;
    }

  /* Create BPB in the VBR */
  tbl = mWindowBuffer;             /* Clear sector */
  memset (tbl, 0, SECTOR_SIZE);
  memcpy (tbl, "\xEB\xFE\x90" "MSDOS5.0", 11); /* Boot jump code, OEM name */

  i = SECTOR_SIZE; /* Sector size */
  ST_WORD (tbl + BPB_BytsPerSec, i);

  tbl[BPB_SecPerClus] = (BYTE)au;        /* Sectors per cluster */
  ST_WORD (tbl + BPB_RsvdSecCnt, n_rsv);  /* Reserved sectors */
  tbl[BPB_NumFATs] = N_FATS;             /* Number of FATs */

  i = (fmt == FS_FAT32) ? 0 : N_ROOTDIR; /* Number of root directory entries */
  ST_WORD (tbl + BPB_RootEntCnt, i);
  if (n_vol < 0x10000) {                 /* Number of total sectors */
    ST_WORD (tbl + BPB_TotSec16, n_vol);
    }
  else {
    ST_DWORD (tbl + BPB_TotSec32, n_vol);
    }
  tbl[BPB_Media] = md;                   /* Media descriptor */

  ST_WORD (tbl + BPB_SecPerTrk, 63);      /* Number of sectors per track */
  ST_WORD (tbl + BPB_NumHeads, 255);      /* Number of heads */
  ST_DWORD (tbl + BPB_HiddSec, b_vol);    /* Hidden sectors */

  n = getFatTime();                     /* Use current time as VSN */
  if (fmt == FS_FAT32) {
    ST_DWORD (tbl + BS_VolID32, n);       /* VSN */
    ST_DWORD (tbl + BPB_FATSz32, n_fat);  /* Number of sectors per FAT */
    ST_DWORD (tbl + BPB_RootClus, 2);     /* Root directory start cluster (2) */
    ST_WORD (tbl + BPB_FSInfo, 1);        /* FSINFO record offset (VBR + 1) */
    ST_WORD (tbl + BPB_BkBootSec, 6);     /* Backup boot record offset (VBR + 6) */
    tbl[BS_DrvNum32] = 0x80;             /* Drive number */
    tbl[BS_BootSig32] = 0x29;            /* Extended boot signature */
    memcpy (tbl + BS_VolLab32, "NO NAME    " "FAT32   ", 19); /* Volume label, FAT signature */
    }
  else {
    ST_DWORD (tbl + BS_VolID, n);          /* VSN */
    ST_WORD (tbl + BPB_FATSz16, n_fat);    /* Number of sectors per FAT */
    tbl[BS_DrvNum] = 0x80;                /* Drive number */
    tbl[BS_BootSig] = 0x29;               /* Extended boot signature */
    memcpy (tbl + BS_VolLab, "NO NAME    " "FAT     ", 19); /* Volume label, FAT signature */
    }

  ST_WORD (tbl + BS_55AA, 0xAA55);         /* Signature (Offset is fixed here regardless of sector size) */
  if (diskWrite (tbl, b_vol, 1) != RES_OK)  /* Write it to the VBR sector */
    return FR_DISK_ERR;
  if (fmt == FS_FAT32)          /* Write backup VBR if needed (VBR + 6) */
    diskWrite (tbl, b_vol + 6, 1);

  /* Initialize FAT area */
  wsect = b_fat;
  for (i = 0; i < N_FATS; i++) {
    /* Initialize each FAT copy */
    memset (tbl, 0, SECTOR_SIZE);   /* 1st sector of the FAT  */
    n = md;                     /* Media descriptor byte */
    if (fmt != FS_FAT32) {
      n |= (fmt == FS_FAT12) ? 0x00FFFF00 : 0xFFFFFF00;
      ST_DWORD (tbl + 0, n);     /* Reserve cluster #0-1 (FAT12/16) */
      }
    else {
      n |= 0xFFFFFF00;
      ST_DWORD (tbl + 0, n);     /* Reserve cluster #0-1 (FAT32) */
      ST_DWORD (tbl + 4, 0xFFFFFFFF);
      ST_DWORD (tbl + 8, 0x0FFFFFFF);  /* Reserve cluster #2 for root directory */
      }

    if (diskWrite (tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;

    memset (tbl, 0, SECTOR_SIZE); /* Fill following FAT entries with zero */
    for (n = 1; n < n_fat; n++) {
      /* This loop may take a time on FAT32 volume due to many single sector writes */
      if (diskWrite (tbl, wsect++, 1) != RES_OK)
        return FR_DISK_ERR;
      }
    }

  /* Initialize root directory */
  i = (fmt == FS_FAT32) ? au : (UINT)n_dir;
  do {
    if (diskWrite (tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;
    } while (--i);

#if USE_TRIM /* Erase data area if needed */
  {
    eb[0] = wsect; eb[1] = wsect + (n_clst - ((fmt == FS_FAT32) ? 1 : 0)) * au - 1;
    diskIoctl (vol, CTRL_TRIM, eb);
  }
#endif

  /* Create FSINFO if needed */
  if (fmt == FS_FAT32) {
    ST_DWORD (tbl + FSI_LeadSig, 0x41615252);
    ST_DWORD (tbl + FSI_StrucSig, 0x61417272);
    ST_DWORD (tbl + FSI_Free_Count, n_clst - 1); /* Number of free clusters */
    ST_DWORD (tbl + FSI_Nxt_Free, 2);            /* Last allocated cluster# */
    ST_WORD (tbl + BS_55AA, 0xAA55);
    diskWrite (tbl, b_vol + 1, 1);        /* Write original (VBR + 1) */
    diskWrite (tbl, b_vol + 7, 1);        /* Write backup (VBR + 7) */
    }

  return (diskIoctl (CTRL_SYNC, 0) == RES_OK) ? FR_OK : FR_DISK_ERR;
  }
//}}}
//{{{  cFatFs private members
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
  if (!mSectorsPerCluster || (mSectorsPerCluster & (mSectorsPerCluster - 1)))  // (Must be power of 2)
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
    return FR_NO_FILESYSTEM; /* (Must not be 0) */

  // Determine the FAT sub type
  sysect = nrsv + fasize + mNumRootdir / (SECTOR_SIZE / SZ_DIRE); /* RSV + FAT + DIR */
  if (tsect < sysect)
    return FR_NO_FILESYSTEM; /* (Invalid volume size) */

  // Number of clusters
  nclst = (tsect - sysect) / mSectorsPerCluster;
  if (!nclst)
    return FR_NO_FILESYSTEM; /* (Invalid volume size) */
  fmt = FS_FAT12;
  if (nclst >= MIN_FAT16)
    fmt = FS_FAT16;
  if (nclst >= MIN_FAT32)
    fmt = FS_FAT32;

  // Boundaries and Limits
  mNumFatEntries = nclst + 2;      // Number of FAT entries
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

  mFsType = fmt;  /* FAT sub-type */
  ++mMountId;
  mCurDirSector = 0;       /* Set current directory to root */
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
  if (cluster == 0) {    /* Create a new chain */
    scl = mLastCluster;     /* Get suggested start point */
    if (!scl || scl >= mNumFatEntries) scl = 1;
    }
  else {          /* Stretch the current chain */
    DWORD cs = getFat (cluster);     /* Check the cluster status */
    if (cs < 2)
      return 1;     /* Invalid value */
    if (cs == 0xFFFFFFFF)
      return cs;  /* A disk error occurred */
    if (cs < mNumFatEntries)
      return cs; /* It is already followed by next cluster */
    scl = cluster;
    }

 DWORD ncl = scl;        /* Start cluster */
  for (;;) {
    ncl++;              /* Next cluster */
    if (ncl >= mNumFatEntries) {    /* Check wrap around */
      ncl = 2;
      if (ncl > scl) return 0;  /* No free cluster */
      }
    DWORD cs = getFat (ncl);      /* Get the cluster status */
    if (cs == 0)
      break;       /* Found a free cluster */
    if (cs == 0xFFFFFFFF || cs == 1)/* An error occurred */
      return cs;
    if (ncl == scl)
      return 0;   /* No free cluster */
    }

  FRESULT result = putFat (ncl, 0x0FFFFFFF); /* Mark the new cluster "last link" */
  if (result == FR_OK && cluster != 0)
    result = putFat (cluster, ncl); /* Link it to the previous one if needed */

  if (result == FR_OK) {
    mLastCluster = ncl;  /* Update FSINFO */
    if (mFreeClusters != 0xFFFFFFFF) {
      mFreeClusters--;
      mFsiFlag |= 1;
      }
    }
  else
    ncl = (result == FR_DISK_ERR) ? 0xFFFFFFFF : 1;

  return ncl;   /* Return new cluster number or error code */
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
    while (cluster < mNumFatEntries) {     /* Not a last link? */
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
      if (mFreeClusters != 0xFFFFFFFF) { /* Update FSINFO */
        mFreeClusters++;
        mFsiFlag |= 1;
        }

#if USE_TRIM
      if (ecl + 1 == nxt)
        /* Is next cluster contiguous? */
        ecl = nxt;
      else {        /* End of contiguous clusters */
        rt[0] = clust2sect(fs, scl);          /* Start sector */
        rt[1] = clust2sect(fs, ecl) + mSectorsPerCluster - 1;  /* End sector */
        diskIoctl (CTRL_TRIM, rt);       /* Erase the block */
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

  return osSemaphoreWait (mSemaphore, 1000) == osOK;
  }
//}}}
//{{{
void cFatFs::unlock (FRESULT result) {

  if (result != FR_NOT_ENABLED &&
      result != FR_INVALID_DRIVE &&
      result != FR_INVALID_OBJECT &&
      result != FR_TIMEOUT)
    osSemaphoreRelease (mSemaphore);
  }
//}}}

//{{{
int cFatFs::enquireFileLock() {

  UINT i;
  for (i = 0; i < FS_LOCK && mFiles[i].mFs; i++) ;
  return (i == FS_LOCK) ? 0 : 1;
  }
//}}}
//{{{
FRESULT cFatFs::checkFileLock (cDirectory* directory, int acc) {

  UINT i, be;

  // Search file semaphore table
  for (i = be = 0; i < FS_LOCK; i++) {
    if (mFiles[i].mFs) {
      // Existing entry
      if (mFiles[i].mFs == directory->mFs && mFiles[i].clu == directory->mStartCluster && mFiles[i].idx == directory->mIndex)
         break;
      }
    else
      // Blank entry
      be = 1;
    }

  if (i == FS_LOCK)
    // The object is not opened
    return (be || acc == 2) ? FR_OK : FR_TOO_MANY_OPEN_FILES; // Is there a blank entry for new object

  // The object has been opened. Reject any open against writing file and all write mode open
  return (acc || mFiles[i].ctr == 0x100) ? FR_LOCKED : FR_OK;
  }
//}}}
//{{{
UINT cFatFs::incFileLock (cDirectory* directory, int acc) {

  UINT i;
  for (i = 0; i < FS_LOCK; i++)
    // Find the object
    if (mFiles[i].mFs == directory->mFs && mFiles[i].clu == directory->mStartCluster && mFiles[i].idx == directory->mIndex)
      break;

  if (i == FS_LOCK) {
    /* Not opened. Register it as new. */
    for (i = 0; i < FS_LOCK && mFiles[i].mFs; i++) ;
    if (i == FS_LOCK)
      return 0;  /* No free entry to register (int err) */

    mFiles[i].mFs = directory->mFs;
    mFiles[i].clu = directory->mStartCluster;
    mFiles[i].idx = directory->mIndex;
    mFiles[i].ctr = 0;
    }

  if (acc && mFiles[i].ctr)
    return 0;  /* Access violation (int err) */

  mFiles[i].ctr = acc ? 0x100 : mFiles[i].ctr + 1;  /* Set semaphore value */

  return i + 1;
  }
//}}}
//{{{
FRESULT cFatFs::decFileLock (UINT i) {

  if (--i < FS_LOCK) {
    /* Shift index number origin from 0 */
    WORD n = mFiles[i].ctr;
    if (n == 0x100)
      n = 0;    /* If write mode open, delete the entry */
    if (n)
      n--;         /* Decrement read mode open count */

    mFiles[i].ctr = n;
    if (!n)
      mFiles[i].mFs = 0;  /* Delete the entry if open count gets zero */

    return FR_OK;
    }
  else
    return FR_INT_ERR;     /* Invalid index nunber */
  }
//}}}
//{{{
void cFatFs::clearFileLock() {

  for (UINT i = 0; i < FS_LOCK; i++)
    if (mFiles[i].mFs == this)
      mFiles[i].mFs = 0;
  }
//}}}
//}}}

// cDirectory
//{{{
FRESULT cDirectory::open (const char* path) {

  FRESULT result = cFatFs::instance()->findVolume (&mFs, 0);
  if (result == FR_OK) {
    WCHAR longFileName[(MAX_LFN + 1) * 2];
    mLongFileName = longFileName;
    result = followPath (path);
    if (result == FR_OK) {
      if (mDirShortFileName) {
        // not itself */
        if (mDirShortFileName[DIR_Attr] & AM_DIR) // subDir
          mStartCluster = mFs->loadCluster (mDirShortFileName);
        else // file
          result = FR_NO_PATH;
        }

      if (result == FR_OK) {
        mMountId = mFs->mMountId;
        // Rewind directory
        result = setIndex (0);
        if (result == FR_OK) {
          if (mStartCluster) {
            // lock subDir
            mLockId = mFs->incFileLock (this, 0);
            if (!mLockId)
              result = FR_TOO_MANY_OPEN_FILES;
            }
          else // root directory not locked
            mLockId = 0;
          }
        }
      }

    if (result == FR_NO_FILE)
      result = FR_NO_PATH;
    }

  if (result != FR_OK)
    mFs = 0;

  mFs->unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cDirectory::read (cFileInfo& fileInfo) {

  FRESULT result = validateDir();
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    mLongFileName = longFileName;
    result = read (0);
    if (result == FR_NO_FILE) {
      // Reached end of directory
      mSector = 0;
      result = FR_OK;
      }

    if (result == FR_OK) {
      // valid entry is found, get the object information */
      getFileInfo (fileInfo);

      // Increment index for next */
      result = next (0);
      if (result == FR_NO_FILE) {
        mSector = 0;
        result = FR_OK;
        }
      }
    }

  mFs->unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cDirectory::findfirst (cFileInfo& fileInfo, const char* path, const char* pattern) {

  mPattern = pattern;

  FRESULT result = open (path);
  if (result == FR_OK)
    result = findnext (fileInfo);

  return result;
  }
//}}}
//{{{
FRESULT cDirectory::findnext (cFileInfo& fileInfo) {

  FRESULT result;

  for (;;) {
    result = read (fileInfo);
    if (result != FR_OK || !fileInfo.mShortFileName[0])
      break;

    // match longFileName
    if (matchPattern (mPattern, fileInfo.mLongFileName, 0, 0))
      break;

    // match sfn
    if (matchPattern (mPattern, fileInfo.mShortFileName, 0, 0))
      break;
    }

  return result;
  }
//}}}
//{{{
FRESULT cDirectory::close() {

  FRESULT result = validateDir();
  if (result == FR_OK) {
    cFatFs* fatFs = mFs;

    if (mLockId)
      // Decrement sub-directory open counter
      result = mFs->decFileLock (mLockId);

    if (result == FR_OK)
      // Invalidate directory object
      mFs = 0;

    fatFs->unlock (FR_OK);
    }

  return result;
  }
//}}}
//{{{  cDirectory private members
//{{{
FRESULT cDirectory::validateDir() {

  if (!mFs ||
      !mFs->mFsType ||
      mFs->mMountId != mMountId ||
      (diskStatus() & STA_NOINIT))
    return FR_INVALID_OBJECT;

  // lock access to file system
  if (!mFs->lock())
    return FR_TIMEOUT;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::followPath (const char* path) {

  FRESULT result;
  BYTE *dir1, ns;

  if (*path == '/' || *path == '\\') {
    //  There is a heading separator
    path++;
    mStartCluster = 0;       /* Strip it and start from the root directory */
    }
  else
    // No heading separator
    mStartCluster = mFs->mCurDirSector;      /* Start from the current directory */

  if ((UINT)*path < ' ') {
    // Null path name is the origin directory itself
    result = setIndex (0);
    mDirShortFileName = 0;
    }

  else {
    for (;;) {
      result = createName (&path); /* Get a segment name of the path */
      if (result != FR_OK)
        break;
      result = find();       /* Find an object with the sagment name */
      ns = mShortFileName[NSFLAG];
      if (result != FR_OK) {
        /* Failed to find the object */
        if (result == FR_NO_FILE) {
          /* Object is not found */
          if (ns & NS_DOT) {
            /* If dot entry is not exist, */
            mStartCluster = 0;
            mDirShortFileName = 0;  /* it is the root directory and stay there */
            if (!(ns & NS_LAST))
              continue;  /* Continue to follow if not last segment */
            result = FR_OK;  /* Ended at the root directroy. Function completed. */
            }
          else if (!(ns & NS_LAST))
            result = FR_NO_PATH;  /* Adirectoryust error code if not last segment */
          }
        break;
        }

      if (ns & NS_LAST)
        break;      /* Last segment matched. Function completed. */

      dir1 = mDirShortFileName;
      if (!(dir1[DIR_Attr] & AM_DIR)) {
        /* It is not a sub-directory and cannot follow */
        result = FR_NO_PATH;
        break;
        }

      mStartCluster = mFs->loadCluster(dir1);
      }
    }

  return result;
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
    // Strip trailing spaces and dots */
    w = longFileName[di - 1];
    if (w != ' ' && w != '.')
      break;
    di--;
    }
  if (!di)
    return FR_INVALID_NAME;  /* Reject nul string */

  // longFileName is created
  longFileName[di] = 0;

  // Create SFN in directory form
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
      // Remove spaces and dots */
      cf |= NS_LOSS | NS_LFN;
      continue;
      }

    if (i >= ni || si == di) {
      // Extension or end of SFN
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

    if (w >= 0x80) {
      // Non ASCII character,  Unicode -> OEM code */
      w = convertToFromUnicode (w, 0);
      if (w)
        w = kExCvt[w - 0x80]; /* Convert extended character to upper (SBCS) */

      // Force create LFN entry
      cf |= NS_LFN;
      }

    if (!w || strchr ("+,;=[]", w)) {
      // Replace illegal characters for SFN
      w = '_';
      cf |= NS_LOSS | NS_LFN; /* Lossy conversion */
      }
    else {
      if (IS_UPPER (w))
        // ASCII large capital
        b |= 2;
      else if (IS_LOWER (w)) {
        // ASCII small capital
        b |= 1;
        w -= 0x20;
        }
      }
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

  // SFN created
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
  if (cluster == 1 || cluster >= mFs->mNumFatEntries)  /* Check start cluster range */
    return FR_INT_ERR;
  if (!cluster && mFs->mFsType == FS_FAT32) /* Replace cluster# 0 with root cluster# if in FAT32 */
    cluster = mFs->mDirBase;

  DWORD sect1;
  if (cluster == 0) {
    // static table (root-directory in FAT12/16)
    if (index >= mFs->mNumRootdir) // index out of range
      return FR_INT_ERR;
    sect1 = mFs->mDirBase;
    }
  else {
    // dynamic table (root-directory in FAT32 or sub-directory)
    UINT ic = SECTOR_SIZE / SZ_DIRE * mFs->mSectorsPerCluster;  // entries per cluster
    while (index >= ic) {
      // follow cluster chain,  get next cluster
      cluster = mFs->getFat (cluster);
      if (cluster == 0xFFFFFFFF)
        return FR_DISK_ERR;
      if (cluster < 2 || cluster >= mFs->mNumFatEntries) // reached end of table or internal error
        return FR_INT_ERR;
      index -= ic;
      }
    sect1 = mFs->clusterToSector (cluster);
    }

  mCluster = cluster;
  if (!sect1)
    return FR_INT_ERR;

  // Sector# of the directory entry
  mSector = sect1 + index / (SECTOR_SIZE / SZ_DIRE);

  // Ptr to the entry in the sector
  mDirShortFileName = mFs->mWindowBuffer + (index % (SECTOR_SIZE / SZ_DIRE)) * SZ_DIRE;

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
      if (i >= mFs->mNumRootdir) /* Report EOT if it reached end of static table */
        return FR_NO_FILE;
      }
    else {
      /* Dynamic table */
      if (((i / (SECTOR_SIZE / SZ_DIRE)) & (mFs->mSectorsPerCluster - 1)) == 0) {
        /* Cluster changed? */
        DWORD cluster = mFs->getFat (mCluster);        /* Get next cluster */
        if (cluster <= 1)
          return FR_INT_ERR;
        if (cluster == 0xFFFFFFFF)
          return FR_DISK_ERR;
        if (cluster >= mFs->mNumFatEntries) {
          /* If it reached end of dynamic table, */
          if (!stretch)
            return FR_NO_FILE;      /* If do not stretch, report EOT */
          cluster = mFs->createChain (mCluster);   /* Stretch cluster chain */
          if (cluster == 0)
            return FR_DENIED;      /* No free cluster */
          if (cluster == 1)
            return FR_INT_ERR;
          if (cluster == 0xFFFFFFFF)
            return FR_DISK_ERR;

          /* Clean-up stretched table */
          if (mFs->syncWindow())
            return FR_DISK_ERR;/* Flush disk access window */

          /* Clear window buffer */
          memset (mFs->mWindowBuffer, 0, SECTOR_SIZE);

          /* Cluster start sector */
          mFs->mWindowSector = mFs->clusterToSector (cluster);

          UINT c;
          for (c = 0; c < mFs->mSectorsPerCluster; c++) {
            /* Fill the new cluster with 0 */
            mFs->mWindowFlag = 1;
            if (mFs->syncWindow())
              return FR_DISK_ERR;
            mFs->mWindowSector++;
            }
          mFs->mWindowSector -= c;           /* Rewind window offset */
          }

        /* Initialize data for new cluster */
        mCluster = cluster;
        mSector = mFs->clusterToSector (cluster);
        }
      }
    }

  /* Current index */
  mIndex = (WORD)i;

  /* Current entry in the window */
  mDirShortFileName = mFs->mWindowBuffer + (i % (SECTOR_SIZE / SZ_DIRE)) * SZ_DIRE;

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
    FRESULT result;
    mShortFileName[NSFLAG] = 0;

    // Find only SFN
    mLongFileName = 0;
    for (n = 1; n < 100; n++) {
      generateNumberedName (mShortFileName, sn, longFileName, n);

      // Check if the name collides with existing SFN
      result = find();
      if (result != FR_OK)
        break;
      }
    if (n == 100)
      return FR_DENIED;   /* Abort if too many collisions */
    if (result != FR_NO_FILE)
      return result;  /* Abort if the result is other than 'not collided' */

    mShortFileName[NSFLAG] = sn[NSFLAG];
    mLongFileName = longFileName;
    }

  if (sn[NSFLAG] & NS_LFN) {
    // When longFileName is to be created, allocate entries for an SFN + longFileNames
    for (n = 0; longFileName[n]; n++) ;
    nent = (n + 25) / 13;
    }
  else // Otherwise allocate an entry for an SFN
    nent = 1;

  FRESULT result = allocate (nent);
  if (result == FR_OK && --nent) {
    /* Set longFileName entry if needed */
    result = setIndex (mIndex - nent);
    if (result == FR_OK) {
      BYTE sum = sumSfn (mShortFileName);
      do {
        // Store longFileName entries in bottom first
        result = mFs->moveWindow (mSector);
        if (result != FR_OK)
          break;
        fitLfn (mLongFileName, mDirShortFileName, (BYTE)nent, sum);
        mFs->mWindowFlag = 1;
        result = next (0);
        } while (result == FR_OK && --nent);
      }
    }

  if (result == FR_OK) {
    // Set SFN entry
    result = mFs->moveWindow (mSector);
    if (result == FR_OK) {
      memset (mDirShortFileName, 0, SZ_DIRE);  // Clean the entry
      memcpy (mDirShortFileName, mShortFileName, 11);      // Put SFN
      mDirShortFileName[DIR_NTres] = mShortFileName[NSFLAG] & (NS_BODY | NS_EXT); // Put NT flag
      mFs->mWindowFlag = 1;
      }
    }

  return result;
  }
//}}}
//{{{
FRESULT cDirectory::allocate (UINT nent) {

  UINT n;
  FRESULT result = setIndex (0);
  if (result == FR_OK) {
    n = 0;
    do {
      result = mFs->moveWindow (mSector);
      if (result != FR_OK)
        break;
      if (mDirShortFileName[0] == DDEM || mDirShortFileName[0] == 0) {
        // free entry
        if (++n == nent)
          break; /* A block of contiguous free entries is found */
        }
      else
        n = 0;          /* Not a blank entry. Restart to search */
      result = next (1);    /* Next entry with table stretch enabled */
      } while (result == FR_OK);
    }

  if (result == FR_NO_FILE)
    result = FR_DENIED; /* No directory entry to allocate */

  return result;
  }
//}}}
//{{{
FRESULT cDirectory::find() {

  BYTE c, *dir1;
  BYTE a, sum;

  FRESULT result = setIndex (0);     /* Rewind directory object */
  if (result != FR_OK)
    return result;

  BYTE ord = sum = 0xFF;
  mLongFileNameIndex = 0xFFFF; /* Reset longFileName sequence */
  do {
    result = mFs->moveWindow (mSector);
    if (result != FR_OK)
      break;

    dir1 = mDirShortFileName;          /* Ptr to the directory entry of current index */
    c = dir1[DIR_Name];
    if (c == 0) {
      result = FR_NO_FILE;
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
          ord = (c == ord && sum == dir1[LDIR_Chksum] && compareLfn (mLongFileName, dir1)) ? ord - 1 : 0xFF;
          }
        }
      else {          /* An SFN entry is found */
        if (!ord && sum == sumSfn (dir1))
          break; /* longFileName matched? */
        if (!(mShortFileName[NSFLAG] & NS_LOSS) && !memcmp (dir1, mShortFileName, 11))
          break;  /* SFN matched? */
        ord = 0xFF;
        mLongFileNameIndex = 0xFFFF; /* Reset longFileName sequence */
        }
      }

    result = next (0);    /* Next entry */
    } while (result == FR_OK);

  return result;
  }
//}}}
//{{{
FRESULT cDirectory::read (int vol) {

  BYTE a, c, *dir1;
  BYTE ord = 0xFF;
  BYTE sum = 0xFF;

  FRESULT result = FR_NO_FILE;
  while (mSector) {
    result = mFs->moveWindow (mSector);
    if (result != FR_OK)
      break;
    dir1 = mDirShortFileName;          /* Ptr to the directory entry of current index */
    c = dir1[DIR_Name];
    if (c == 0) {
      result = FR_NO_FILE;
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
        ord = (c == ord && sum == dir1[LDIR_Chksum] && pickLfn (mLongFileName, dir1)) ? ord - 1 : 0xFF;
        }
      else {
        /* An SFN entry is found */
        if (ord || sum != sumSfn (dir1)) /* Is there a valid longFileName? */
          mLongFileNameIndex = 0xFFFF;   /* It has no longFileName. */
        break;
        }
      }

    result = next (0);        /* Next entry */
    if (result != FR_OK)
      break;
    }

  if (result != FR_OK)
    mSector = 0;

  return result;
  }
//}}}
//{{{
FRESULT cDirectory::remove() {

  // SFN index
  UINT i = mIndex;

  FRESULT result = setIndex ((mLongFileNameIndex == 0xFFFF) ? i : mLongFileNameIndex); /* Goto the SFN or top of the longFileName entries */
  if (result == FR_OK) {
    do {
      result = mFs->moveWindow (mSector);
      if (result != FR_OK)
        break;
      memset (mDirShortFileName, 0, SZ_DIRE); /* Clear and mark the entry "deleted" */

      *mDirShortFileName = DDEM;
      mFs->mWindowFlag = 1;
      if (mIndex >= i)
        break;  /* When reached SFN, all entries of the object has been deleted. */

      result = next (0);    /* Next entry */
      } while (result == FR_OK);

    if (result == FR_NO_FILE)
      result = FR_INT_ERR;
    }

  return result;
  }
//}}}

//{{{
void cDirectory::getFileInfo (cFileInfo& fileInfo) {

  UINT i;
  char *p, c;
  WCHAR w;

  p = fileInfo.mShortFileName;
  if (mSector) {
    // Get sfn
    BYTE* dir1 = mDirShortFileName;
    i = 0;
    while (i < 11) {
      // Copy name body and extension
      c = (char)dir1[i++];
      if (c == ' ')
        continue;
      if (c == RDDEM)
        c = (char)DDEM;  // Restore replaced DDEM character
      if (i == 9)
        *p++ = '.';       // Insert a . if extension is exist
      if (IS_UPPER (c) && (dir1[DIR_NTres] & (i >= 9 ? NS_EXT : NS_BODY)))
        c += 0x20;  // To lower
      *p++ = c;
      }

    fileInfo.mAttribute = dir1[DIR_Attr];              // Attribute
    fileInfo.mFileSize = LD_DWORD (dir1 + DIR_FileSize); // Size
    fileInfo.mDate = LD_WORD (dir1 + DIR_WrtDate);   // Date
    fileInfo.mTime = LD_WORD (dir1 + DIR_WrtTime);   // Time
    }

  // terminate sfn string
  *p = 0;

  i = 0;
  p = fileInfo.mLongFileName;
  if (mSector && fileInfo.mLongFileNameSize && mLongFileNameIndex != 0xFFFF) {
    // get longFileName if available
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
      p[i++] = (char)w;
      }

    // terminate longFileName string
    p[i] = 0;
    }
  }
//}}}
//}}}

// cFile
//{{{
cFile::cFile() {
  fileBuffer = (BYTE*)malloc (SECTOR_SIZE);
  }
//}}}
//{{{
cFile::~cFile() {
  free (fileBuffer);
  }
//}}}
//{{{
FRESULT cFile::open (const char* path, BYTE mode) {

  DWORD dw, cl;

  mFs = 0;
  cDirectory directory;
  mode &= FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW;
  FRESULT result = cFatFs::instance()->findVolume (&directory.mFs, (BYTE)(mode & ~FA_READ));
  if (result == FR_OK) {
    WCHAR longFileName [(MAX_LFN + 1) * 2];
    directory.mLongFileName = longFileName;
    result = directory.followPath (path);
    BYTE* dir = directory.mDirShortFileName;
    if (result == FR_OK) {
      if (!dir)
        /* Default directory itself */
        result = FR_INVALID_NAME;
      else
        result = directory.mFs->checkFileLock (&directory, (mode & ~FA_READ) ? 1 : 0);
      }

    /* Create or Open a file */
    if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
      if (result != FR_OK) {
        /* No file, create new */
        if (result == FR_NO_FILE)
          /* There is no file to open, create a new entry */
          result = mFs->enquireFileLock() ? directory.registerNewEntry() : FR_TOO_MANY_OPEN_FILES;
        mode |= FA_CREATE_ALWAYS;   /* File is created */
        dir = directory.mDirShortFileName;         /* New entry */
        }
      else {
        /* Any object is already existing */
        if (dir[DIR_Attr] & (AM_RDO | AM_DIR))
          /* Cannot overwrite it (R/O or DIR) */
          result = FR_DENIED;
        else if (mode & FA_CREATE_NEW)
          /* Cannot create as new file */
          result = FR_EXIST;
        }

      if (result == FR_OK && (mode & FA_CREATE_ALWAYS)) {
        /* Truncate it if overwrite mode */
        dw = getFatTime();       /* Created time */
        ST_DWORD (dir + DIR_CrtTime, dw);
        dir[DIR_Attr] = 0;        /* result attribute */
        ST_DWORD (dir + DIR_FileSize, 0);/* size = 0 */
        cl = directory.mFs->loadCluster (dir);    /* Get start cluster */
        storeCluster (dir, 0);       /* cluster = 0 */
        directory.mFs->mWindowFlag = 1;
        if (cl) {
          /* Remove the cluster chain if exist */
          dw = directory.mFs->mWindowSector;
          result = directory.mFs->removeChain (cl);
          if (result == FR_OK) {
            directory.mFs->mLastCluster = cl - 1;
            /* Reuse the cluster hole */
            result = directory.mFs->moveWindow (dw);
            }
          }
        }     }

    else {
      /* Open an existing file */
      if (result == FR_OK) {
        /* Follow succeeded */
        if (dir[DIR_Attr] & AM_DIR)
          /* It is a directory */
          result = FR_NO_FILE;
        else if ((mode & FA_WRITE) && (dir[DIR_Attr] & AM_RDO))
          /* R/O violation */
          result = FR_DENIED;
        }
      }

    if (result == FR_OK) {
      if (mode & FA_CREATE_ALWAYS)
        /* Set file change flag if created or overwritten */
        mode |= FA__WRITTEN;
      mDirSectorNum = directory.mFs->mWindowSector;  /* Pointer to the directory entry */
      mDirPtr = dir;
      mLockId = directory.mFs->incFileLock (&directory, (mode & ~FA_READ) ? 1 : 0);
      if (!mLockId)
        result = FR_INT_ERR;
      }

    if (result == FR_OK) {
      mFlag = mode;                          // File access mode
      mError = 0;                              // Clear error flag
      mStartCluster = directory.mFs->loadCluster (dir);  // File start cluster
      mFileSize = LD_DWORD (dir + DIR_FileSize); // File size
      mFilePtr = 0;                             // File pointer
      mCachedSector = 0;
      mClusterTable = 0;                            // Normal seek mode
      mFs = directory.mFs;                    // Validate file object
      mMountId = mFs->mMountId;
      }
    }

  directory.mFs->unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFile::lseek (DWORD fileOffset) {

  DWORD clst, bcs, nsect, ifptr;
  DWORD cl, pcl, ncl, tcl, dsc, tlen, ulen, *tbl;

  FRESULT result = validateFile();
  if (result != FR_OK) {
    //{{{  error
    mFs->unlock (result);
    return result;
    }
    //}}}
  if (mError) {
    //{{{  error
    mFs->unlock ((FRESULT)mError);
    return (FRESULT)mError;
    }
    //}}}

  if (mClusterTable) {
    // fast seek
    if (fileOffset == CREATE_LINKMAP) {
      //{{{  create CLMT
      tbl = mClusterTable;
      tlen = *tbl++;
      ulen = 2;  // Given table size and required table size */

      // Top of the chain */
      cl = mStartCluster;
      if (cl) {
        do {
          // Get a fragment */
          tcl = cl;
          ncl = 0;
          ulen += 2; // Top, length and used items */
          do {
            pcl = cl; ncl++;
            cl = mFs->getFat (cl);
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
          } while (cl < mFs->mNumFatEntries);  // Repeat until end of chain
        }

      // Number of items used
      *mClusterTable = ulen;
      if (ulen <= tlen) // Terminate table
        *tbl = 0;
      else // Given table size is smaller than required
        result = FR_NOT_ENOUGH_CORE;
      }
      //}}}
    else {
      //{{{  fast seek
      if (fileOffset > mFileSize)    /* Clip offset at the file size */
        fileOffset = mFileSize;

      mFilePtr = fileOffset;       /* Set file pointer */
      if (fileOffset) {
        mCluster = clmtCluster (fileOffset - 1);
        dsc = mFs->clusterToSector (mCluster);
        if (!dsc)
          ABORT (FR_INT_ERR);
        dsc += (fileOffset - 1) / SECTOR_SIZE & (mFs->mSectorsPerCluster - 1);
        if (mFilePtr % SECTOR_SIZE && dsc != mCachedSector) {
          /* Refill sector cache if needed */
          if (mFlag & FA__DIRTY) {
            /* Write-back dirty sector cache */
            if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
              ABORT (FR_DISK_ERR);
            mFlag &= ~FA__DIRTY;
            }
          if (diskRead (fileBuffer, dsc, 1) != RES_OK) /* Load current sector */
            ABORT (FR_DISK_ERR);
          mCachedSector = dsc;
          }
        }
      }
      //}}}
    }

  else {
    //{{{  normal Seek
    if (fileOffset > mFileSize && !(mFlag & FA_WRITE))
      fileOffset = mFileSize;

    ifptr = mFilePtr;
    mFilePtr = nsect = 0;

    if (fileOffset) {
      // Cluster size (byte)
      bcs = (DWORD)mFs->mSectorsPerCluster * SECTOR_SIZE;
      if (ifptr > 0 && (fileOffset - 1) / bcs >= (ifptr - 1) / bcs) {
        //{{{  When seek to same or following cluster
        mFilePtr = (ifptr - 1) & ~(bcs - 1);  /* start from the current cluster */
        fileOffset -= mFilePtr;
        clst = mCluster;
        }
        //}}}
      else {
        //{{{  When seek to back cluster, start from the first cluster
        clst = mStartCluster;
        if (clst == 0) {
          /* If no cluster chain, create a new chain */
          clst = mFs->createChain (0);
          if (clst == 1)
            ABORT (FR_INT_ERR);
          if (clst == 0xFFFFFFFF)
            ABORT (FR_DISK_ERR);
          mStartCluster = clst;
          }
        mCluster = clst;
        }
        //}}}

      if (clst != 0) {
        while (fileOffset > bcs) {
          //{{{  Cluster following loop
          if (mFlag & FA_WRITE) {
            /* Check if in write mode or not */
            clst = mFs->createChain (clst);  /* Force stretch if in write mode */
            if (clst == 0) {
              /* When disk gets full, clip file size */
              fileOffset = bcs;
              break;
              }
            }
          else
            clst = mFs->getFat (clst); /* Follow cluster chain if not in write mode */

          if (clst == 0xFFFFFFFF)
            ABORT (FR_DISK_ERR);
          if (clst <= 1 || clst >= mFs->mNumFatEntries)
            ABORT (FR_INT_ERR);

          mCluster = clst;
          mFilePtr += bcs;
          fileOffset -= bcs;
          }
          //}}}

        mFilePtr += fileOffset;
        if (fileOffset % SECTOR_SIZE) {
          /* Current sector */
          nsect = mFs->clusterToSector (clst);
          if (!nsect)
            ABORT (FR_INT_ERR);
          nsect += fileOffset / SECTOR_SIZE;
          }
        }
      }

    if (mFilePtr % SECTOR_SIZE && nsect != mCachedSector) {
      if (mFlag & FA__DIRTY) {
        //{{{  Write-back dirty sector cache
        if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
          ABORT (FR_DISK_ERR);
        mFlag &= ~FA__DIRTY;
        }
        //}}}

      // Fill sector cache
      if (diskRead (fileBuffer, nsect, 1) != RES_OK)
        ABORT (FR_DISK_ERR);
      mCachedSector = nsect;
      }

    if (mFilePtr > mFileSize) {
      //{{{  Set file change flag if the file size is extended
      mFileSize = mFilePtr;
      mFlag |= FA__WRITTEN;
      }
      //}}}
    }
    //}}}

  mFs->unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFile::read (void* readBuffer, int bytesToRead, int& bytesRead) {

  bytesRead = 0;
  FRESULT result = validateFile();
  if (result != FR_OK) {
    //{{{  error
    mFs->unlock (result);
    return result;
    }
    //}}}
  if (mError) {
    //{{{  error
    mFs->unlock ((FRESULT)mError);
    return (FRESULT)mError;
    }
    //}}}
  if (!(mFlag & FA_READ)) {
    //{{{  error
    mFs->unlock (FR_DENIED);
    return FR_DENIED;
    }
    //}}}

  // truncate bytesToRead by fileSize
  int remain = mFileSize - mFilePtr;
  if (bytesToRead > remain)
    bytesToRead = remain;

  auto readBufferPtr = (BYTE*)readBuffer;
  int readCount;
  for (; bytesToRead; readBufferPtr += readCount, mFilePtr += readCount, bytesRead += readCount, bytesToRead -= readCount) {
    // repeat until all bytesToRead read
    if ((mFilePtr % SECTOR_SIZE) == 0) {
      // on sector boundary, sector offset in cluster
      BYTE csect = (BYTE)(mFilePtr / SECTOR_SIZE & (mFs->mSectorsPerCluster - 1));
      if (!csect) {
      //{{{  on cluster boundary
      DWORD readCluster;
      if (mFilePtr == 0) // at the top of the file, Follow from the origin
        readCluster = mStartCluster;
      else if (mClusterTable) // get cluster# from the CLMT
        readCluster = clmtCluster (mFilePtr);
      else // follow cluster chain on the FAT
        readCluster = mFs->getFat (mCluster);

      if (readCluster < 2)
        ABORT (FR_INT_ERR);
      if (readCluster == 0xFFFFFFFF)
        ABORT (FR_DISK_ERR);

      // Update current cluster
      mCluster = readCluster;
      }
      //}}}

      // get current sector
      DWORD readSector = mFs->clusterToSector (mCluster);
      if (!readSector)
        ABORT (FR_INT_ERR);

      readSector += csect;
      UINT contiguousClusters = bytesToRead / SECTOR_SIZE;
      if (contiguousClusters) {
        //{{{  read contiguousClusters sectors directly into readBuffer
        if (csect + contiguousClusters > mFs->mSectorsPerCluster) // Clip at cluster boundary
          contiguousClusters = mFs->mSectorsPerCluster - csect;

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
    readCount = SECTOR_SIZE - ((UINT)mFilePtr % SECTOR_SIZE);
    if (readCount > bytesToRead)
      readCount = bytesToRead;
    memcpy (readBufferPtr, &fileBuffer[mFilePtr % SECTOR_SIZE], readCount);
    }

  mFs->unlock (FR_OK);
  return FR_OK;
  }
//}}}
//{{{
FRESULT cFile::write (const void *buff, UINT btw, UINT* bw) {

  DWORD clst, sect;
  UINT wcnt, cc;
  const BYTE *wbuff = (const BYTE*)buff;
  BYTE csect;

  *bw = 0;  /* Clear write byte counter */

  FRESULT result = validateFile();
  if (result != FR_OK) {
    mFs->unlock (result);
    return result;
    }
  if (mError) {
    mFs->unlock ((FRESULT)mError);
    return (FRESULT)mError;
    }
  if (!(mFlag & FA_WRITE)) {
    mFs->unlock (FR_DENIED);
    return FR_DENIED;
    }

  /* File size cannot reach 4GB */
  if (mFilePtr + btw < mFilePtr)
    btw = 0;

  for ( ;  btw; wbuff += wcnt, mFilePtr += wcnt, *bw += wcnt, btw -= wcnt) {
    /* Repeat until all data written */
    if ((mFilePtr % SECTOR_SIZE) == 0) {
      /* On the sector boundary? , Sector offset in the cluster */
      csect = (BYTE)(mFilePtr / SECTOR_SIZE & (mFs->mSectorsPerCluster - 1));
      if (!csect) {
        /* On the cluster boundary? */
        if (mFilePtr == 0) {
          /* On the top of the file? */
          clst = mStartCluster;
          /* Follow from the origin */
          if (clst == 0)
            /* When no cluster is allocated, */
            clst = mFs->createChain (0); /* Create a new cluster chain */
          }
        else {
          /* Middle or end of the file */
          if (mClusterTable)
            clst = clmtCluster (mFilePtr);  /* Get cluster# from the CLMT */
          else
            clst = mFs->createChain (mCluster); /* Follow or stretch cluster chain on the FAT */
          }

        if (clst == 0)
          break;   /* Could not allocate a new cluster (disk full) */
        if (clst == 1)
          ABORT (FR_INT_ERR);
        if (clst == 0xFFFFFFFF)
          ABORT (FR_DISK_ERR);

        mCluster = clst;     /* Update current cluster */
        if (mStartCluster == 0)
          mStartCluster = clst; /* Set start cluster if the first write */
        }

      if (mFlag & FA__DIRTY) {
        /* Write-back sector cache */
        if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
          ABORT (FR_DISK_ERR);
        mFlag &= ~FA__DIRTY;
        }

      sect = mFs->clusterToSector (mCluster); /* Get current sector */
      if (!sect)
        ABORT (FR_INT_ERR);
      sect += csect;

      cc = btw / SECTOR_SIZE;      /* When remaining bytes >= sector size, */
      if (cc) {           /* Write maximum contiguous sectors directly */
        if (csect + cc > mFs->mSectorsPerCluster) /* Clip at cluster boundary */
          cc = mFs->mSectorsPerCluster - csect;
        if (diskWrite (wbuff, sect, cc) != RES_OK)
          ABORT (FR_DISK_ERR);
        if (mCachedSector - sect < cc) { /* Refill sector cache if it gets invalidated by the direct write */
          memcpy (fileBuffer, wbuff + ((mCachedSector - sect) * SECTOR_SIZE), SECTOR_SIZE);
          mFlag &= ~FA__DIRTY;
          }
        wcnt = SECTOR_SIZE * cc;   /* Number of bytes transferred */
        continue;
        }

      if (mCachedSector != sect) {
        /* Fill sector cache with file data */
        if (mFilePtr < mFileSize && diskRead (fileBuffer, sect, 1) != RES_OK)
          ABORT (FR_DISK_ERR);
        }
      mCachedSector = sect;
      }

    /* Put partial sector into file I/O buffer */
    wcnt = SECTOR_SIZE - ((UINT)mFilePtr % SECTOR_SIZE);
    if (wcnt > btw)
      wcnt = btw;

    /* Fit partial sector */
    memcpy (&fileBuffer[mFilePtr % SECTOR_SIZE], wbuff, wcnt);
    mFlag |= FA__DIRTY;
    }

  /* Update file size if needed */
  if (mFilePtr > mFileSize)
    mFileSize = mFilePtr;

  /* Set file change flag */
  mFlag |= FA__WRITTEN;

  mFs->unlock (FR_OK);
  return FR_OK;
  }
//}}}
//{{{
FRESULT cFile::truncate() {

  DWORD ncl;
  FRESULT result = validateFile();
  if (result == FR_OK) {
    if (mError)             /* Check error */
      result = (FRESULT)mError;
    else if (!(mFlag & FA_WRITE))   /* Check access mode */
      result = FR_DENIED;
    }

  if (result == FR_OK) {
    if (mFileSize > mFilePtr) {
      mFileSize = mFilePtr; /* Set file size to current R/W point */
      mFlag |= FA__WRITTEN;
      if (mFilePtr == 0) {
        /* When set file size to zero, remove entire cluster chain */
        result = mFs->removeChain (mStartCluster);
        mStartCluster = 0;
        }
      else {
        /* When truncate a part of the file, remove remaining clusters */
        ncl = mFs->getFat (mCluster);
        result = FR_OK;
        if (ncl == 0xFFFFFFFF)
          result = FR_DISK_ERR;
        if (ncl == 1)
          result = FR_INT_ERR;
        if (result == FR_OK && ncl < mFs->mNumFatEntries) {
          result = mFs->putFat (mCluster, 0x0FFFFFFF);
          if (result == FR_OK)
            result = mFs->removeChain (ncl);
          }
        }

      if (result == FR_OK && (mFlag & FA__DIRTY)) {
        if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK)
          result = FR_DISK_ERR;
        else
          mFlag &= ~FA__DIRTY;
        }
      }
    if (result != FR_OK)
      mError = (FRESULT)result;
    }

  mFs->unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFile::sync() {

  DWORD tm;
  BYTE* dir;

  FRESULT result = validateFile();
  if (result == FR_OK) {
    if (mFlag & FA__WRITTEN) {
      if (mFlag & FA__DIRTY) {
        if (diskWrite (fileBuffer, mCachedSector, 1) != RES_OK) {
          mFs->unlock (FR_DISK_ERR);
          return FR_DISK_ERR;
          }
        mFlag &= ~FA__DIRTY;
        }

      // Update the directory entry
      result = mFs->moveWindow (mDirSectorNum);
      if (result == FR_OK) {
        dir = mDirPtr;
        dir[DIR_Attr] |= AM_ARC;              // Set archive bit
        ST_DWORD (dir + DIR_FileSize, mFileSize);  // Update file size
        storeCluster (dir, mStartCluster);           // Update start cluster
        tm = getFatTime();                    // Update updated time
        ST_DWORD (dir + DIR_WrtTime, tm);
        ST_WORD (dir + DIR_LstAccDate, 0);
        mFlag &= ~FA__WRITTEN;
        mFs->mWindowFlag = 1;
        result = mFs->syncFs();
        }
      }
    }

  mFs->unlock (result);
  return result;
  }
//}}}
//{{{
FRESULT cFile::close() {

  FRESULT result = sync();
  if (result == FR_OK) {
    result = validateFile();
    if (result == FR_OK) {
      cFatFs* fatFs = mFs;

      // Decrement file open counter
      result = mFs->decFileLock (mLockId);
      if (result == FR_OK)
        // Invalidate file object
        mFs = 0;

      fatFs->unlock (FR_OK);
      }
    }

  return result;
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
      file->write (buf, (UINT)i, &bw);
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
  if (putBuffer.idx >= 0 && write (putBuffer.buf, (UINT)putBuffer.idx, &nw) == FR_OK && (UINT)putBuffer.idx == nw)
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
  if (putBuffer.idx >= 0 && write (putBuffer.buf, (UINT)putBuffer.idx, &nw) == FR_OK && (UINT)putBuffer.idx == nw)
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

  if (putBuffer.idx >= 0 && putBuffer.file->write (putBuffer.buf, (UINT)putBuffer.idx, &nw) == FR_OK && (UINT)putBuffer.idx == nw)
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
    // Read characters until buffer gets filled */
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
  return n ? buff : 0; // When no data read (eof or error), return with error
  }
//}}}
//{{{  cFile private members
//{{{
FRESULT cFile::validateFile() {

  if (!mFs ||
      !mFs->mFsType ||
      mFs->mMountId != mMountId ||
      (diskStatus () & STA_NOINIT))
    return FR_INVALID_OBJECT;

  // lock access to file system
  if (!mFs->lock())
    return FR_TIMEOUT;

  return FR_OK;
  }
//}}}
//{{{
DWORD cFile::clmtCluster (DWORD ofs) {

  // Top of CLMT
  DWORD* tbl = mClusterTable + 1;

  // Cluster order from top of the file
  DWORD cl = ofs / SECTOR_SIZE / mFs->mSectorsPerCluster;
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
//}}}
