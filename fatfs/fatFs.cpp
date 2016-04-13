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
#include "stdarg.h"

#include "fatFs.h"
#include "diskio.h"

#include "memory.h"
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
// Character code support macros
#define IsUpper(c)  (((c) >= 'A') && ((c) <= 'Z'))
#define IsLower(c)  (((c) >= 'a') && ((c) <= 'z'))
#define IsDigit(c)  (((c) >= '0') && ((c) <= '9'))

// Multi-byte word access macros
#define LD_WORD(ptr)   (WORD)(((WORD)*((BYTE*)(ptr)+1)<<8)|(WORD)*(BYTE*)(ptr))
#define LD_DWORD(ptr)  (DWORD)(((DWORD)*((BYTE*)(ptr)+3)<<24)|((DWORD)*((BYTE*)(ptr)+2)<<16)|((WORD)*((BYTE*)(ptr)+1)<<8)|*(BYTE*)(ptr))

#define ST_WORD(ptr,val)  *(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8)
#define ST_DWORD(ptr,val) *(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8); *((BYTE*)(ptr)+2)=(BYTE)((DWORD)(val)>>16); *((BYTE*)(ptr)+3)=(BYTE)((DWORD)(val)>>24)

// unlock macros
#define LEAVE_FF(fs, res) { fs->unlock (res); return res; }
#define ABORT(fs, res)    { err = (BYTE)(res); fs->unlock (res); return res; }
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
static WCHAR wideToUpperCase (WCHAR chr) {

  int i;
  for (i = 0; kTableLower[i] && chr != kTableLower[i]; i++);
  return kTableLower[i] ? kTableUpper[i] : chr;
  }
//}}}
//{{{
static WCHAR get_achar (const TCHAR** ptr) {

  WCHAR chr = (BYTE)*(*ptr)++;
  if (IsLower (chr))
    chr -= 0x20; // To upper ASCII char

  if (chr >= 0x80)
    chr = kExCvt[chr - 0x80]; // To upper SBCS extended char

  return chr;
  }
//}}}
//{{{
static int matchPattern (const TCHAR* pat, const TCHAR* nam, int skip, int inf) {

  while (skip--) {
    // Pre-skip name chars
    if (!get_achar (&nam))
      return 0; // Branch mismatched if less name chars
    }
  if (!*pat && inf)
    return 1;  // short circuit

  WCHAR nc;
  do {
    const TCHAR* pp = pat;
    const TCHAR* np = nam;  // Top of pattern and name to match
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
    WCHAR uc = LD_WORD(dir + LfnOfs[s]);  /* Pick an LFN character from the entry */
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
    WCHAR uc = LD_WORD(dir + LfnOfs[s]);    /* Pick an LFN character from the entry */
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
static void generateNumName (BYTE* dst, const BYTE* src, const WCHAR* lfn, UINT seq) {

  BYTE ns[8], c;
  UINT i, j;
  WCHAR wc;
  DWORD sr;

  memcpy (dst, src, 11);

  if (seq > 5) {
    /* On many collisions, generate a hash number instead of sequential number */
    sr = seq;
    while (*lfn) {
      /* Create a CRC */
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

  /* itoa (hexdecimal) */
  i = 7;
  do {
    c = (seq % 16) + '0';
    if (c > '9')
      c += 7;
    ns[i--] = c;
    seq /= 16;
    } while (seq);

  ns[i] = '~';

  /* Append the number */
  for (j = 0; j < i && dst[j] != ' '; j++);
  do {
    dst[j++] = (i < 8) ? ns[i++] : ' ';
    } while (j < 8);
  }
//}}}

//{{{
static void storeCluster (BYTE* dir, DWORD cluster) {

  ST_WORD(dir + DIR_FstClusLO, cluster);
  ST_WORD(dir + DIR_FstClusHI, cluster >> 16);
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
  semaphore = osSemaphoreCreate (osSemaphore (fatfs), 1);

  // non cacheable sector buffer explicitly allocated in DTCM
  windowBuffer = (BYTE*)FATFS_BUFFER;
  }
//}}}
//{{{
FRESULT cFatFs::getFree (DWORD* numClusters, DWORD* clusterSize) {

  cFatFs* dummyFs;
  FRESULT res = findVolume (&dummyFs, 0);
  if (res == FR_OK) {
    *clusterSize = csize;
    // if freeCluster is valid, return it without full cluster scan
    if (freeCluster <= numFatEntries - 2)
      *numClusters = freeCluster;
    else {
      //{{{  scan number of free clusters
      DWORD n, clst, sect, stat;
      UINT i;
      BYTE *p;

      BYTE fat = fsType;
      n = 0;
      if (fat == FS_FAT12) {
        clst = 2;
        do {
          stat = getFat (clst);
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
          } while (++clst < numFatEntries);
        }

      else {
        clst = numFatEntries;
        sect = fatbase;
        i = 0;
        p = 0;
        do {
          if (!i) {
            res = moveWindow (sect++);
            if (res != FR_OK)
              break;
            p = windowBuffer;
            i = SECTOR_SIZE;
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

      freeCluster = n;
      fsi_flag |= 1;
      *numClusters = n;
      }
      //}}}
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::getCwd (TCHAR* buff, UINT len) {

  UINT i, n;
  DWORD ccl;
  TCHAR *tp;

  *buff = 0;
  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 0);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;
    i = len;      /* Bottom of buffer (directory stack base) */
    directory.sclust = cdir;      /* Start to follow upper directory from current directory */
    while ((ccl = directory.sclust) != 0) {
      /* Repeat while current directory is a sub-directory, Get parent directory */
      res = directory.setIndex (1);
      if (res != FR_OK)
        break;

      res = directory.read (0);
      if (res != FR_OK)
        break;

      // Goto parent directory */
      directory.sclust = loadCluster (directory.dir);
      res = directory.setIndex (0);
      if (res != FR_OK)
        break;

      do {
        /* Find the entry links to the child directory */
        res = directory.read (0);
        if (res != FR_OK)
          break;
        if (ccl == loadCluster (directory.dir))
          break;  /* Found the entry */
        res = directory.next (0);
        } while (res == FR_OK);
      if (res == FR_NO_FILE)
        res = FR_INT_ERR;/* It cannot be 'not found'. */
      if (res != FR_OK)
        break;

      /* Get the directory name and push it to the buffer */
      cFileInfo fileInfo;
      fileInfo.lfname = buff;
      fileInfo.lfsize = i;
      directory.getFileInfo (&fileInfo);
      tp = fileInfo.fname;
      if (*buff)
        tp = buff;
      for (n = 0; tp[n]; n++);
      if (i < n + 3) {
        res = FR_NOT_ENOUGH_CORE;
          break;
        }
      while (n)
        buff[--i] = tp[--n];
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

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::getLabel (TCHAR* label, DWORD* vsn) {

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 0);
  if (res == FR_OK && label) {
    directory.sclust = 0;
    // Open root directory
    res = directory.setIndex (0);
    if (res == FR_OK) {
      // Get an entry with AM_VOL
      res = directory.read (1);
      if (res == FR_OK) {
        // A volume label is exist
        memcpy (label, directory.dir, 11);
        UINT k = 11;
        do {
          label[k] = 0;
          if (!k)
            break;
          } while (label[--k] == ' ');
        }

      if (res == FR_NO_FILE) {
        // No label, return nul string
        label[0] = 0;
        res = FR_OK;
        }
      }
    }

  // Get volume serial number
  if (res == FR_OK && vsn) {
    res = moveWindow (volbase);
    if (res == FR_OK) {
      UINT i = fsType == FS_FAT32 ? BS_VolID32 : BS_VolID;
      *vsn = LD_DWORD (&windowBuffer[i]);
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::setLabel (const TCHAR* label) {

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 1);
  if (res) {
    unlock (res);
    return res;
    }

  // Create a volume label in directory form */
  BYTE vn[11];
  vn[0] = 0;

  // Get name length */
  UINT sl;
  for (sl = 0; label[sl]; sl++) ;

  // Remove trailing spaces */
  for ( ; sl && label[sl - 1] == ' '; sl--) ;

  if (sl) {
    // Create volume label in directory form
    UINT i = 0;
    UINT j = 0;
    do {
      WCHAR w = (BYTE)label[i++];
      w = convertToFromUnicode (wideToUpperCase (convertToFromUnicode(w, 1)), 0);
      if (!w || strchr ("\"*+,.:;<=>\?[]|\x7F", w) || j >= (UINT)((w >= 0x100) ? 10 : 11)) {
        // Reject invalid characters for volume label
        unlock (FR_INVALID_NAME);
        return FR_INVALID_NAME;
        }

      if (w >= 0x100)
        vn[j++] = (BYTE)(w >> 8);
      vn[j++] = (BYTE)w;
      } while (i < sl);

    // Fill remaining name field
    while (j < 11) vn[j++] = ' ';
    if (vn[0] == DDEM) {
      // Reject illegal name (heading DDEM)
      unlock (FR_INVALID_NAME);
      return FR_INVALID_NAME;
      }
    }

  // Open root directory
  directory.sclust = 0;
  res = directory.setIndex (0);
  if (res == FR_OK) {
    // Get an entry with AM_VOL
    res = directory.read (1);
    if (res == FR_OK) {
      // volume label found
      if (vn[0]) {
        // Change the volume label name
        memcpy (directory.dir, vn, 11);
        DWORD tm = getFatTime();
        ST_DWORD(directory.dir + DIR_WrtTime, tm);
        }
      else // Remove the volume label
        directory.dir[0] = DDEM;
      wflag = 1;
      res = syncFs();
      }
    else {
      // No volume label is found or error
      if (res == FR_NO_FILE) {
        res = FR_OK;
        if (vn[0]) {
          // Create volume label as new, Allocate an entry for volume label
          res = directory.allocate (1);
          if (res == FR_OK) {
            // Set volume label
            memset (directory.dir, 0, SZ_DIRE);
            memcpy (directory.dir, vn, 11);
            directory.dir[DIR_Attr] = AM_VOL;
            DWORD tm = getFatTime();
            ST_DWORD(directory.dir + DIR_WrtTime, tm);
            wflag = 1;
            res = syncFs();
            }
          }
        }
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::mkDir (const TCHAR* path) {

  BYTE *dir, n;
  DWORD dsc, dcl, pcl;

  DWORD tm = getFatTime();

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;
    res = directory.followPath (path);
    if (res == FR_OK)
      res = FR_EXIST;   /* Any object with same name is already existing */
    if (res == FR_NO_FILE && (directory.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_NO_FILE) {
      /* Can create a new directory */
      dcl = createChain (0);   /* Allocate a cluster for the new directory table */
      res = FR_OK;
      if (dcl == 0)
        res = FR_DENIED;    /* No space to allocate a new cluster */
      if (dcl == 1)
        res = FR_INT_ERR;
      if (dcl == 0xFFFFFFFF)
        res = FR_DISK_ERR;

      if (res == FR_OK)  /* Flush FAT */
        res = syncWindow();

      if (res == FR_OK) {
        /* Initialize the new directory table */
        dsc = clusterToSector (dcl);
        dir = windowBuffer;
        memset(dir, 0, SECTOR_SIZE);
        memset(dir + DIR_Name, ' ', 11); /* Create "." entry */
        dir[DIR_Name] = '.';
        dir[DIR_Attr] = AM_DIR;
        ST_DWORD(dir + DIR_WrtTime, tm);
        storeCluster (dir, dcl);
        memcpy(dir + SZ_DIRE, dir, SZ_DIRE);   /* Create ".." entry */

        dir[SZ_DIRE + 1] = '.';
        pcl = directory.sclust;
        if (fsType == FS_FAT32 && pcl == dirbase)
          pcl = 0;
        storeCluster (dir + SZ_DIRE, pcl);
        for (n = csize; n; n--) {  /* Write dot entries and clear following sectors */
          winsect = dsc++;
          wflag = 1;
          res = syncWindow();
          if (res != FR_OK)
            break;
          memset(dir, 0, SECTOR_SIZE);
          }
        }

      if (res == FR_OK)
        res = directory.registerNewEntry();
      if (res != FR_OK)
        removeChain (dcl);     /* Could not register, remove cluster chain */
      else {
        dir = directory.dir;
        dir[DIR_Attr] = AM_DIR;       /* Attribute */
        ST_DWORD(dir + DIR_WrtTime, tm);  /* Created time */
        storeCluster (dir, dcl);         /* Table start cluster */
        wflag = 1;
        res = syncFs();
        }
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::chDir (const TCHAR* path) {

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 0);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;
    res = directory.followPath (path);

    if (res == FR_OK) {
      /* Follow completed */
      if (!directory.dir)
        cdir = directory.sclust;  /* Start directory itself */
      else {
        if (directory.dir[DIR_Attr] & AM_DIR)  /* Reached to the directory */
          cdir = loadCluster (directory.dir);
        else
          res = FR_NO_PATH;   /* Reached but a file */
        }
      }
    if (res == FR_NO_FILE)
      res = FR_NO_PATH;
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::stat (const TCHAR* path, cFileInfo* fileInfo) {

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 0);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;

    res = directory.followPath (path);
    if (res == FR_OK) {
      if (directory.dir) {
        if (fileInfo)
          directory.getFileInfo (fileInfo);
        }
      else
        /* It is root directory */
        res = FR_INVALID_NAME;
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::rename (const TCHAR* path_old, const TCHAR* path_new) {

  BYTE buf[21], *dir;
  DWORD dw;

  cDirectory oldDirectory;
  FRESULT res = findVolume (&oldDirectory.fs, 1);
  if (res == FR_OK) {
    cDirectory newDirectory;
    newDirectory.fs = oldDirectory.fs;

    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    oldDirectory.lfn = lfn;
    oldDirectory.fn = sfn;
    res = oldDirectory.followPath (path_old);
    if (res == FR_OK && (oldDirectory.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_OK)
      res = checkFileLock (&oldDirectory, 2);
    if (res == FR_OK) {
      // old object is found
      if (!oldDirectory.dir)  // Is root dir?
        res = FR_NO_FILE;
      else {
        memcpy (buf, oldDirectory.dir + DIR_Attr, 21); /* Save information about object except name */
        memcpy (&newDirectory, &oldDirectory, sizeof (cDirectory));    /* Duplicate the directory object */
        res = newDirectory.followPath (path_new);  /* and make sure if new object name is not conflicting */
        if (res == FR_OK)
          res = FR_EXIST;   /* The new object name is already existing */
        if (res == FR_NO_FILE) {
          /* It is a valid path and no name collision */
          res = newDirectory.registerNewEntry();
          if (res == FR_OK) {
/* Start of critical section where any interruption can cause a cross-link */
            dir = newDirectory.dir;          /* Copy information about object except name */
            memcpy (dir + 13, buf + 2, 19);
            dir[DIR_Attr] = buf[0] | AM_ARC;
            wflag = 1;
            if ((dir[DIR_Attr] & AM_DIR) && oldDirectory.sclust != newDirectory.sclust) {
              /* Update .. entry in the sub-directory if needed */
              dw = clusterToSector (loadCluster (dir));
              if (!dw)
                res = FR_INT_ERR;
              else {
                res = moveWindow (dw);
                dir = windowBuffer + SZ_DIRE * 1; /* Ptr to .. entry */
                if (res == FR_OK && dir[1] == '.') {
                  storeCluster (dir, newDirectory.sclust);
                  wflag = 1;
                  }
                }
              }

            if (res == FR_OK) {
              res = oldDirectory.remove();   /* Remove old entry */
              if (res == FR_OK)
                res = syncFs();
              }
/* End of critical section */
            }
          }
        }
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::chMod (const TCHAR* path, BYTE attr, BYTE mask) {

  BYTE* dir;

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;
    res = directory.followPath (path);

    if (res == FR_OK && (directory.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_OK) {
      dir = directory.dir;
      if (!dir)
        // root directory
        res = FR_INVALID_NAME;
      else {
        // File or sub directory
        mask &= AM_RDO|AM_HID|AM_SYS|AM_ARC;  /* Valid attribute mask */
        dir[DIR_Attr] = (attr & mask) | (dir[DIR_Attr] & (BYTE)~mask);  /* Apply attribute change */
        wflag = 1;
        res = syncFs();
        }
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::utime (const TCHAR* path, const cFileInfo* fileInfo) {

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;
    res = directory.followPath (path);
    if (res == FR_OK && (directory.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_OK) {
      BYTE* dir = directory.dir;
      if (!dir)
        // root dir
        res = FR_INVALID_NAME;
      else {
        // file or subDir
        ST_WORD(dir + DIR_WrtTime, fileInfo->ftime);
        ST_WORD(dir + DIR_WrtDate, fileInfo->fdate);
        wflag = 1;
        res = syncFs();
        }
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::unlink (const TCHAR* path) {

  BYTE *dir;
  DWORD dclst = 0;

  cDirectory directory;
  FRESULT res = findVolume (&directory.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;
    res = directory.followPath (path);
    if (res == FR_OK && (directory.fn[NSFLAG] & NS_DOT)) // Cannot remove dot entry */
      res = FR_INVALID_NAME;
    if (res == FR_OK)  // Cannot remove open object */
      res = checkFileLock (&directory, 2);
    if (res == FR_OK) {
      // The object is accessible
      dir = directory.dir;
      if (!dir)  // Cannot remove the origin directory */
        res = FR_INVALID_NAME;
      else if (dir[DIR_Attr] & AM_RDO)  // Cannot remove R/O object */
        res = FR_DENIED;

      if (res == FR_OK) {
        dclst = loadCluster (dir);
        if (dclst && (dir[DIR_Attr] & AM_DIR)) {
          if (dclst == cdir) // current directory
            res = FR_DENIED;
          else {
            // Open the sub-directory
            cDirectory subDirectory;
            memcpy (&subDirectory, &directory, sizeof (cDirectory));
            subDirectory.sclust = dclst;
            res = subDirectory.setIndex (2);
            if (res == FR_OK) {
              res = subDirectory.read (0);      /* Read an item (excluding dot entries) */
              if (res == FR_OK)
                res = FR_DENIED;  /* Not empty? (cannot remove) */
              if (res == FR_NO_FILE)
                res = FR_OK; /* Empty? (can remove) */
              }
            }
          }
        }

      if (res == FR_OK) {
        res = directory.remove();  /* Remove the directory entry */
        if (res == FR_OK && dclst)  /* Remove the cluster chain if exist */
          res = removeChain (dclst);
        if (res == FR_OK)
          res = syncFs();
        }
      }
    }

  unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFatFs::mkfs (const TCHAR* path, BYTE sfd, UINT au) {

  BYTE fmt, md, sys, *tbl;
  DWORD n_clst, vs, n, wsect;
  UINT i;
  DWORD b_vol, b_fat, b_dir, b_data;  /* LBA */
  DWORD n_vol, n_rsv, n_fat, n_dir; /* Size */

#if USE_TRIM
  DWORD eb[2];
#endif

  /* Check mounted drive and clear work area */
  if (sfd > 1)
    return FR_INVALID_PARAMETER;

  int vol = 0;
  fsType = 0;

  /* Get disk statics */
  DSTATUS stat = diskInitialize (vol);
  if (stat & STA_NOINIT)
    return FR_NOT_READY;
  if (stat & STA_PROTECT)
    return FR_WRITE_PROTECTED;

  /* Create a partition in this function */
  if (diskIoctl (vol, GET_SECTOR_COUNT, &n_vol) != RES_OK || n_vol < 128)
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
  if (diskIoctl (vol, GET_BLOCK_SIZE, &n) != RES_OK || !n || n > 32768)
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
    memset (windowBuffer, 0, SECTOR_SIZE);
    tbl = windowBuffer + MBR_Table;  /* Create partition table for single partition in the drive */
    tbl[1] = 1;                    /* Partition start head */
    tbl[2] = 1;                    /* Partition start sector */
    tbl[3] = 0;                    /* Partition start cylinder */
    tbl[4] = sys;                  /* System type */
    tbl[5] = 254;                  /* Partition end head */
    n = (b_vol + n_vol) / 63 / 255;
    tbl[6] = (BYTE)(n >> 2 | 63);  /* Partition end sector */
    tbl[7] = (BYTE)n;              /* End cylinder */
    ST_DWORD(tbl + 8, 63);         /* Partition start in LBA */
    ST_DWORD(tbl + 12, n_vol);     /* Partition size in LBA */
    ST_WORD(windowBuffer + BS_55AA, 0xAA55);  /* MBR signature */
    if (diskWrite (vol, windowBuffer, 0, 1) != RES_OK) /* Write it to the MBR */
      return FR_DISK_ERR;
    md = 0xF8;
    }

  /* Create BPB in the VBR */
  tbl = windowBuffer;             /* Clear sector */
  memset (tbl, 0, SECTOR_SIZE);
  memcpy (tbl, "\xEB\xFE\x90" "MSDOS5.0", 11); /* Boot jump code, OEM name */

  i = SECTOR_SIZE; /* Sector size */
  ST_WORD(tbl + BPB_BytsPerSec, i);

  tbl[BPB_SecPerClus] = (BYTE)au;        /* Sectors per cluster */
  ST_WORD(tbl + BPB_RsvdSecCnt, n_rsv);  /* Reserved sectors */
  tbl[BPB_NumFATs] = N_FATS;             /* Number of FATs */

  i = (fmt == FS_FAT32) ? 0 : N_ROOTDIR; /* Number of root directory entries */
  ST_WORD(tbl + BPB_RootEntCnt, i);
  if (n_vol < 0x10000) {                 /* Number of total sectors */
    ST_WORD(tbl + BPB_TotSec16, n_vol);
    }
  else {
    ST_DWORD(tbl + BPB_TotSec32, n_vol);
    }
  tbl[BPB_Media] = md;                   /* Media descriptor */

  ST_WORD(tbl + BPB_SecPerTrk, 63);      /* Number of sectors per track */
  ST_WORD(tbl + BPB_NumHeads, 255);      /* Number of heads */
  ST_DWORD(tbl + BPB_HiddSec, b_vol);    /* Hidden sectors */

  n = getFatTime();                     /* Use current time as VSN */
  if (fmt == FS_FAT32) {
    ST_DWORD(tbl + BS_VolID32, n);       /* VSN */
    ST_DWORD(tbl + BPB_FATSz32, n_fat);  /* Number of sectors per FAT */
    ST_DWORD(tbl + BPB_RootClus, 2);     /* Root directory start cluster (2) */
    ST_WORD(tbl + BPB_FSInfo, 1);        /* FSINFO record offset (VBR + 1) */
    ST_WORD(tbl + BPB_BkBootSec, 6);     /* Backup boot record offset (VBR + 6) */
    tbl[BS_DrvNum32] = 0x80;             /* Drive number */
    tbl[BS_BootSig32] = 0x29;            /* Extended boot signature */
    memcpy (tbl + BS_VolLab32, "NO NAME    " "FAT32   ", 19); /* Volume label, FAT signature */
    }
  else {
    ST_DWORD(tbl + BS_VolID, n);          /* VSN */
    ST_WORD(tbl + BPB_FATSz16, n_fat);    /* Number of sectors per FAT */
    tbl[BS_DrvNum] = 0x80;                /* Drive number */
    tbl[BS_BootSig] = 0x29;               /* Extended boot signature */
    memcpy (tbl + BS_VolLab, "NO NAME    " "FAT     ", 19); /* Volume label, FAT signature */
    }

  ST_WORD(tbl + BS_55AA, 0xAA55);         /* Signature (Offset is fixed here regardless of sector size) */
  if (diskWrite (vol, tbl, b_vol, 1) != RES_OK)  /* Write it to the VBR sector */
    return FR_DISK_ERR;
  if (fmt == FS_FAT32)          /* Write backup VBR if needed (VBR + 6) */
    diskWrite (vol, tbl, b_vol + 6, 1);

  /* Initialize FAT area */
  wsect = b_fat;
  for (i = 0; i < N_FATS; i++) {
    /* Initialize each FAT copy */
    memset (tbl, 0, SECTOR_SIZE);   /* 1st sector of the FAT  */
    n = md;                     /* Media descriptor byte */
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

    if (diskWrite (vol, tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;

    memset(tbl, 0, SECTOR_SIZE); /* Fill following FAT entries with zero */
    for (n = 1; n < n_fat; n++) {
      /* This loop may take a time on FAT32 volume due to many single sector writes */
      if (diskWrite (vol, tbl, wsect++, 1) != RES_OK)
        return FR_DISK_ERR;
      }
    }

  /* Initialize root directory */
  i = (fmt == FS_FAT32) ? au : (UINT)n_dir;
  do {
    if (diskWrite (vol, tbl, wsect++, 1) != RES_OK)
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
    ST_DWORD(tbl + FSI_LeadSig, 0x41615252);
    ST_DWORD(tbl + FSI_StrucSig, 0x61417272);
    ST_DWORD(tbl + FSI_Free_Count, n_clst - 1); /* Number of free clusters */
    ST_DWORD(tbl + FSI_Nxt_Free, 2);            /* Last allocated cluster# */
    ST_WORD(tbl + BS_55AA, 0xAA55);
    diskWrite (vol, tbl, b_vol + 1, 1);        /* Write original (VBR + 1) */
    diskWrite (vol, tbl, b_vol + 7, 1);        /* Write backup (VBR + 7) */
    }

  return (diskIoctl (vol, CTRL_SYNC, 0) == RES_OK) ? FR_OK : FR_DISK_ERR;
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

  if (fsType) {
    // check volume mounted
    DSTATUS status = diskStatus (cFatFs::drv);
    if (!(status & STA_NOINIT)) {
      // and the physical drive is kept initialized
      if (wmode && (status & STA_PROTECT)) // Check write protection if needed
        return FR_WRITE_PROTECTED;
      return FR_OK;
      }
    }

  // mount volume, analyze BPB and initialize the fs
  int vol = 0;
  fsType = 0;
  drv = vol;

  // init physical drive
  DSTATUS status = diskInitialize (drv);
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
      pt = windowBuffer + MBR_Table + i * SZ_PTE;
      br[i] = pt[4] ? LD_DWORD(&pt[8]) : 0;
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
  if (LD_WORD(windowBuffer + BPB_BytsPerSec) != SECTOR_SIZE) // (BPB_BytsPerSec must be equal to the physical sector size)
    return FR_NO_FILESYSTEM;

  // Number of sectors per FAT
  fasize = LD_WORD(windowBuffer + BPB_FATSz16);
  if (!fasize)
    fasize = LD_DWORD(windowBuffer + BPB_FATSz32);
  fsize = fasize;

  // Number of FAT copies
  numFatCopies = windowBuffer[BPB_NumFATs];
  if (numFatCopies != 1 && numFatCopies != 2) // (Must be 1 or 2)
    return FR_NO_FILESYSTEM;
  fasize *= numFatCopies; // Number of sectors for FAT area

  // Number of sectors per cluster
  csize = windowBuffer[BPB_SecPerClus];
  if (!csize || (csize & (csize - 1)))  // (Must be power of 2)
    return FR_NO_FILESYSTEM;

  // Number of root directory entries
  n_rootdir = LD_WORD(windowBuffer + BPB_RootEntCnt);
  if (n_rootdir % (SECTOR_SIZE / SZ_DIRE)) // (Must be sector aligned)
    return FR_NO_FILESYSTEM;

  // Number of sectors on the volume
  tsect = LD_WORD(windowBuffer + BPB_TotSec16);
  if (!tsect)
    tsect = LD_DWORD(windowBuffer + BPB_TotSec32);

  // Number of reserved sectors
  nrsv = LD_WORD(windowBuffer + BPB_RsvdSecCnt);
  if (!nrsv)
    return FR_NO_FILESYSTEM; /* (Must not be 0) */

  // Determine the FAT sub type
  sysect = nrsv + fasize + n_rootdir / (SECTOR_SIZE / SZ_DIRE); /* RSV + FAT + DIR */
  if (tsect < sysect)
    return FR_NO_FILESYSTEM; /* (Invalid volume size) */

  // Number of clusters
  nclst = (tsect - sysect) / csize;
  if (!nclst)
    return FR_NO_FILESYSTEM; /* (Invalid volume size) */
  fmt = FS_FAT12;
  if (nclst >= MIN_FAT16)
    fmt = FS_FAT16;
  if (nclst >= MIN_FAT32)
    fmt = FS_FAT32;

  // Boundaries and Limits
  numFatEntries = nclst + 2;      // Number of FAT entries
  volbase = bsect;           // Volume start sector
  fatbase = bsect + nrsv;    // FAT start sector
  database = bsect + sysect; // Data start sector
  if (fmt == FS_FAT32) {
    if (n_rootdir)
      return FR_NO_FILESYSTEM;       // (BPB_RootEntCnt must be 0)
    dirbase = LD_DWORD(windowBuffer + BPB_RootClus);  // Root directory start cluster
    szbfat = numFatEntries * 4;   // (Needed FAT size)
    }
  else {
    if (!n_rootdir)
      return FR_NO_FILESYSTEM;  // (BPB_RootEntCnt must not be 0)
    dirbase = fatbase + fasize;  // Root directory start sector
    szbfat = (fmt == FS_FAT16) ? numFatEntries * 2 : numFatEntries * 3 / 2 + (numFatEntries & 1);  /* (Needed FAT size) */
    }

  // (BPB_FATSz must not be less than the size needed)
  if (fsize < (szbfat + (SECTOR_SIZE - 1)) / SECTOR_SIZE)
    return FR_NO_FILESYSTEM;

  // Initialize cluster allocation information
  lastCluster = freeCluster = 0xFFFFFFFF;

  // Get fsinfo if available
  fsi_flag = 0x80;
  if (fmt == FS_FAT32 && LD_WORD(windowBuffer + BPB_FSInfo) == 1 && moveWindow (bsect + 1) == FR_OK) {
    fsi_flag = 0;
    if (LD_WORD(windowBuffer + BS_55AA) == 0xAA55 &&
        LD_DWORD(windowBuffer + FSI_LeadSig) == 0x41615252 &&
        LD_DWORD(windowBuffer + FSI_StrucSig) == 0x61417272) {
      freeCluster = LD_DWORD(windowBuffer + FSI_Free_Count);
      lastCluster = LD_DWORD(windowBuffer + FSI_Nxt_Free);
      }
    }

  fsType = fmt;  /* FAT sub-type */
  ++id;
  cdir = 0;       /* Set current directory to root */
  clearFileLock();

  return FR_OK;
  }
//}}}

//{{{
FRESULT cFatFs::syncWindow() {

  FRESULT res = FR_OK;

  if (wflag) {  /* Write back the sector if it is dirty */
    DWORD wsect = winsect;  /* Current sector number */
    if (diskWrite (drv, windowBuffer, wsect, 1) != RES_OK)
      res = FR_DISK_ERR;
    else {
      wflag = 0;
      if (wsect - fatbase < fsize) {    /* Is it in the FAT area? */
        for (UINT nf = numFatCopies; nf >= 2; nf--) {  /* Reflect the change to all FAT copies */
          wsect += fsize;
          diskWrite (drv, windowBuffer, wsect, 1);
          }
        }
      }
    }

  return res;
  }
//}}}
//{{{
FRESULT cFatFs::moveWindow (DWORD sector) {

  FRESULT res = FR_OK;
  if (sector != winsect) {
    /* Window offset changed?  Write-back changes */
    res = syncWindow();
    if (res == FR_OK) {
      /* Fill sector window with new data */
      if (diskRead (drv, windowBuffer, sector, 1) != RES_OK) {
        /* Invalidate window if data is not reliable */
        sector = 0xFFFFFFFF;
        res = FR_DISK_ERR;
        }
      winsect = sector;
      }
    }

  return res;
  }
//}}}

//{{{
BYTE cFatFs::checkFs (DWORD sector) {

  // Invalidate window
  wflag = 0;
  winsect = 0xFFFFFFFF;

  // Load boot record
  if (moveWindow (sector) != FR_OK)
    return 3;

  // Check boot record signature (always placed at offset 510 even if the sector size is >512)
  if (LD_WORD(&windowBuffer[BS_55AA]) != 0xAA55)
    return 2;

  // Check "FAT" string
  if ((LD_DWORD(&windowBuffer[BS_FilSysType]) & 0xFFFFFF) == 0x544146)
    return 0;

  // Check "FAT" string
  if ((LD_DWORD(&windowBuffer[BS_FilSysType32]) & 0xFFFFFF) == 0x544146)
    return 0;

  return 1;
  }
//}}}
//{{{
FRESULT cFatFs::syncFs() {

  FRESULT res = syncWindow();
  if (res == FR_OK) {
    /* Update FSINFO sector if needed */
    if (fsType == FS_FAT32 && fsi_flag == 1) {
      /* Create FSINFO structure */
      memset (windowBuffer, 0, SECTOR_SIZE);
      ST_WORD(windowBuffer + BS_55AA, 0xAA55);
      ST_DWORD(windowBuffer + FSI_LeadSig, 0x41615252);
      ST_DWORD(windowBuffer + FSI_StrucSig, 0x61417272);
      ST_DWORD(windowBuffer + FSI_Free_Count, freeCluster);
      ST_DWORD(windowBuffer + FSI_Nxt_Free, lastCluster);
      /* Write it into the FSINFO sector */
      winsect = volbase + 1;
      diskWrite (drv, windowBuffer, winsect, 1);
      fsi_flag = 0;
      }

    /* Make sure that no pending write process in the physical drive */
    if (diskIoctl (drv, CTRL_SYNC, 0) != RES_OK)
      res = FR_DISK_ERR;
    }

  return res;
  }
//}}}

//{{{
DWORD cFatFs::getFat (DWORD cluster) {

  UINT wc, bc;
  BYTE *p;
  DWORD val;

  if (cluster < 2 || cluster >= numFatEntries)  /* Check range */
    val = 1;  /* Internal error */
  else {
    val = 0xFFFFFFFF; /* Default value falls on disk error */

    switch (fsType) {
      case FS_FAT12 :
        bc = (UINT)cluster; bc += bc / 2;
        if (moveWindow (fatbase + (bc / SECTOR_SIZE)) != FR_OK)
          break;

        wc = windowBuffer[bc++ % SECTOR_SIZE];
        if (moveWindow (fatbase + (bc / SECTOR_SIZE)) != FR_OK)
          break;

        wc |= windowBuffer[bc % SECTOR_SIZE] << 8;
        val = cluster & 1 ? wc >> 4 : (wc & 0xFFF);
        break;

      case FS_FAT16 :
        if (moveWindow (fatbase + (cluster / (SECTOR_SIZE / 2))) != FR_OK)
          break;
        p = &windowBuffer[cluster * 2 % SECTOR_SIZE];
        val = LD_WORD(p);
        break;

      case FS_FAT32 :
        if (moveWindow (fatbase + (cluster / (SECTOR_SIZE / 4))) != FR_OK)
          break;
        p = &windowBuffer[cluster * 4 % SECTOR_SIZE];
        val = LD_DWORD(p) & 0x0FFFFFFF;
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
  FRESULT res;

  if (cluster < 2 || cluster >= numFatEntries)  /* Check range */
    res = FR_INT_ERR;
  else {
    switch (fsType) {
      case FS_FAT12 :
        bc = (UINT)cluster; bc += bc / 2;
        res = moveWindow (fatbase + (bc / SECTOR_SIZE));
        if (res != FR_OK)
          break;
        p = &windowBuffer[bc++ % SECTOR_SIZE];
        *p = (cluster & 1) ? ((*p & 0x0F) | ((BYTE)val << 4)) : (BYTE)val;
        wflag = 1;
        res = moveWindow (fatbase + (bc / SECTOR_SIZE));
        if (res != FR_OK)
          break;
        p = &windowBuffer[bc % SECTOR_SIZE];
        *p = (cluster & 1) ? (BYTE)(val >> 4) : ((*p & 0xF0) | ((BYTE)(val >> 8) & 0x0F));
        wflag = 1;
        break;

      case FS_FAT16 :
        res = moveWindow (fatbase + (cluster / (SECTOR_SIZE / 2)));
        if (res != FR_OK)
          break;
        p = &windowBuffer[cluster * 2 % SECTOR_SIZE];
        ST_WORD(p, (WORD)val);
        wflag = 1;
        break;

      case FS_FAT32 :
        res = moveWindow (fatbase + (cluster / (SECTOR_SIZE / 4)));
        if (res != FR_OK)
          break;
        p = &windowBuffer[cluster * 4 % SECTOR_SIZE];
        val |= LD_DWORD(p) & 0xF0000000;
        ST_DWORD(p, val);
        wflag = 1;
        break;

      default :
        res = FR_INT_ERR;
      }
    }

  return res;
  }
//}}}

//{{{
DWORD cFatFs::clusterToSector (DWORD cluster) {

  cluster -= 2;
  return (cluster >= numFatEntries - 2) ? 0 : cluster * csize + database;
  }
//}}}
//{{{
DWORD cFatFs::loadCluster (BYTE* dir) {

  DWORD cluster = LD_WORD(dir + DIR_FstClusLO);
  if (fsType == FS_FAT32)
    cluster |= (DWORD)LD_WORD(dir + DIR_FstClusHI) << 16;

  return cluster;
  }
//}}}

//{{{
DWORD cFatFs::createChain (DWORD cluster) {

  DWORD scl;
  if (cluster == 0) {    /* Create a new chain */
    scl = lastCluster;     /* Get suggested start point */
    if (!scl || scl >= numFatEntries) scl = 1;
    }
  else {          /* Stretch the current chain */
    DWORD cs = getFat (cluster);     /* Check the cluster status */
    if (cs < 2)
      return 1;     /* Invalid value */
    if (cs == 0xFFFFFFFF)
      return cs;  /* A disk error occurred */
    if (cs < numFatEntries)
      return cs; /* It is already followed by next cluster */
    scl = cluster;
    }

 DWORD ncl = scl;        /* Start cluster */
  for (;;) {
    ncl++;              /* Next cluster */
    if (ncl >= numFatEntries) {    /* Check wrap around */
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

  FRESULT res = putFat (ncl, 0x0FFFFFFF); /* Mark the new cluster "last link" */
  if (res == FR_OK && cluster != 0)
    res = putFat (cluster, ncl); /* Link it to the previous one if needed */

  if (res == FR_OK) {
    lastCluster = ncl;  /* Update FSINFO */
    if (freeCluster != 0xFFFFFFFF) {
      freeCluster--;
      fsi_flag |= 1;
      }
    }
  else
    ncl = (res == FR_DISK_ERR) ? 0xFFFFFFFF : 1;

  return ncl;   /* Return new cluster number or error code */
  }
//}}}
//{{{
FRESULT cFatFs::removeChain (DWORD cluster) {

#if USE_TRIM
  DWORD scl = cluster, ecl = cluster, rt[2];
#endif

  FRESULT res;
  if (cluster < 2 || cluster >= numFatEntries)
    /* Check range */
    res = FR_INT_ERR;
  else {
    res = FR_OK;
    while (cluster < numFatEntries) {     /* Not a last link? */
      DWORD nxt = getFat (cluster); /* Get cluster status */
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

      res = putFat (cluster, 0);     /* Mark the cluster "empty" */
      if (res != FR_OK) break;
      if (freeCluster != 0xFFFFFFFF) { /* Update FSINFO */
        freeCluster++;
        fsi_flag |= 1;
        }

#if USE_TRIM
      if (ecl + 1 == nxt)
        /* Is next cluster contiguous? */
        ecl = nxt;
      else {        /* End of contiguous clusters */
        rt[0] = clust2sect(fs, scl);          /* Start sector */
        rt[1] = clust2sect(fs, ecl) + csize - 1;  /* End sector */
        diskIoctl (drv, CTRL_TRIM, rt);       /* Erase the block */
        scl = ecl = nxt;
        }
#endif

      cluster = nxt; /* Next cluster */
      }
    }

  return res;
  }
//}}}

//{{{
bool cFatFs::lock() {

  return osSemaphoreWait (semaphore, 1000) == osOK;
  }
//}}}
//{{{
void cFatFs::unlock (FRESULT res) {

  if (res != FR_NOT_ENABLED && res != FR_INVALID_DRIVE && res != FR_INVALID_OBJECT && res != FR_TIMEOUT)
    osSemaphoreRelease (semaphore);
  }
//}}}

//{{{
int cFatFs::enquireFileLock() {

  UINT i;
  for (i = 0; i < FS_LOCK && mFiles[i].fs; i++) ;
  return (i == FS_LOCK) ? 0 : 1;
  }
//}}}
//{{{
FRESULT cFatFs::checkFileLock (cDirectory* dp, int acc) {

  UINT i, be;

  // Search file semaphore table
  for (i = be = 0; i < FS_LOCK; i++) {
    if (mFiles[i].fs) {
      // Existing entry
      if (mFiles[i].fs == dp->fs && mFiles[i].clu == dp->sclust && mFiles[i].idx == dp->index)
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
UINT cFatFs::incFileLock (cDirectory* dp, int acc) {

  UINT i;
  for (i = 0; i < FS_LOCK; i++)
    // Find the object
    if (mFiles[i].fs == dp->fs && mFiles[i].clu == dp->sclust && mFiles[i].idx == dp->index)
      break;

  if (i == FS_LOCK) {
    /* Not opened. Register it as new. */
    for (i = 0; i < FS_LOCK && mFiles[i].fs; i++) ;
    if (i == FS_LOCK)
      return 0;  /* No free entry to register (int err) */

    mFiles[i].fs = dp->fs;
    mFiles[i].clu = dp->sclust;
    mFiles[i].idx = dp->index;
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
      mFiles[i].fs = 0;  /* Delete the entry if open count gets zero */

    return FR_OK;
    }
  else
    return FR_INT_ERR;     /* Invalid index nunber */
  }
//}}}
//{{{
void cFatFs::clearFileLock() {

  for (UINT i = 0; i < FS_LOCK; i++)
    if (mFiles[i].fs == this)
      mFiles[i].fs = 0;
  }
//}}}
//}}}

// cDirectory
//{{{
FRESULT cDirectory::open (const TCHAR* path) {

  FRESULT res = cFatFs::instance()->findVolume (&fs, 0);
  if (res == FR_OK) {
    BYTE sfn1[12];
    WCHAR lfn1[(MAX_LFN + 1) * 2];
    lfn = lfn1;
    fn = sfn1;
    res = followPath (path);
    if (res == FR_OK) {
      if (dir) {
        // not itself */
        if (dir[DIR_Attr] & AM_DIR) // subDir
          sclust = fs->loadCluster (dir);
        else // file
          res = FR_NO_PATH;
        }

      if (res == FR_OK) {
        id = fs->id;
        // Rewind directory
        res = setIndex (0);
        if (res == FR_OK) {
          if (sclust) {
            // lock subDir
            lockid = fs->incFileLock (this, 0);
            if (!lockid)
              res = FR_TOO_MANY_OPEN_FILES;
            }
          else // root directory not locked
            lockid = 0;
          }
        }
      }

    if (res == FR_NO_FILE)
      res = FR_NO_PATH;
    }

  if (res != FR_OK)
    fs = 0;

  fs->unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cDirectory::read (cFileInfo* fileInfo) {

  FRESULT res = validateDir();
  if (res == FR_OK) {
    if (!fileInfo)
      // Rewind the directory object
      res = setIndex (0);
    else {
      BYTE sfn1[12];
      WCHAR lfn1 [(MAX_LFN + 1) * 2];
      lfn = lfn1;
      fn = sfn1;
      res = read (0);
      if (res == FR_NO_FILE) {
        // Reached end of directory
        sect = 0;
        res = FR_OK;
        }

      if (res == FR_OK) {
        // valid entry is found, get the object information */
        getFileInfo (fileInfo);

        // Increment index for next */
        res = next (0);
        if (res == FR_NO_FILE) {
          sect = 0;
          res = FR_OK;
          }
        }
      }
    }

  fs->unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cDirectory::findfirst (cFileInfo* fileInfo, const TCHAR* path, const TCHAR* pattern) {

  pat = pattern;
  FRESULT res = open (path);
  if (res == FR_OK)
    res = findnext (fileInfo);
  return res;
  }
//}}}
//{{{
FRESULT cDirectory::findnext (cFileInfo* fileInfo) {

  FRESULT res;
  for (;;) {
    res = read (fileInfo);
    if (res != FR_OK || !fileInfo || !fileInfo->fname[0])
      break;

    // match lfn
    if (fileInfo->lfname && matchPattern (pat, fileInfo->lfname, 0, 0))
      break;

    // match sfn
    if (matchPattern (pat, fileInfo->fname, 0, 0))
      break;
    }

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::close() {

  FRESULT res = validateDir();
  if (res == FR_OK) {
    cFatFs* fatFs = fs;

    if (lockid)
      // Decrement sub-directory open counter
      res = fs->decFileLock (lockid);

    if (res == FR_OK)
      // Invalidate directory object
      fs = 0;

    fatFs->unlock (FR_OK);
    }

  return res;
  }
//}}}
//{{{  cDirectory private members
//{{{
FRESULT cDirectory::validateDir() {

  if (!fs ||
      !fs->fsType ||
      fs->id != id ||
      (diskStatus (fs->drv) & STA_NOINIT))
    return FR_INVALID_OBJECT;

  // lock access to file system
  if (!fs->lock())
    return FR_TIMEOUT;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::followPath (const TCHAR* path) {

  FRESULT res;
  BYTE *dir1, ns;

  if (*path == '/' || *path == '\\') {
    //  There is a heading separator
    path++;
    sclust = 0;       /* Strip it and start from the root directory */
    }
  else
    // No heading separator
    sclust = fs->cdir;      /* Start from the current directory */

  if ((UINT)*path < ' ') {
    // Null path name is the origin directory itself
    res = setIndex (0);
    dir = 0;
    }

  else {
    for (;;) {
      res = createName (&path); /* Get a segment name of the path */
      if (res != FR_OK)
        break;
      res = find();       /* Find an object with the sagment name */
      ns = fn[NSFLAG];
      if (res != FR_OK) {
        /* Failed to find the object */
        if (res == FR_NO_FILE) {
          /* Object is not found */
          if (ns & NS_DOT) {
            /* If dot entry is not exist, */
            sclust = 0;
            dir = 0;  /* it is the root directory and stay there */
            if (!(ns & NS_LAST))
              continue;  /* Continue to follow if not last segment */
            res = FR_OK;  /* Ended at the root directroy. Function completed. */
            }
          else if (!(ns & NS_LAST))
            res = FR_NO_PATH;  /* Adirectoryust error code if not last segment */
          }
        break;
        }

      if (ns & NS_LAST)
        break;      /* Last segment matched. Function completed. */

      dir1 = dir;
      if (!(dir1[DIR_Attr] & AM_DIR)) {
        /* It is not a sub-directory and cannot follow */
        res = FR_NO_PATH;
        break;
        }

      sclust = fs->loadCluster(dir1);
      }
    }

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::createName (const TCHAR** path) {

  BYTE b, cf;
  UINT i, ni;

  // Create LFN in Unicode
  const TCHAR* p;
  for (p = *path; *p == '/' || *p == '\\'; p++) ; /* Strip duplicated separator */
  WCHAR* lfn1 = lfn;
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
    lfn1[di++] = w;   // Store the Unicode character */
    }

  *path = &p[si];  // Return pointer to the next segment */
  cf = (w < ' ') ? NS_LAST : 0;   // Set last segment flag if end of path */
  if ((di == 1 && lfn1[di - 1] == '.') || (di == 2 && lfn1[di - 1] == '.' && lfn1[di - 2] == '.')) {
    lfn1[di] = 0;
    for (i = 0; i < 11; i++)
      fn[i] = (i < di) ? '.' : ' ';
    fn[i] = cf | NS_DOT;   // This is a dot entry */
    return FR_OK;
    }

  while (di) {
    // Strip trailing spaces and dots */
    w = lfn1[di - 1];
    if (w != ' ' && w != '.')
      break;
    di--;
    }
  if (!di)
    return FR_INVALID_NAME;  /* Reject nul string */

  // LFN is created
  lfn1[di] = 0;

  // Create SFN in directory form
  memset(fn, ' ', 11);
  for (si = 0; lfn1[si] == ' ' || lfn1[si] == '.'; si++) ;  /* Strip leading spaces and dots */
  if (si)
    cf |= NS_LOSS | NS_LFN;
  while (di && lfn1[di - 1] != '.')
    di--;  /* Find extension (di<=si: no extension) */

  b = i = 0; ni = 8;
  for (;;) {
    w = lfn1[si++];  // Get an LFN character */
    if (!w) // Break on end of the LFN
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
      if (IsUpper(w))
        // ASCII large capital
        b |= 2;
      else if (IsLower(w)) {
        // ASCII small capital
        b |= 1;
        w -= 0x20;
        }
      }
    fn[i++] = (BYTE)w;
    }

  if (fn[0] == DDEM)
    fn[0] = RDDEM; /* If the first character collides with deleted mark, replace it with RDDEM */

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
  fn[NSFLAG] = cf;

  return FR_OK;
  }
//}}}

//{{{
FRESULT cDirectory::setIndex (UINT idx) {

  // Current index
  index = (WORD)idx;

  // Table start cluster (0:root)
  DWORD cluster = sclust;
  if (cluster == 1 || cluster >= fs->numFatEntries)  /* Check start cluster range */
    return FR_INT_ERR;
  if (!cluster && fs->fsType == FS_FAT32) /* Replace cluster# 0 with root cluster# if in FAT32 */
    cluster = fs->dirbase;

  DWORD sect1;
  if (cluster == 0) {
    // static table (root-directory in FAT12/16)
    if (idx >= fs->n_rootdir) // index out of range
      return FR_INT_ERR;
    sect1 = fs->dirbase;
    }
  else {
    // dynamic table (root-directory in FAT32 or sub-directory)
    UINT ic = SECTOR_SIZE / SZ_DIRE * fs->csize;  // entries per cluster
    while (idx >= ic) {
      // follow cluster chain,  get next cluster
      cluster = fs->getFat (cluster);
      if (cluster == 0xFFFFFFFF)
        return FR_DISK_ERR;
      if (cluster < 2 || cluster >= fs->numFatEntries) // reached end of table or internal error
        return FR_INT_ERR;
      idx -= ic;
      }
    sect1 = fs->clusterToSector (cluster);
    }

  clust = cluster;
  if (!sect1)
    return FR_INT_ERR;

  // Sector# of the directory entry
  sect = sect1 + idx / (SECTOR_SIZE / SZ_DIRE);

  // Ptr to the entry in the sector
  dir = fs->windowBuffer + (idx % (SECTOR_SIZE / SZ_DIRE)) * SZ_DIRE;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::next (int stretch) {

  UINT i = index + 1;
  if (!(i & 0xFFFF) || !sect) /* Report EOT when index has reached 65535 */
    return FR_NO_FILE;

  if (!(i % (SECTOR_SIZE / SZ_DIRE))) {
    sect++;
    if (!clust) {
      /* Static table */
      if (i >= fs->n_rootdir) /* Report EOT if it reached end of static table */
        return FR_NO_FILE;
      }
    else {
      /* Dynamic table */
      if (((i / (SECTOR_SIZE / SZ_DIRE)) & (fs->csize - 1)) == 0) {
        /* Cluster changed? */
        DWORD cluster = fs->getFat (clust);        /* Get next cluster */
        if (cluster <= 1)
          return FR_INT_ERR;
        if (cluster == 0xFFFFFFFF)
          return FR_DISK_ERR;
        if (cluster >= fs->numFatEntries) {
          /* If it reached end of dynamic table, */
          if (!stretch)
            return FR_NO_FILE;      /* If do not stretch, report EOT */
          cluster = fs->createChain (clust);   /* Stretch cluster chain */
          if (cluster == 0)
            return FR_DENIED;      /* No free cluster */
          if (cluster == 1)
            return FR_INT_ERR;
          if (cluster == 0xFFFFFFFF)
            return FR_DISK_ERR;

          /* Clean-up stretched table */
          if (fs->syncWindow())
            return FR_DISK_ERR;/* Flush disk access window */

          /* Clear window buffer */
          memset (fs->windowBuffer, 0, SECTOR_SIZE);

          /* Cluster start sector */
          fs->winsect = fs->clusterToSector (cluster);

          UINT c;
          for (c = 0; c < fs->csize; c++) {
            /* Fill the new cluster with 0 */
            fs->wflag = 1;
            if (fs->syncWindow())
              return FR_DISK_ERR;
            fs->winsect++;
            }
          fs->winsect -= c;           /* Rewind window offset */
          }

        /* Initialize data for new cluster */
        clust = cluster;
        sect = fs->clusterToSector (cluster);
        }
      }
    }

  /* Current index */
  index = (WORD)i;

  /* Current entry in the window */
  dir = fs->windowBuffer + (i % (SECTOR_SIZE / SZ_DIRE)) * SZ_DIRE;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::registerNewEntry() {

  UINT n, nent;

  BYTE* fn1 = fn;
  WCHAR* lfn1 = lfn;

  BYTE sn[12];
  memcpy (sn, fn1, 12);

  if (sn[NSFLAG] & NS_DOT)   /* Cannot create dot entry */
    return FR_INVALID_NAME;

  if (sn[NSFLAG] & NS_LOSS) {     /* When LFN is out of 8.3 format, generate a numbered name */
  FRESULT res;
  fn1[NSFLAG] = 0;
    lfn = 0;      /* Find only SFN */
    for (n = 1; n < 100; n++) {
      generateNumName (fn1, sn, lfn1, n);  /* Generate a numbered name */
      res = find();       /* Check if the name collides with existing SFN */
      if (res != FR_OK)
        break;
      }
    if (n == 100)
      return FR_DENIED;   /* Abort if too many collisions */
    if (res != FR_NO_FILE)
      return res;  /* Abort if the result is other than 'not collided' */

    fn1[NSFLAG] = sn[NSFLAG];
    lfn = lfn1;
    }

  if (sn[NSFLAG] & NS_LFN) {      /* When LFN is to be created, allocate entries for an SFN + LFNs. */
    for (n = 0; lfn1[n]; n++) ;
    nent = (n + 25) / 13;
    }
  else
    /* Otherwise allocate an entry for an SFN  */
    nent = 1;

  FRESULT res = allocate (nent);    /* Allocate entries */
  if (res == FR_OK && --nent) {
    /* Set LFN entry if needed */
    res = setIndex (index - nent);
    if (res == FR_OK) {
      BYTE sum = sumSfn (fn);  /* Sum value of the SFN tied to the LFN */
      do {
        /* Store LFN entries in bottom first */
        res = fs->moveWindow (sect);
        if (res != FR_OK)
          break;
        fitLfn (lfn, dir, (BYTE)nent, sum);
        fs->wflag = 1;
        res = next (0);  /* Next entry */
        } while (res == FR_OK && --nent);
      }
    }

  if (res == FR_OK) {
    /* Set SFN entry */
    res = fs->moveWindow (sect);
    if (res == FR_OK) {
      memset (dir, 0, SZ_DIRE); /* Clean the entry */
      memcpy (dir, fn, 11); /* Put SFN */
      dir[DIR_NTres] = fn[NSFLAG] & (NS_BODY | NS_EXT); /* Put NT flag */
      fs->wflag = 1;
      }
    }

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::allocate (UINT nent) {

  UINT n;
  FRESULT res = setIndex (0);
  if (res == FR_OK) {
    n = 0;
    do {
      res = fs->moveWindow (sect);
      if (res != FR_OK)
        break;
      if (dir[0] == DDEM || dir[0] == 0) {
        // free entry
        if (++n == nent)
          break; /* A block of contiguous free entries is found */
        }
      else
        n = 0;          /* Not a blank entry. Restart to search */
      res = next (1);    /* Next entry with table stretch enabled */
      } while (res == FR_OK);
    }

  if (res == FR_NO_FILE)
    res = FR_DENIED; /* No directory entry to allocate */

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::find() {

  BYTE c, *dir1;
  BYTE a, sum;

  FRESULT res = setIndex (0);     /* Rewind directory object */
  if (res != FR_OK)
    return res;

  BYTE ord = sum = 0xFF;
  lfn_idx = 0xFFFF; /* Reset LFN sequence */
  do {
    res = fs->moveWindow (sect);
    if (res != FR_OK)
      break;

    dir1 = dir;          /* Ptr to the directory entry of current index */
    c = dir1[DIR_Name];
    if (c == 0) {
      res = FR_NO_FILE;
      break;
      }  /* Reached to end of table */

    a = dir1[DIR_Attr] & AM_MASK;
    if (c == DDEM || ((a & AM_VOL) && a != AM_LFN)) { /* An entry without valid data */
      ord = 0xFF;
      lfn_idx = 0xFFFF; /* Reset LFN sequence */
      }
    else {
      if (a == AM_LFN) {      /* An LFN entry is found */
        if (lfn) {
          if (c & LLEF) {   /* Is it start of LFN sequence? */
            sum = dir1[LDIR_Chksum];
            c &= ~LLEF;
            ord = c;  /* LFN start order */
            lfn_idx = index;  /* Start index of LFN */
            }
          /* Check validity of the LFN entry and compare it with given name */
          ord = (c == ord && sum == dir1[LDIR_Chksum] && compareLfn (lfn, dir1)) ? ord - 1 : 0xFF;
          }
        }
      else {          /* An SFN entry is found */
        if (!ord && sum == sumSfn (dir1))
          break; /* LFN matched? */
        if (!(fn[NSFLAG] & NS_LOSS) && !memcmp (dir1, fn, 11))
          break;  /* SFN matched? */
        ord = 0xFF;
        lfn_idx = 0xFFFF; /* Reset LFN sequence */
        }
      }

    res = next (0);    /* Next entry */
    } while (res == FR_OK);

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::read (int vol) {

  BYTE a, c, *dir1;
  BYTE ord = 0xFF;
  BYTE sum = 0xFF;

  FRESULT res = FR_NO_FILE;
  while (sect) {
    res = fs->moveWindow (sect);
    if (res != FR_OK)
      break;
    dir1 = dir;          /* Ptr to the directory entry of current index */
    c = dir1[DIR_Name];
    if (c == 0) {
      res = FR_NO_FILE;
      break;
      }  /* Reached to end of table */

    a = dir1[DIR_Attr] & AM_MASK;
    if (c == DDEM || (int)((a & ~AM_ARC) == AM_VOL) != vol)
      // entry without valid data
      ord = 0xFF;
    else {
      if (a == AM_LFN) {      /* An LFN entry is found */
        if (c & LLEF) {     /* Is it start of LFN sequence? */
          sum = dir1[LDIR_Chksum];
          c &= ~LLEF; ord = c;
          lfn_idx = index;
          }
        /* Check LFN validity and capture it */
        ord = (c == ord && sum == dir1[LDIR_Chksum] && pickLfn (lfn, dir1)) ? ord - 1 : 0xFF;
        }
      else {          /* An SFN entry is found */
        if (ord || sum != sumSfn (dir1)) /* Is there a valid LFN? */
          lfn_idx = 0xFFFF;   /* It has no LFN. */
        break;
        }
      }

    res = next (0);        /* Next entry */
    if (res != FR_OK)
      break;
    }

  if (res != FR_OK)
    sect = 0;

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::remove() {

  // SFN index
  UINT i = index;

  FRESULT res = setIndex ((lfn_idx == 0xFFFF) ? i : lfn_idx); /* Goto the SFN or top of the LFN entries */
  if (res == FR_OK) {
    do {
      res = fs->moveWindow (sect);
      if (res != FR_OK)
        break;
      memset (dir, 0, SZ_DIRE); /* Clear and mark the entry "deleted" */

      *dir = DDEM;
      fs->wflag = 1;
      if (index >= i)
        break;  /* When reached SFN, all entries of the object has been deleted. */

      res = next (0);    /* Next entry */
      } while (res == FR_OK);

    if (res == FR_NO_FILE)
      res = FR_INT_ERR;
    }

  return res;
  }
//}}}

//{{{
void cDirectory::getFileInfo (cFileInfo* fileInfo) {

  UINT i;
  TCHAR *p, c;
  WCHAR w;

  p = fileInfo->fname;
  if (sect) {
    // Get sfn
    BYTE* dir1 = dir;
    i = 0;
    while (i < 11) {
      // Copy name body and extension
      c = (TCHAR)dir1[i++];
      if (c == ' ')
        continue;
      if (c == RDDEM)
        c = (TCHAR)DDEM;  // Restore replaced DDEM character
      if (i == 9)
        *p++ = '.';       // Insert a . if extension is exist
      if (IsUpper(c) && (dir1[DIR_NTres] & (i >= 9 ? NS_EXT : NS_BODY)))
        c += 0x20;  // To lower
      *p++ = c;
      }

    fileInfo->fattrib = dir1[DIR_Attr];              // Attribute
    fileInfo->fsize = LD_DWORD(dir1 + DIR_FileSize); // Size
    fileInfo->fdate = LD_WORD(dir1 + DIR_WrtDate);   // Date
    fileInfo->ftime = LD_WORD(dir1 + DIR_WrtTime);   // Time
    }

  // terminate sfn string
  *p = 0;

  if (fileInfo->lfname) {
    i = 0;
    p = fileInfo->lfname;
    if (sect && fileInfo->lfsize && lfn_idx != 0xFFFF) {
      // Get lfn if available
      WCHAR* lfnPtr = lfn;
      while ((w = *lfnPtr++) != 0) {
        // Get an lfn character
        w = convertToFromUnicode (w, 0);  // Unicode -> OEM
        if (!w) {
          // No lfn if it could not be converted
          i = 0;
          break;
          }
        if (i >= fileInfo->lfsize - 1) {
          // No lfn if buffer overflow
          i = 0;
          break;
          }
        p[i++] = (TCHAR)w;
        }
      }

    // terminate lfn string
    p[i] = 0;
    }
  }
//}}}
//}}}

// cFile
//{{{
cFile::cFile() {
  fileBuffer = (BYTE*)pvPortMalloc (SECTOR_SIZE);
  }
//}}}
//{{{
cFile::~cFile() {
  vPortFree (fileBuffer);
  }
//}}}
//{{{
FRESULT cFile::open (const TCHAR* path, BYTE mode) {

  DWORD dw, cl;

  fs = 0;
  cDirectory directory;
  mode &= FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW;
  FRESULT res = cFatFs::instance()->findVolume (&directory.fs, (BYTE)(mode & ~FA_READ));
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(MAX_LFN + 1) * 2];
    directory.lfn = lfn;
    directory.fn = sfn;
    res = directory.followPath (path);
    BYTE* dir = directory.dir;
    if (res == FR_OK) {
      if (!dir)
        /* Default directory itself */
        res = FR_INVALID_NAME;
      else
        res = directory.fs->checkFileLock (&directory, (mode & ~FA_READ) ? 1 : 0);
      }

    /* Create or Open a file */
    if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
      if (res != FR_OK) {
        /* No file, create new */
        if (res == FR_NO_FILE)
          /* There is no file to open, create a new entry */
          res = fs->enquireFileLock() ? directory.registerNewEntry() : FR_TOO_MANY_OPEN_FILES;
        mode |= FA_CREATE_ALWAYS;   /* File is created */
        dir = directory.dir;         /* New entry */
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
        dw = getFatTime();       /* Created time */
        ST_DWORD(dir + DIR_CrtTime, dw);
        dir[DIR_Attr] = 0;        /* Reset attribute */
        ST_DWORD(dir + DIR_FileSize, 0);/* size = 0 */
        cl = directory.fs->loadCluster (dir);    /* Get start cluster */
        storeCluster (dir, 0);       /* cluster = 0 */
        directory.fs->wflag = 1;
        if (cl) {
          /* Remove the cluster chain if exist */
          dw = directory.fs->winsect;
          res = directory.fs->removeChain (cl);
          if (res == FR_OK) {
            directory.fs->lastCluster = cl - 1;
            /* Reuse the cluster hole */
            res = directory.fs->moveWindow (dw);
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
      dir_sect = directory.fs->winsect;  /* Pointer to the directory entry */
      dir_ptr = dir;
      lockid = directory.fs->incFileLock (&directory, (mode & ~FA_READ) ? 1 : 0);
      if (!lockid)
        res = FR_INT_ERR;
      }

    if (res == FR_OK) {
      flag = mode;                          /* File access mode */
      err = 0;                              /* Clear error flag */
      sclust = directory.fs->loadCluster (dir);    /* File start cluster */
      fsize = LD_DWORD(dir + DIR_FileSize); /* File size */
      fptr = 0;                             /* File pointer */
      dsect = 0;
      cltbl = 0;                            /* Normal seek mode */
      fs = directory.fs;                           /* Validate file object */
      id = fs->id;
      }
    }

  directory.fs->unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFile::lseek (DWORD ofs) {

  DWORD clst, bcs, nsect, ifptr;
  DWORD cl, pcl, ncl, tcl, dsc, tlen, ulen, *tbl;

  FRESULT res = validateFile();
  if (res != FR_OK)
    LEAVE_FF(fs, res);
  if (err)                 /* Check error */
    LEAVE_FF(fs, (FRESULT)err);

  if (cltbl) {
    /* Fast seek */
    if (ofs == CREATE_LINKMAP) {
      /* Create CLMT */
      tbl = cltbl;
      tlen = *tbl++; ulen = 2;  /* Given table size and required table size */
      cl = sclust;      /* Top of the chain */
      if (cl) {
        do {
          /* Get a fragment */
          tcl = cl;
          ncl = 0;
          ulen += 2; /* Top, length and used items */
          do {
            pcl = cl; ncl++;
            cl = fs->getFat (cl);
            if (cl <= 1)
              ABORT(fs, FR_INT_ERR);
            if (cl == 0xFFFFFFFF)
              ABORT(fs, FR_DISK_ERR);
            } while (cl == pcl + 1);
          if (ulen <= tlen) {
            /* Store the length and top of the fragment */
            *tbl++ = ncl;
            *tbl++ = tcl;
            }
          } while (cl < fs->numFatEntries);  /* Repeat until end of chain */
        }

      /* Number of items used */
      *cltbl = ulen;
      if (ulen <= tlen)
        /* Terminate table */
        *tbl = 0;
      else
        /* Given table size is smaller than required */
        res = FR_NOT_ENOUGH_CORE;
      }

    else {
      /* Fast seek */
      if (ofs > fsize)    /* Clip offset at the file size */
        ofs = fsize;
      fptr = ofs;       /* Set file pointer */
      if (ofs) {
        clust = clmtCluster (ofs - 1);
        dsc = fs->clusterToSector (clust);
        if (!dsc)
          ABORT(fs, FR_INT_ERR);
        dsc += (ofs - 1) / SECTOR_SIZE & (fs->csize - 1);
        if (fptr % SECTOR_SIZE && dsc != dsect) {
          /* Refill sector cache if needed */
          if (flag & FA__DIRTY) {
            /* Write-back dirty sector cache */
            if (diskWrite (fs->drv, fileBuffer, dsect, 1) != RES_OK)
              ABORT (fs, FR_DISK_ERR);
            flag &= ~FA__DIRTY;
            }
          if (diskRead (fs->drv, fileBuffer, dsc, 1) != RES_OK) /* Load current sector */
            ABORT(fs, FR_DISK_ERR);
          dsect = dsc;
          }
        }
      }
    }

  else {
    /* Normal Seek */
    if (ofs > fsize && !(flag & FA_WRITE))
      ofs = fsize;

    ifptr = fptr;
    fptr = nsect = 0;
    if (ofs) {
      bcs = (DWORD)fs->csize * SECTOR_SIZE;  /* Cluster size (byte) */
      if (ifptr > 0 && (ofs - 1) / bcs >= (ifptr - 1) / bcs) {
        /* When seek to same or following cluster, */
        fptr = (ifptr - 1) & ~(bcs - 1);  /* start from the current cluster */
        ofs -= fptr;
        clst = clust;
        }
      else {
        /* When seek to back cluster, start from the first cluster */
        clst = sclust;
        if (clst == 0) {
          /* If no cluster chain, create a new chain */
          clst = fs->createChain (0);
          if (clst == 1)
            ABORT(fs, FR_INT_ERR);
          if (clst == 0xFFFFFFFF)
            ABORT(fs, FR_DISK_ERR);
          sclust = clst;
          }
        clust = clst;
        }

      if (clst != 0) {
        while (ofs > bcs) {
          /* Cluster following loop */
          if (flag & FA_WRITE) {
            /* Check if in write mode or not */
            clst = fs->createChain (clst);  /* Force stretch if in write mode */
            if (clst == 0) {
              /* When disk gets full, clip file size */
              ofs = bcs;
              break;
              }
            }
          else
            clst = fs->getFat (clst); /* Follow cluster chain if not in write mode */

          if (clst == 0xFFFFFFFF)
            ABORT(fs, FR_DISK_ERR);
          if (clst <= 1 || clst >= fs->numFatEntries)
            ABORT(fs, FR_INT_ERR);

          clust = clst;
          fptr += bcs;
          ofs -= bcs;
          }

        fptr += ofs;
        if (ofs % SECTOR_SIZE) {
          nsect = fs->clusterToSector (clst); /* Current sector */
          if (!nsect)
            ABORT(fs, FR_INT_ERR);
          nsect += ofs / SECTOR_SIZE;
          }
        }
      }

    if (fptr % SECTOR_SIZE && nsect != dsect) {
      /* Fill sector cache if needed */
      if (flag & FA__DIRTY) {
        /* Write-back dirty sector cache */
        if (diskWrite (fs->drv, fileBuffer, dsect, 1) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        flag &= ~FA__DIRTY;
        }

      if (diskRead (fs->drv, fileBuffer, nsect, 1) != RES_OK)
        /* Fill sector cache */
        ABORT(fs, FR_DISK_ERR);
      dsect = nsect;
      }

    if (fptr > fsize) {
      /* Set file change flag if the file size is extended */
      fsize = fptr;
      flag |= FA__WRITTEN;
      }
    }

  fs->unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFile::read (void* buff, UINT btr, UINT* br) {

  DWORD clst, sect;
  UINT rcnt, cc;
  BYTE csect;
  BYTE* rbuff = (BYTE*)buff;

  *br = 0;
  FRESULT res = validateFile();
  if (res != FR_OK)
    LEAVE_FF(fs, res);
  if (err)
    LEAVE_FF(fs, (FRESULT)err);
  if (!(flag & FA_READ))
    LEAVE_FF(fs, FR_DENIED);

  DWORD remain = fsize - fptr;
  if (btr > remain)
    btr = (UINT)remain;   /* Truncate btr by remaining bytes */

  for ( ;  btr; /* Repeat until all data read */ rbuff += rcnt, fptr += rcnt, *br += rcnt, btr -= rcnt) {
    if ((fptr % SECTOR_SIZE) == 0) {
      /* On the sector boundary? */
      csect = (BYTE)(fptr / SECTOR_SIZE & (fs->csize - 1));  /* Sector offset in the cluster */
      if (!csect) {
        /* On the cluster boundary? */
        if (fptr == 0)
          /* On the top of the file? */
          clst = sclust;  /* Follow from the origin */
        else {
          /* Middle or end of the file */
          if (cltbl)
            clst = clmtCluster (fptr);  /* Get cluster# from the CLMT */
          else
            clst = fs->getFat (clust);  /* Follow cluster chain on the FAT */
          }
        if (clst < 2)
          ABORT(fs, FR_INT_ERR);
        if (clst == 0xFFFFFFFF)
          ABORT(fs, FR_DISK_ERR);
        clust = clst;  /* Update current cluster */
        }

      sect = fs->clusterToSector (clust); /* Get current sector */
      if (!sect)
        ABORT(fs, FR_INT_ERR);
      sect += csect;
      cc = btr / SECTOR_SIZE; /* When remaining bytes >= sector size, */
      if (cc) {
        /* Read maximum contiguous sectors directly */
        if (csect + cc > fs->csize) /* Clip at cluster boundary */
          cc = fs->csize - csect;
        if (diskRead (fs->drv, rbuff, sect, cc) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        if ((flag & FA__DIRTY) && dsect - sect < cc)
          memcpy(rbuff + ((dsect - sect) * SECTOR_SIZE), fileBuffer, SECTOR_SIZE);
        rcnt = SECTOR_SIZE * cc;     /* Number of bytes transferred */
        continue;
        }

      if (dsect != sect) {
        /* Load data sector if not in cache */
        if (flag & FA__DIRTY) {
          /* Write-back dirty sector cache */
          if (diskWrite (fs->drv, fileBuffer, dsect, 1) != RES_OK)
            ABORT(fs, FR_DISK_ERR);
          flag &= ~FA__DIRTY;
          }
        if (diskRead (fs->drv, fileBuffer, sect, 1) != RES_OK)  /* Fill sector cache */
          ABORT(fs, FR_DISK_ERR);
        }
      dsect = sect;
      }

    rcnt = SECTOR_SIZE - ((UINT)fptr % SECTOR_SIZE);  /* Get partial sector data from sector buffer */
    if (rcnt > btr)
      rcnt = btr;
    memcpy (rbuff, &fileBuffer[fptr % SECTOR_SIZE], rcnt); /* Pick partial sector */
    }

  fs->unlock (FR_OK);
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

  FRESULT res = validateFile();           /* Check validity */
  if (res != FR_OK)
    LEAVE_FF(fs, res);
  if (err)
    LEAVE_FF(fs, (FRESULT)err);
  if (!(flag & FA_WRITE))
    LEAVE_FF(fs, FR_DENIED);

  /* File size cannot reach 4GB */
  if (fptr + btw < fptr)
    btw = 0;

  for ( ;  btw; wbuff += wcnt, fptr += wcnt, *bw += wcnt, btw -= wcnt) {
    /* Repeat until all data written */
    if ((fptr % SECTOR_SIZE) == 0) {
      /* On the sector boundary? , Sector offset in the cluster */
      csect = (BYTE)(fptr / SECTOR_SIZE & (fs->csize - 1));
      if (!csect) {
        /* On the cluster boundary? */
        if (fptr == 0) {
          /* On the top of the file? */
          clst = sclust;
          /* Follow from the origin */
          if (clst == 0)
            /* When no cluster is allocated, */
            clst = fs->createChain (0); /* Create a new cluster chain */
          }
        else {
          /* Middle or end of the file */
          if (cltbl)
            clst = clmtCluster (fptr);  /* Get cluster# from the CLMT */
          else
            clst = fs->createChain (clust); /* Follow or stretch cluster chain on the FAT */
          }

        if (clst == 0)
          break;   /* Could not allocate a new cluster (disk full) */
        if (clst == 1)
          ABORT(fs, FR_INT_ERR);
        if (clst == 0xFFFFFFFF)
          ABORT(fs, FR_DISK_ERR);

        clust = clst;     /* Update current cluster */
        if (sclust == 0)
          sclust = clst; /* Set start cluster if the first write */
        }

      if (flag & FA__DIRTY) {
        /* Write-back sector cache */
        if (diskWrite (fs->drv, fileBuffer, dsect, 1) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        flag &= ~FA__DIRTY;
        }

      sect = fs->clusterToSector (clust); /* Get current sector */
      if (!sect)
        ABORT(fs, FR_INT_ERR);
      sect += csect;

      cc = btw / SECTOR_SIZE;      /* When remaining bytes >= sector size, */
      if (cc) {           /* Write maximum contiguous sectors directly */
        if (csect + cc > fs->csize) /* Clip at cluster boundary */
          cc = fs->csize - csect;
        if (diskWrite (fs->drv, wbuff, sect, cc) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        if (dsect - sect < cc) { /* Refill sector cache if it gets invalidated by the direct write */
          memcpy (fileBuffer, wbuff + ((dsect - sect) * SECTOR_SIZE), SECTOR_SIZE);
          flag &= ~FA__DIRTY;
          }
        wcnt = SECTOR_SIZE * cc;   /* Number of bytes transferred */
        continue;
        }

      if (dsect != sect) {
        /* Fill sector cache with file data */
        if (fptr < fsize && diskRead (fs->drv, fileBuffer, sect, 1) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        }
      dsect = sect;
      }

    /* Put partial sector into file I/O buffer */
    wcnt = SECTOR_SIZE - ((UINT)fptr % SECTOR_SIZE);
    if (wcnt > btw)
      wcnt = btw;

    /* Fit partial sector */
    memcpy (&fileBuffer[fptr % SECTOR_SIZE], wbuff, wcnt);
    flag |= FA__DIRTY;
    }

  /* Update file size if needed */
  if (fptr > fsize)
    fsize = fptr;

  /* Set file change flag */
  flag |= FA__WRITTEN;

  fs->unlock (FR_OK);
  return FR_OK;
  }
//}}}
//{{{
FRESULT cFile::truncate() {

  DWORD ncl;
  FRESULT res = validateFile();
  if (res == FR_OK) {
    if (err)             /* Check error */
      res = (FRESULT)err;
    else {
      if (!(flag & FA_WRITE))   /* Check access mode */
        res = FR_DENIED;
      }
    }

  if (res == FR_OK) {
    if (fsize > fptr) {
      fsize = fptr; /* Set file size to current R/W point */
      flag |= FA__WRITTEN;
      if (fptr == 0) {
        /* When set file size to zero, remove entire cluster chain */
        res = fs->removeChain (sclust);
        sclust = 0;
        }
      else {
        /* When truncate a part of the file, remove remaining clusters */
        ncl = fs->getFat (clust);
        res = FR_OK;
        if (ncl == 0xFFFFFFFF)
          res = FR_DISK_ERR;
        if (ncl == 1)
          res = FR_INT_ERR;
        if (res == FR_OK && ncl < fs->numFatEntries) {
          res = fs->putFat (clust, 0x0FFFFFFF);
          if (res == FR_OK)
            res = fs->removeChain (ncl);
          }
        }

      if (res == FR_OK && (flag & FA__DIRTY)) {
        if (diskWrite (fs->drv, fileBuffer, dsect, 1) != RES_OK)
          res = FR_DISK_ERR;
        else
          flag &= ~FA__DIRTY;
        }
      }
    if (res != FR_OK)
      err = (FRESULT)res;
    }

  fs->unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFile::sync() {

  DWORD tm;
  BYTE* dir;

  FRESULT res = validateFile();         /* Check validity of the object */
  if (res == FR_OK) {
    if (flag & FA__WRITTEN) {
      /* Has the file been written? ,  Write-back dirty buffer */
      if (flag & FA__DIRTY) {
        if (diskWrite (fs->drv, fileBuffer, dsect, 1) != RES_OK)
          LEAVE_FF(fs, FR_DISK_ERR);
        flag &= ~FA__DIRTY;
        }

      /* Update the directory entry */
      res = fs->moveWindow (dir_sect);
      if (res == FR_OK) {
        dir = dir_ptr;
        dir[DIR_Attr] |= AM_ARC;          /* Set archive bit */
        ST_DWORD(dir + DIR_FileSize, fsize);  /* Update file size */
        storeCluster (dir, sclust);          /* Update start cluster */
        tm = getFatTime();             /* Update updated time */
        ST_DWORD(dir + DIR_WrtTime, tm);
        ST_WORD(dir + DIR_LstAccDate, 0);
        flag &= ~FA__WRITTEN;
        fs->wflag = 1;
        res = fs->syncFs();
        }
      }
    }

  fs->unlock (res);
  return res;
  }
//}}}
//{{{
FRESULT cFile::close() {

  FRESULT res = sync();
  if (res == FR_OK) {
    res = validateFile();
    if (res == FR_OK) {
      cFatFs* fatFs = fs;

      // Decrement file open counter
      res = fs->decFileLock (lockid);
      if (res == FR_OK)
        // Invalidate file object
        fs = 0;

      fatFs->unlock (FR_OK);
      }
    }

  return res;
  }
//}}}

//{{{
class cPutBuffer {
public:
  //{{{
  void putChBuffered (TCHAR c) {

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
int cFile::putCh (TCHAR c) {

  /* Initialize output buffer */
  cPutBuffer putBuffer;
  putBuffer.file = this;
  putBuffer.nchr = 0;
  putBuffer.idx = 0;

  /* Put a character */
  putBuffer.putChBuffered (c);

  UINT nw;
  if (putBuffer.idx >= 0 && write (putBuffer.buf, (UINT)putBuffer.idx, &nw) == FR_OK && (UINT)putBuffer.idx == nw)
    return putBuffer.nchr;

  return -1;
  }
//}}}
//{{{
int cFile::putStr (const TCHAR* str) {

  /* Initialize output buffer */
  cPutBuffer putBuffer;
  putBuffer.file = this;
  putBuffer.nchr = putBuffer.idx = 0;

  /* Put the string */
  while (*str)
    putBuffer.putChBuffered (*str++);

  UINT nw;
  if (putBuffer.idx >= 0 && write (putBuffer.buf, (UINT)putBuffer.idx, &nw) == FR_OK && (UINT)putBuffer.idx == nw)
    return putBuffer.nchr;

  return -1;
  }
//}}}
//{{{
int cFile::printf (const TCHAR* fmt, ...) {

  BYTE f, r;
  UINT nw, i, j, w;
  DWORD v;
  TCHAR c, d, s[16], *p;

  /* Initialize output buffer */
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
      /* Non escape character */
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
    while (IsDigit(c)) {
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
    if (IsLower (d))
      d -= 0x20;

    switch (d) {        /* Type is... */
      //{{{
      case 'S' :          /* String */
        p = va_arg(arp, TCHAR*);
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
      case 'C' :          /* Character */
        putBuffer.putChBuffered ((TCHAR)va_arg(arp, int));
        continue;
      //}}}
      //{{{
      case 'B' :          /* Binary */
        r = 2;
        break;
      //}}}
      //{{{
      case 'O' :          /* Octal */
        r = 8;
        break;
      //}}}
      case 'D' :          /* Signed decimal */
      //{{{
      case 'U' :          /* Unsigned decimal */
        r = 10;
        break;
      //}}}
      //{{{
      case 'X' :          /* Hexdecimal */
        r = 16;
        break;
      //}}}
      //{{{
      default:          /* Unknown type (pass-through) */
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
      putBuffer.putChBuffered (d);

    do
      putBuffer.putChBuffered (s[--i]);
      while (i);

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
TCHAR* cFile::gets (TCHAR* buff, int len) {

  int n = 0;
  TCHAR c, *p = buff;
  BYTE s[2];
  UINT rc;

  while (n < len - 1) {
    /* Read characters until buffer gets filled */
    read (s, 1, &rc);
    if (rc != 1)
      break;
    c = s[0];
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
//}}}
//{{{  cFile private members
//{{{
DWORD cFile::clmtCluster (DWORD ofs) {

  /* Top of CLMT */
  DWORD* tbl = cltbl + 1;

  /* Cluster order from top of the file */
  DWORD cl = ofs / SECTOR_SIZE / fs->csize;
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
//}}}
//{{{
FRESULT cFile::validateFile() {

  if (!fs ||
      !fs->fsType ||
      fs->id != id ||
      (diskStatus (fs->drv) & STA_NOINIT))
    return FR_INVALID_OBJECT;

  // lock access to file system
  if (!fs->lock())
    return FR_TIMEOUT;

  return FR_OK;
  }
//}}}
//}}}