//{{{  ff.c copyright
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

#include "ff.h"
#include "diskio.h"

#include "memory.h"
//}}}
//{{{  defines
#define _USE_TRIM    0

#define _FS_LOCK     2   // 0:Disable or >=1:Enable

#define N_FATS    1     // Number of FATs (1 or 2)
#define N_ROOTDIR 512   // Number of root directory entries for FAT12/16

// Character code support macros
#define IsUpper(c)  (((c)>='A')&&((c)<='Z'))
#define IsLower(c)  (((c)>='a')&&((c)<='z'))
#define IsDigit(c)  (((c)>='0')&&((c)<='9'))

// Multi-byte word access macros
#define LD_WORD(ptr)   (WORD)(((WORD)*((BYTE*)(ptr)+1)<<8)|(WORD)*(BYTE*)(ptr))
#define LD_DWORD(ptr)  (DWORD)(((DWORD)*((BYTE*)(ptr)+3)<<24)|((DWORD)*((BYTE*)(ptr)+2)<<16)|((WORD)*((BYTE*)(ptr)+1)<<8)|*(BYTE*)(ptr))

#define ST_WORD(ptr,val)  *(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8)
#define ST_DWORD(ptr,val) *(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8); *((BYTE*)(ptr)+2)=(BYTE)((DWORD)(val)>>16); *((BYTE*)(ptr)+3)=(BYTE)((DWORD)(val)>>24)

// name status flags defines
#define NSFLAG   11    // Index of name status byte in fn[]

#define NS_LOSS  0x01  // Out of 8.3 format
#define NS_LFN   0x02  // Force to create LFN entry
#define NS_LAST  0x04  // Last segment
#define NS_BODY  0x08  // Lower case flag (body)
#define NS_EXT   0x10  // Lower case flag (ext)
#define NS_DOT   0x20  // Dot entry
//}}}
//{{{  fatFs defines
#define MIN_FAT16    4086U    /* Minimum number of clusters as FAT16 */
#define MIN_FAT32   65526U    /* Minimum number of clusters as FAT32 */

// FAT sub type (FATFS.fs_type)
#define FS_FAT12  1
#define FS_FAT16  2
#define FS_FAT32  3

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
//}}}

//{{{
class cPutBuff {
public:
  cFile* fp;
  int idx, nchr;
  BYTE buf[64];
  };
//}}}
//{{{
class cFatFs {
public :
  union {
    UINT  d32[_MAX_SS/4]; // Force 32bits alignement
    BYTE   d8[_MAX_SS];   // Disk access window for Directory, FAT (and file data at tiny cfg)
    } win;

  BYTE  fs_type;    // FAT sub-type (0:Not mounted)
  BYTE  drv;        // Physical drive number
  osSemaphoreId semaphore; // Identifier of sync object
  WORD  id;         // File system mount ID

  BYTE  csize;      // Sectors per cluster (1,2,4...128)
  BYTE  n_fats;     // Number of FAT copies (1 or 2)
  BYTE  wflag;      // win[] flag (b0:dirty)
  BYTE  fsi_flag;   // FSINFO flags (b7:disabled, b0:dirty)

  WORD  n_rootdir;  // Number of root directory entries (FAT12/16)
  DWORD last_clust; // Last allocated cluster
  DWORD free_clust; // Number of free clusters
  DWORD cdir;       // Current directory start cluster (0:root)
  DWORD n_fatent;   // Number of FAT entries, = number of clusters + 2
  DWORD fsize;      // Sectors per FAT

  DWORD volbase;    // Volume start sector
  DWORD fatbase;    // FAT start sector
  DWORD dirbase;    // Root directory start sector (FAT32:Cluster#)
  DWORD database;   // Data start sector
  DWORD winsect;    // Current sector appearing in the win[]
  };
//}}}
//{{{  static const
static const BYTE LfnOfs[] = { 1,3,5,7,9,14,16,18,20,22,24,28,30 };  /* Offset of LFN characters in the directory entry */

static const WORD vst[] = { 1024,   512,  256,  128,   64,    32,   16,    8,    4,    2,   0};
static const WORD cst[] = {32768, 16384, 8192, 4096, 2048, 16384, 8192, 4096, 2048, 1024, 512};

//{{{
//  CP1252(0x80-0xFF) to Unicode conversion table
static const WCHAR Tbl[] = {
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
static const WCHAR tbl_lower[] = { 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
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
                                   0xFF50, 0xFF51, 0xFF52, 0xFF53, 0xFF54, 0xFF55, 0xFF56, 0xFF57, 0xFF58, 0xFF59, 0xFF5A, 0 };
//}}}
//{{{
static const WCHAR tbl_upper[] = { 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
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
                                   0xFF30, 0xFF31, 0xFF32, 0xFF33, 0xFF34, 0xFF35, 0xFF36, 0xFF37, 0xFF38, 0xFF39, 0xFF3A, 0 };
//}}}
//{{{
// Upper conversion table for extended characters
static const BYTE ExCvt[] = { 0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
                              0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0xAd,0x9B,0x8C,0x9D,0xAE,0x9F,
                              0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
                              0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
                              0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
                              0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
                              0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
                              0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0x9F };
//}}}

//}}}
//{{{  static vars
static cFatFs* FatFs[1]; // Pointer to the file system objects (logical drives)
static WORD Fsid;       // File system mount ID

class cFileSem {
public:
  cFatFs* fs; // Object ID 1, volume (NULL:blank entry)
  DWORD clu;  // Object ID 2, directory (0:root)
  WORD idx;   // Object ID 3, directory index
  WORD ctr;   // Object open counter, 0:none, 0x01..0xFF:read mode open count, 0x100:write mode
  };

static cFileSem Files[_FS_LOCK];  // Open object lock semaphores
//}}}

//{{{  lock, synchronisation
//{{{
static int ff_req_grant (osSemaphoreId semaphore) {

  int ret = 0;
  if (osSemaphoreWait (semaphore, 1000) == osOK)
    ret = 1;

  return ret;
  }
//}}}
//{{{
static void ff_rel_grant (osSemaphoreId semaphore) {

  osSemaphoreRelease (semaphore);
  }
//}}}

//{{{
static void unlock_fs (cFatFs* fs, FRESULT res) {

  if (fs && res != FR_NOT_ENABLED && res != FR_INVALID_DRIVE && res != FR_INVALID_OBJECT && res != FR_TIMEOUT)
    ff_rel_grant (fs->semaphore);
  }
//}}}

#define ENTER_FF(fs)      { if (!ff_req_grant (fs->semaphore)) return FR_TIMEOUT; }
#define LEAVE_FF(fs, res) { unlock_fs(fs, res); return res; }
#define ABORT(fs, res)    { err = (BYTE)(res); LEAVE_FF(fs, res); }

//{{{
static FRESULT chk_lock (cDirectory* dp, int acc) {

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
//}}}
//{{{
static int enq_lock() {

  UINT i;
  for (i = 0; i < _FS_LOCK && Files[i].fs; i++) ;
  return (i == _FS_LOCK) ? 0 : 1;
  }
//}}}
//{{{
static UINT inc_lock (cDirectory* dp, int acc) {

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
//}}}
//{{{
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
//}}}
//{{{
static void clear_lock (cFatFs* fs) {

  for (UINT i = 0; i < _FS_LOCK; i++)
    if (Files[i].fs == fs)
      Files[i].fs = 0;
  }
//}}}
//}}}
//{{{  char utils
//{{{
static DWORD get_fattime() {
// year, mon, day, time - 1.04.2016
  return ((2016 - 1980) << 25) | (4 << 21) | (1 << 16);
  }
//}}}

//{{{
static WCHAR ff_convert (WCHAR chr, UINT direction /* 0: Unicode to OEMCP, 1: OEMCP to Unicode */ ) {
// Converted character, Returns zero on error

  WCHAR c;
  if (chr < 0x80)
    // ASCII
    c = chr;
  else if (direction)
    // OEMCP to Unicode
    c = (chr >= 0x100) ? 0 : Tbl[chr - 0x80];
  else {
    // Unicode to OEMCP
    for (c = 0; c < 0x80; c++) {
      if (chr == Tbl[c])
        break;
      }
    c = (c + 0x80) & 0xFF;
    }

  return c;
  }
//}}}
//{{{
static WCHAR ff_wtoupper (WCHAR chr) {

  int i;
  for (i = 0; tbl_lower[i] && chr != tbl_lower[i]; i++) ;
  return tbl_lower[i] ? tbl_upper[i] : chr;
  }
//}}}
//{{{
static WCHAR get_achar (const TCHAR** ptr) {

#if _LFN_UNICODE

  return ff_wtoupper (*(*ptr)++); // Get a word and to upper

#else

  WCHAR chr = (BYTE)*(*ptr)++;
  if (IsLower (chr))
    chr -= 0x20; // To upper ASCII char

  #ifdef _EXCVT
    if (chr >= 0x80)
      chr = ExCvt[chr - 0x80]; // To upper SBCS extended char
  #endif

  return chr;

#endif
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
static void putc_bfd (cPutBuff* pb, TCHAR c) {

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
    pb->fp->f_write (pb->buf, (UINT)i, &bw);
    i = (bw == (UINT)i) ? 0 : -1;
    }

  pb->idx = i;
  pb->nchr++;
  }
//}}}
//}}}
//{{{  fs utils
// window
//{{{
static FRESULT syncWindow (cFatFs* fs) {

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
          disk_write (fs->drv, fs->win.d8, wsect, 1);
          }
        }
      }
    }

  return res;
  }
//}}}
//{{{
static FRESULT moveWindow (cFatFs* fs, DWORD sector) {

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
//}}}

// fs
//{{{
static BYTE checkFs (cFatFs* fs, DWORD sect) {

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
//}}}
//{{{
static FRESULT syncFs (cFatFs* fs) {

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
    if (disk_ioctl (fs->drv, CTRL_SYNC, 0) != RES_OK)
      res = FR_DISK_ERR;
    }

  return res;
  }
//}}}

// fat
//{{{
static DWORD getFat (cFatFs* fs, DWORD clst) {

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
//}}}
//{{{
static FRESULT putFat (cFatFs* fs, DWORD clst, DWORD val) {

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
//}}}

// cluster
//{{{
static DWORD clusterToSector (cFatFs* fs, DWORD clst) {

  clst -= 2;
  return (clst >= fs->n_fatent - 2) ? 0 : clst * fs->csize + fs->database;
  }
//}}}
//{{{
static DWORD loadCluster (cFatFs* fs, BYTE* dir) {

  DWORD cl = LD_WORD(dir + DIR_FstClusLO);
  if (fs->fs_type == FS_FAT32)
    cl |= (DWORD)LD_WORD(dir + DIR_FstClusHI) << 16;

  return cl;
  }
//}}}
//{{{
static void storeCluster (BYTE* dir, DWORD cl) {

  ST_WORD(dir + DIR_FstClusLO, cl);
  ST_WORD(dir + DIR_FstClusHI, cl >> 16);
  }
//}}}

//{{{
static FRESULT findVolume (cFatFs** returnedfs, BYTE wmode) {

  BYTE fmt, *pt;
  DWORD bsect, fasize, tsect, sysect, nclst, szbfat, br[4];
  WORD nrsv;
  UINT i;

  int vol = 0;
  cFatFs* fs = FatFs[vol];
  if (!ff_req_grant (fs->semaphore)) {
    *returnedfs = 0;
    return FR_TIMEOUT;
    }
  *returnedfs = fs;
  if (fs->fs_type) {
    // check volume mounted
    DSTATUS stat = disk_status (fs->drv);
    if (!(stat & STA_NOINIT)) {
      // and the physical drive is kept initialized
      if (wmode && (stat & STA_PROTECT)) // Check write protection if needed
        return FR_WRITE_PROTECTED;
      return FR_OK;
      }
    }

  // mount volume, analyze BPB and initialize the fs
  fs->fs_type = 0;
  fs->drv = vol;

  // init physical drive
  DSTATUS stat = disk_initialize (fs->drv);
  if (stat & STA_NOINIT)
    return FR_NOT_READY;
  if (wmode && (stat & STA_PROTECT))
    return FR_WRITE_PROTECTED;

  // find FAT partition, supports only generic partitioning, FDISK and SFD
  bsect = 0;
  fmt = checkFs (fs, bsect); // Load sector 0 and check if it is an FAT boot sector as SFD
  if (fmt == 1 || (!fmt && vol)) {
    // Not an FAT boot sector or forced partition number
    for (i = 0; i < 4; i++) {
      // Get partition offset
      pt = fs->win.d8 + MBR_Table + i * SZ_PTE;
      br[i] = pt[4] ? LD_DWORD(&pt[8]) : 0;
      }

    // Partition number: 0:auto, 1-4:forced
    i = vol;
    if (i)
      i--;
    do {
      // Find an FAT volume
      bsect = br[i];
      fmt = bsect ? checkFs (fs, bsect) : 2; // Check the partition
      } while (!vol && fmt && ++i < 4);
    }

  if (fmt == 3)
    return FR_DISK_ERR; // An error occured in the disk I/O layer
  if (fmt)
    return FR_NO_FILESYSTEM; // No FAT volume is found

  // FAT volume found, code initializes the file system
  if (LD_WORD(fs->win.d8 + BPB_BytsPerSec) != _MAX_SS) // (BPB_BytsPerSec must be equal to the physical sector size)
    return FR_NO_FILESYSTEM;

  fasize = LD_WORD(fs->win.d8 + BPB_FATSz16); // Number of sectors per FAT
  if (!fasize)
    fasize = LD_DWORD(fs->win.d8 + BPB_FATSz32);
  fs->fsize = fasize;

  fs->n_fats = fs->win.d8[BPB_NumFATs];   // Number of FAT copies
  if (fs->n_fats != 1 && fs->n_fats != 2) // (Must be 1 or 2)
    return FR_NO_FILESYSTEM;
  fasize *= fs->n_fats; // Number of sectors for FAT area

  fs->csize = fs->win.d8[BPB_SecPerClus];           // Number of sectors per cluster
  if (!fs->csize || (fs->csize & (fs->csize - 1)))  // (Must be power of 2)
    return FR_NO_FILESYSTEM;

  fs->n_rootdir = LD_WORD(fs->win.d8 + BPB_RootEntCnt); // Number of root directory entries
  if (fs->n_rootdir % (_MAX_SS / SZ_DIRE)             ) // (Must be sector aligned)
    return FR_NO_FILESYSTEM;

  tsect = LD_WORD(fs->win.d8 + BPB_TotSec16); /* Number of sectors on the volume */
  if (!tsect)
    tsect = LD_DWORD(fs->win.d8 + BPB_TotSec32);

  nrsv = LD_WORD(fs->win.d8 + BPB_RsvdSecCnt); /* Number of reserved sectors */
  if (!nrsv)
    return FR_NO_FILESYSTEM; /* (Must not be 0) */

  /* Determine the FAT sub type */
  sysect = nrsv + fasize + fs->n_rootdir / (_MAX_SS / SZ_DIRE); /* RSV + FAT + DIR */
  if (tsect < sysect)
    return FR_NO_FILESYSTEM; /* (Invalid volume size) */

  nclst = (tsect - sysect) / fs->csize; /* Number of clusters */
  if (!nclst)
    return FR_NO_FILESYSTEM; /* (Invalid volume size) */
  fmt = FS_FAT12;
  if (nclst >= MIN_FAT16)
    fmt = FS_FAT16;
  if (nclst >= MIN_FAT32)
    fmt = FS_FAT32;

  /* Boundaries and Limits */
  fs->n_fatent = nclst + 2;      /* Number of FAT entries */
  fs->volbase = bsect;           /* Volume start sector */
  fs->fatbase = bsect + nrsv;    /* FAT start sector */
  fs->database = bsect + sysect; /* Data start sector */
  if (fmt == FS_FAT32) {
    if (fs->n_rootdir)
      return FR_NO_FILESYSTEM;   /* (BPB_RootEntCnt must be 0) */
    fs->dirbase = LD_DWORD(fs->win.d8 + BPB_RootClus);  /* Root directory start cluster */
    szbfat = fs->n_fatent * 4;   /* (Needed FAT size) */
    }
  else {
    if (!fs->n_rootdir)
      return FR_NO_FILESYSTEM;  /* (BPB_RootEntCnt must not be 0) */
    fs->dirbase = fs->fatbase + fasize;  /* Root directory start sector */
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
//}}}

//chain
//{{{
static DWORD createChain (cFatFs* fs, DWORD clst) {

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
//}}}
//{{{
static FRESULT removeChain (cFatFs* fs, DWORD clst) {

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
        disk_ioctl (fs->drv, CTRL_TRIM, rt);       /* Erase the block */
        scl = ecl = nxt;
        }
#endif

      clst = nxt; /* Next cluster */
      }
    }

  return res;
  }
//}}}
//}}}
//{{{  lfn utils
//{{{
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
//}}}
//{{{
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
//}}}
//{{{
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
//}}}
//{{{
static BYTE sum_sfn (const BYTE* dir) {

  BYTE sum = 0;
  UINT n = 11;
  do sum = (sum >> 1) + (sum << 7) + *dir++;
    while (--n);

  return sum;
  }
//}}}
//{{{
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
  for (j = 0; j < i && dst[j] != ' '; j++);
  do {
    dst[j++] = (i < 8) ? ns[i++] : ' ';
    } while (j < 8);
  }
//}}}
//}}}

//{{{
FRESULT f_mount() {

  cFatFs* fs = (cFatFs*)FATFS_BUFFER;
  FatFs[0] = fs;

  fs->fs_type = 0;

  osSemaphoreDef (fatfs);
  fs->semaphore = osSemaphoreCreate (osSemaphore (fatfs), 1);

  // Do not mount now, it will be mounted later
  return FR_OK;
  }
//}}}

//{{{
FRESULT cFile::f_open (const TCHAR* path, BYTE mode) {

  cDirectory dj;
  BYTE* dir;
  DWORD dw, cl;

  fs = 0;

  mode &= FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW;
  FRESULT res = findVolume (&dj.fs, (BYTE)(mode & ~FA_READ));
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = dj.followPath (path); /* Follow the file path */
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
          res = enq_lock() ? dj.dir_register() : FR_TOO_MANY_OPEN_FILES;
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
        dw = get_fattime();       /* Created time */
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
      dir_sect = dj.fs->winsect;  /* Pointer to the directory entry */
      dir_ptr = dir;
      lockid = inc_lock (&dj, (mode & ~FA_READ) ? 1 : 0);
      if (!lockid)
        res = FR_INT_ERR;
      }

    if (res == FR_OK) {
      flag = mode;                          /* File access mode */
      err = 0;                              /* Clear error flag */
      sclust = loadCluster (dj.fs, dir);    /* File start cluster */
      fsize = LD_DWORD(dir + DIR_FileSize); /* File size */
      fptr = 0;                             /* File pointer */
      dsect = 0;
      cltbl = 0;                            /* Normal seek mode */
      fs = dj.fs;                           /* Validate file object */
      id = fs->id;
      }
    }

  LEAVE_FF(dj.fs, res);
  }
//}}}
//{{{
FRESULT cFile::f_lseek (DWORD ofs) {

  DWORD clst, bcs, nsect, ifptr;
  DWORD cl, pcl, ncl, tcl, dsc, tlen, ulen, *tbl;

  FRESULT res = validateFile();  /* Check validity of the object */
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
            cl = getFat (fs, cl);
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
          } while (cl < fs->n_fatent);  /* Repeat until end of chain */
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
        dsc = clusterToSector (fs, clust);
        if (!dsc)
          ABORT(fs, FR_INT_ERR);
        dsc += (ofs - 1) / _MAX_SS & (fs->csize - 1);
        if (fptr % _MAX_SS && dsc != dsect) {  /* Refill sector cache if needed */
          if (flag & FA__DIRTY) {
            /* Write-back dirty sector cache */
            if (disk_write (fs->drv, buf.d8, dsect, 1) != RES_OK)
              ABORT (fs, FR_DISK_ERR);
            flag &= ~FA__DIRTY;
            }
          if (disk_read (fs->drv, buf.d8, dsc, 1) != RES_OK) /* Load current sector */
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
      bcs = (DWORD)fs->csize * _MAX_SS;  /* Cluster size (byte) */
      if (ifptr > 0 && (ofs - 1) / bcs >= (ifptr - 1) / bcs) {
        /* When seek to same or following cluster, */
        fptr = (ifptr - 1) & ~(bcs - 1);  /* start from the current cluster */
        ofs -= fptr;
        clst = clust;
        }
      else {
        /* When seek to back cluster, */
        /* start from the first cluster */
        clst = sclust;
        if (clst == 0) {
          /* If no cluster chain, create a new chain */
          clst = createChain (fs, 0);
          if (clst == 1)
            ABORT(fs, FR_INT_ERR);
          if (clst == 0xFFFFFFFF)
            ABORT(fs, FR_DISK_ERR);
          sclust = clst;
          }
        clust = clst;
        }

      if (clst != 0) {
        while (ofs > bcs) {           /* Cluster following loop */
          if (flag & FA_WRITE) {      /* Check if in write mode or not */
            clst = createChain (fs, clst);  /* Force stretch if in write mode */
            if (clst == 0) {        /* When disk gets full, clip file size */
              ofs = bcs; break;
              }
            }
          else
            clst = getFat (fs, clst); /* Follow cluster chain if not in write mode */
          if (clst == 0xFFFFFFFF)
            ABORT(fs, FR_DISK_ERR);
          if (clst <= 1 || clst >= fs->n_fatent)
            ABORT(fs, FR_INT_ERR);
          clust = clst;
          fptr += bcs;
          ofs -= bcs;
          }

        fptr += ofs;
        if (ofs % _MAX_SS) {
          nsect = clusterToSector (fs, clst); /* Current sector */
          if (!nsect)
            ABORT(fs, FR_INT_ERR);
          nsect += ofs / _MAX_SS;
          }
        }
      }

    if (fptr % _MAX_SS && nsect != dsect) {
      /* Fill sector cache if needed */
      if (flag & FA__DIRTY) {
        /* Write-back dirty sector cache */
        if (disk_write (fs->drv, buf.d8, dsect, 1) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        flag &= ~FA__DIRTY;
        }

      if (disk_read (fs->drv, buf.d8, nsect, 1) != RES_OK)
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

  LEAVE_FF(fs, res);
  }
//}}}
//{{{
FRESULT cFile::f_read (void* buff, UINT btr, UINT* br) {

  DWORD clst, sect;
  UINT rcnt, cc;
  BYTE csect;
  BYTE* rbuff = (BYTE*)buff;

  *br = 0;  /* Clear read byte counter */
  FRESULT res = validateFile(); /* Check validity */
  if (res != FR_OK)
    LEAVE_FF(fs, res);
  if (err)                /* Check error */
    LEAVE_FF(fs, (FRESULT)err);
  if (!(flag & FA_READ))   /* Check access mode */
    LEAVE_FF(fs, FR_DENIED);

  DWORD remain = fsize - fptr;
  if (btr > remain)
    btr = (UINT)remain;   /* Truncate btr by remaining bytes */

  for ( ;  btr; /* Repeat until all data read */ rbuff += rcnt, fptr += rcnt, *br += rcnt, btr -= rcnt) {
    if ((fptr % _MAX_SS) == 0) {
      /* On the sector boundary? */
      csect = (BYTE)(fptr / _MAX_SS & (fs->csize - 1));  /* Sector offset in the cluster */
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
            clst = getFat (fs, clust);  /* Follow cluster chain on the FAT */
          }
        if (clst < 2)
          ABORT(fs, FR_INT_ERR);
        if (clst == 0xFFFFFFFF)
          ABORT(fs, FR_DISK_ERR);
        clust = clst;  /* Update current cluster */
        }

      sect = clusterToSector (fs, clust); /* Get current sector */
      if (!sect)
        ABORT(fs, FR_INT_ERR);
      sect += csect;
      cc = btr / _MAX_SS; /* When remaining bytes >= sector size, */
      if (cc) {
        /* Read maximum contiguous sectors directly */
        if (csect + cc > fs->csize) /* Clip at cluster boundary */
          cc = fs->csize - csect;
        if (disk_read (fs->drv, rbuff, sect, cc) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        if ((flag & FA__DIRTY) && dsect - sect < cc)
          memcpy(rbuff + ((dsect - sect) * _MAX_SS), buf.d8, _MAX_SS);
        rcnt = _MAX_SS * cc;     /* Number of bytes transferred */
        continue;
        }

      if (dsect != sect) {
        /* Load data sector if not in cache */
        if (flag & FA__DIRTY) {
          /* Write-back dirty sector cache */
          if (disk_write (fs->drv, buf.d8, dsect, 1) != RES_OK)
            ABORT(fs, FR_DISK_ERR);
          flag &= ~FA__DIRTY;
          }
        if (disk_read (fs->drv, buf.d8, sect, 1) != RES_OK)  /* Fill sector cache */
          ABORT(fs, FR_DISK_ERR);
        }
      dsect = sect;
      }

    rcnt = _MAX_SS - ((UINT)fptr % _MAX_SS);  /* Get partial sector data from sector buffer */
    if (rcnt > btr)
      rcnt = btr;
    memcpy (rbuff, &buf.d8[fptr % _MAX_SS], rcnt); /* Pick partial sector */
    }

  LEAVE_FF(fs, FR_OK);
  }
//}}}
//{{{
FRESULT cFile::f_write (const void *buff, UINT btw, UINT* bw) {

  DWORD clst, sect;
  UINT wcnt, cc;
  const BYTE *wbuff = (const BYTE*)buff;
  BYTE csect;

  *bw = 0;  /* Clear write byte counter */

  FRESULT res = validateFile();           /* Check validity */
  if (res != FR_OK)
    LEAVE_FF(fs, res);
  if (err)              /* Check error */
    LEAVE_FF(fs, (FRESULT)err);
  if (!(flag & FA_WRITE))       /* Check access mode */
    LEAVE_FF(fs, FR_DENIED);
  if (fptr + btw < fptr) btw = 0; /* File size cannot reach 4GB */

  for ( ;  btw;             /* Repeat until all data written */
    wbuff += wcnt, fptr += wcnt, *bw += wcnt, btw -= wcnt) {
    if ((fptr % _MAX_SS) == 0) { /* On the sector boundary? */
      csect = (BYTE)(fptr / _MAX_SS & (fs->csize - 1));  /* Sector offset in the cluster */
      if (!csect) {
        /* On the cluster boundary? */
        if (fptr == 0) {
          /* On the top of the file? */
          clst = sclust;
          /* Follow from the origin */
          if (clst == 0)
            /* When no cluster is allocated, */
            clst = createChain (fs, 0); /* Create a new cluster chain */
          }
        else {
          /* Middle or end of the file */
          if (cltbl)
            clst = clmtCluster (fptr);  /* Get cluster# from the CLMT */
          else
            clst = createChain (fs, clust); /* Follow or stretch cluster chain on the FAT */
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

      if (flag & FA__DIRTY) {   /* Write-back sector cache */
        if (disk_write (fs->drv, buf.d8, dsect, 1) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        flag &= ~FA__DIRTY;
        }

      sect = clusterToSector (fs, clust); /* Get current sector */
      if (!sect)
        ABORT(fs, FR_INT_ERR);
      sect += csect;

      cc = btw / _MAX_SS;      /* When remaining bytes >= sector size, */
      if (cc) {           /* Write maximum contiguous sectors directly */
        if (csect + cc > fs->csize) /* Clip at cluster boundary */
          cc = fs->csize - csect;
        if (disk_write (fs->drv, wbuff, sect, cc) != RES_OK)
          ABORT(fs, FR_DISK_ERR);
        if (dsect - sect < cc) { /* Refill sector cache if it gets invalidated by the direct write */
          memcpy (buf.d8, wbuff + ((dsect - sect) * _MAX_SS), _MAX_SS);
          flag &= ~FA__DIRTY;
          }
        wcnt = _MAX_SS * cc;   /* Number of bytes transferred */
        continue;
        }

      if (dsect != sect) {    /* Fill sector cache with file data */
        if (fptr < fsize &&
          disk_read (fs->drv, buf.d8, sect, 1) != RES_OK)
            ABORT(fs, FR_DISK_ERR);
        }
      dsect = sect;
      }

    wcnt = _MAX_SS - ((UINT)fptr % _MAX_SS); /* Put partial sector into file I/O buffer */
    if (wcnt > btw)
      wcnt = btw;
    memcpy (&buf.d8[fptr % _MAX_SS], wbuff, wcnt); /* Fit partial sector */
    flag |= FA__DIRTY;
    }

  if (fptr > fsize)
    fsize = fptr;  /* Update file size if needed */
  flag |= FA__WRITTEN; /* Set file change flag */

  LEAVE_FF(fs, FR_OK);
  }
//}}}
//{{{
FRESULT cFile::f_truncate() {

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
      if (fptr == 0) {  /* When set file size to zero, remove entire cluster chain */
        res = removeChain (fs, sclust);
        sclust = 0;
        }
      else {
        /* When truncate a part of the file, remove remaining clusters */
        ncl = getFat (fs, clust);
        res = FR_OK;
        if (ncl == 0xFFFFFFFF)
          res = FR_DISK_ERR;
        if (ncl == 1)
          res = FR_INT_ERR;
        if (res == FR_OK && ncl < fs->n_fatent) {
          res = putFat (fs, clust, 0x0FFFFFFF);
          if (res == FR_OK)
            res = removeChain (fs, ncl);
          }
        }

      if (res == FR_OK && (flag & FA__DIRTY)) {
        if (disk_write (fs->drv, buf.d8, dsect, 1) != RES_OK)
          res = FR_DISK_ERR;
        else
          flag &= ~FA__DIRTY;
        }
      }
    if (res != FR_OK)
      err = (FRESULT)res;
    }

  LEAVE_FF(fs, res);
  }
//}}}
//{{{
FRESULT cFile::f_sync() {

  DWORD tm;
  BYTE* dir;

  FRESULT res = validateFile();         /* Check validity of the object */
  if (res == FR_OK) {
    if (flag & FA__WRITTEN) {
      /* Has the file been written? ,  Write-back dirty buffer */
      if (flag & FA__DIRTY) {
        if (disk_write (fs->drv, buf.d8, dsect, 1) != RES_OK)
          LEAVE_FF(fs, FR_DISK_ERR);
        flag &= ~FA__DIRTY;
        }

      /* Update the directory entry */
      res = moveWindow (fs, dir_sect);
      if (res == FR_OK) {
        dir = dir_ptr;
        dir[DIR_Attr] |= AM_ARC;          /* Set archive bit */
        ST_DWORD(dir + DIR_FileSize, fsize);  /* Update file size */
        storeCluster (dir, sclust);          /* Update start cluster */
        tm = get_fattime();             /* Update updated time */
        ST_DWORD(dir + DIR_WrtTime, tm);
        ST_WORD(dir + DIR_LstAccDate, 0);
        flag &= ~FA__WRITTEN;
        fs->wflag = 1;
        res = syncFs (fs);
        }
      }
    }

  LEAVE_FF(fs, res);
  }
//}}}
//{{{
FRESULT cFile::f_close() {

  FRESULT res = f_sync();
  if (res == FR_OK) {
    res = validateFile();          // Lock volume
    if (res == FR_OK) {
      cFatFs* fs1 = fs;
      res = dec_lock (lockid);     // Decrement file open counter
      if (res == FR_OK)
        fs = 0;              // Invalidate file object
      unlock_fs (fs1, FR_OK);       // Unlock volume
      }
    }

  return res;
  }
//}}}
//{{{
int cFile::f_putc (TCHAR c) {

  /* Initialize output buffer */
  cPutBuff pb;
  pb.fp = this;
  pb.nchr = pb.idx = 0;

  /* Put a character */
  putc_bfd (&pb, c);

  UINT nw;
  if (pb.idx >= 0 && f_write (pb.buf, (UINT)pb.idx, &nw) == FR_OK && (UINT)pb.idx == nw)
    return pb.nchr;

  return -1;
  }
//}}}
//{{{
int cFile::f_puts (const TCHAR* str) {

  /* Initialize output buffer */
  cPutBuff pb;
  pb.fp = this;
  pb.nchr = pb.idx = 0;

  /* Put the string */
  while (*str)
    putc_bfd(&pb, *str++);

  UINT nw;
  if (pb.idx >= 0 && f_write (pb.buf, (UINT)pb.idx, &nw) == FR_OK && (UINT)pb.idx == nw)
    return pb.nchr;

  return -1;
  }
//}}}
//{{{
int cFile::f_printf (const TCHAR* fmt, ...) {

  BYTE f, r;
  UINT nw, i, j, w;
  DWORD v;
  TCHAR c, d, s[16], *p;

  /* Initialize output buffer */
  cPutBuff pb;
  pb.fp = this;
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
            putc_bfd(&pb, ' ');
          }
        while (*p)
          putc_bfd (&pb, *p++);
        while (j++ < w)
          putc_bfd (&pb, ' ');
        continue;
      //}}}
      //{{{
      case 'C' :          /* Character */
        putc_bfd(&pb, (TCHAR)va_arg(arp, int));
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
        putc_bfd(&pb, c);
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
      putc_bfd (&pb, d);

    do
      putc_bfd (&pb, s[--i]);
      while (i);

    while (j++ < w)
      putc_bfd (&pb, d);
    }

  va_end (arp);

  if (pb.idx >= 0 && pb.fp->f_write (pb.buf, (UINT)pb.idx, &nw) == FR_OK && (UINT)pb.idx == nw)
    return pb.nchr;

  return -1;
  }
//}}}
//{{{
TCHAR* cFile::f_gets (TCHAR* buff, int len) {

  int n = 0;
  TCHAR c, *p = buff;
  BYTE s[2];
  UINT rc;

  while (n < len - 1) {
    /* Read characters until buffer gets filled */
#if _LFN_UNICODE
#if _STRF_ENCODE == 3
    /* Read a character in UTF-8 */
    f_read (s, 1, &rc);
    if (rc != 1) break;
    c = s[0];
    if (c >= 0x80) {
      if (c < 0xC0)
        continue; /* Skip stray trailer */
      if (c < 0xE0) {     /* Two-byte sequence */
        f_read (s, 1, &rc);
        if (rc != 1)
          break;
        c = (c & 0x1F) << 6 | (s[0] & 0x3F);
        if (c < 0x80)
          c = '?';
        }
      else {
        if (c < 0xF0) {   /* Three-byte sequence */
          f_read (s, 2, &rc);
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
    f_read (s, 2, &rc);
    if (rc != 2)
      break;
    c = s[1] + (s[0] << 8);
#elif _STRF_ENCODE == 1   /* Read a character in UTF-16LE */
    f_read (s, 2, &rc);
    if (rc != 2)
      break;
    c = s[0] + (s[1] << 8);
#else           /* Read a character in ANSI/OEM */
    f_read (s, 1, &rc);
    if (rc != 1)
      break;
    c = s[0];
    c = ff_convert(c, 1); /* OEM -> Unicode */
    if (!c) c = '?';
#endif
#else           /* Read a character without conversion */
    f_read (s, 1, &rc);
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
//}}}
//{{{
DWORD cFile::clmtCluster (DWORD ofs) {

  /* Top of CLMT */
  DWORD* tbl = cltbl + 1;

  /* Cluster order from top of the file */
  DWORD cl = ofs / _MAX_SS / fs->csize;
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
      !fs->fs_type ||
      fs->id != id ||
      (disk_status (fs->drv) & STA_NOINIT))
    return FR_INVALID_OBJECT;

  // Lock file system
  if (!ff_req_grant (fs->semaphore))
    return FR_TIMEOUT;

  return FR_OK;
  }
//}}}

//{{{
FRESULT cDirectory::f_opendir (const TCHAR* path) {

  // get logical drive number
  cFatFs* fs1;
  FRESULT res = findVolume (&fs1, 0);
  if (res == FR_OK) {
    fs = fs1;

    BYTE sfn1[12];
    WCHAR lfn1[(_MAX_LFN + 1) * 2];
    lfn = lfn1;
    fn = sfn1;
    res = followPath (path);
    if (res == FR_OK) {
      if (dir) {
        // not itself */
        if (dir[DIR_Attr] & AM_DIR) // subDir
          sclust = loadCluster (fs, dir);
        else // file
          res = FR_NO_PATH;
        }

      if (res == FR_OK) {
        id = fs->id;
        // Rewind directory
        res = dir_sdi (0);
        if (res == FR_OK) {
          if (sclust) {
            // lock subDir
            lockid = inc_lock (this, 0);
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

  LEAVE_FF(fs, res);
  }
//}}}
//{{{
FRESULT cDirectory::f_findfirst (cFileInfo* fileInfo, const TCHAR* path, const TCHAR* pattern) {

  pat = pattern;                   // Save pointer to pattern string
  FRESULT res = f_opendir (path);  // Open the target directory
  if (res == FR_OK)
    res = f_findnext (fileInfo);   // Find the first item
  return res;
  }
//}}}
//{{{
FRESULT cDirectory::f_readdir (cFileInfo* fileInfo) {

  FRESULT res = validateDir();
  if (res == FR_OK) {
    if (!fileInfo)
      res = dir_sdi (0);     /* Rewind the directory object */
    else {
      BYTE sfn1[12];
      WCHAR lfn1 [(_MAX_LFN + 1) * 2];
      lfn = lfn1;
      fn = sfn1;
      res = dir_read (0);
      if (res == FR_NO_FILE) {
        /* Reached end of directory */
        sect = 0;
        res = FR_OK;
        }

      if (res == FR_OK) {
        /* A valid entry is found */
        getFileInfo (fileInfo);    /* Get the object information */
        res = dir_next (0);    /* Increment index for next */
        if (res == FR_NO_FILE) {
          sect = 0;
          res = FR_OK;
          }
        }
      }
    }

  LEAVE_FF(fs, res);
  }
//}}}
//{{{
FRESULT cDirectory::f_findnext (cFileInfo* fileInfo) {

  FRESULT res;
  for (;;) {
    res = f_readdir (fileInfo);
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
FRESULT cDirectory::f_closedir() {

  FRESULT res = validateDir();
  if (res == FR_OK) {
    cFatFs* fs1 = fs;
    if (lockid)       /* Decrement sub-directory open counter */
      res = dec_lock (lockid);
    if (res == FR_OK)
      fs = 0;       /* Invalidate directory object */
    unlock_fs (fs1, FR_OK);   /* Unlock volume */
    }

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::validateDir() {

  if (!fs ||
      !fs->fs_type ||
      fs->id != id ||
      (disk_status (fs->drv) & STA_NOINIT))
    return FR_INVALID_OBJECT;

  // Lock file system
  if (!ff_req_grant (fs->semaphore))
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
    res = dir_sdi (0);
    dir = 0;
    }

  else {
    for (;;) {
      res = createName (&path); /* Get a segment name of the path */
      if (res != FR_OK)
        break;
      res = dir_find();       /* Find an object with the sagment name */
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
            res = FR_NO_PATH;  /* Adjust error code if not last segment */
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

      sclust = loadCluster (fs, dir1);
      }
    }

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::createName (const TCHAR** path) {

  BYTE b, cf;
  WCHAR w, *lfn1;
  UINT i, ni, si, di;
  const TCHAR *p;

  /* Create LFN in Unicode */
  for (p = *path; *p == '/' || *p == '\\'; p++) ; /* Strip duplicated separator */
  lfn1 = lfn;
  si = di = 0;
  for (;;) {
    w = p[si++];          /* Get a character */
    if (w < ' ' || w == '/' || w == '\\')
      break;  /* Break on end of segment */
    if (di >= _MAX_LFN)       /* Reject too long name */
      return FR_INVALID_NAME;

#if !_LFN_UNICODE
    w &= 0xFF;
    w = ff_convert (w, 1);     /* Convert ANSI/OEM to Unicode */
    if (!w)
      return FR_INVALID_NAME; /* Reject invalid code */
#endif

    if (w < 0x80 && strchr ("\"*:<>\?|\x7F", w)) /* Reject illegal characters for LFN */
      return FR_INVALID_NAME;
    lfn1[di++] = w;          /* Store the Unicode character */
    }

  *path = &p[si];           /* Return pointer to the next segment */
  cf = (w < ' ') ? NS_LAST : 0;   /* Set last segment flag if end of path */
  if ((di == 1 && lfn1[di - 1] == '.') || (di == 2 && lfn1[di - 1] == '.' && lfn1[di - 2] == '.')) {
    lfn1[di] = 0;
    for (i = 0; i < 11; i++)
      fn[i] = (i < di) ? '.' : ' ';
    fn[i] = cf | NS_DOT;    /* This is a dot entry */
    return FR_OK;
    }

  while (di) {
    /* Strip trailing spaces and dots */
    w = lfn1[di - 1];
    if (w != ' ' && w != '.')
      break;
    di--;
    }
  if (!di)
    return FR_INVALID_NAME;  /* Reject nul string */

  lfn1[di] = 0;            /* LFN is created */

  /* Create SFN in directory form */
  memset(fn, ' ', 11);
  for (si = 0; lfn1[si] == ' ' || lfn1[si] == '.'; si++) ;  /* Strip leading spaces and dots */
  if (si)
    cf |= NS_LOSS | NS_LFN;
  while (di && lfn1[di - 1] != '.')
    di--;  /* Find extension (di<=si: no extension) */

  b = i = 0; ni = 8;
  for (;;) {
    w = lfn1[si++];          /* Get an LFN character */
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
      w = ff_convert (w, 0);   /* Unicode -> OEM code */
      if (w)
        w = ExCvt[w - 0x80]; /* Convert extended character to upper (SBCS) */
#else
      w = ff_convert (ff_wtoupper(w), 0);  /* Upper converted Unicode -> OEM code */
#endif
      cf |= NS_LFN;       /* Force create LFN entry */
      }

    if (!w || strchr ("+,;=[]", w)) { /* Replace illegal characters for SFN */
      w = '_';
      cf |= NS_LOSS | NS_LFN; /* Lossy conversion */
      }
    else {
      if (IsUpper(w)) /* ASCII large capital */
        b |= 2;
      else if (IsLower(w)) { /* ASCII small capital */
        b |= 1;
        w -= 0x20;
        }
      }
    fn[i++] = (BYTE)w;
    }

  if (fn[0] == DDEM)
    fn[0] = RDDEM; /* If the first character collides with deleted mark, replace it with RDDEM */

  if (ni == 8) b <<= 2;
  if ((b & 0x0C) == 0x0C || (b & 0x03) == 0x03) /* Create LFN entry when there are composite capitals */
    cf |= NS_LFN;
  if (!(cf & NS_LFN)) {           /* When LFN is in 8.3 format without extended character, NT flags are created */
    if ((b & 0x03) == 0x01)
      cf |= NS_EXT; /* NT flag (Extension has only small capital) */
    if ((b & 0x0C) == 0x04)
      cf |= NS_BODY;  /* NT flag (Filename has only small capital) */
    }

  fn[NSFLAG] = cf;  /* SFN is created */

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::dir_sdi (UINT idx) {

  // Current index
  index = (WORD)idx;

  // Table start cluster (0:root)
  DWORD clst = sclust;
  if (clst == 1 || clst >= fs->n_fatent)  /* Check start cluster range */
    return FR_INT_ERR;
  if (!clst && fs->fs_type == FS_FAT32) /* Replace cluster# 0 with root cluster# if in FAT32 */
    clst = fs->dirbase;

  DWORD sect1;
  if (clst == 0) {
    // static table (root-directory in FAT12/16)
    if (idx >= fs->n_rootdir) // index out of range
      return FR_INT_ERR;
    sect1 = fs->dirbase;
    }
  else {
    // dynamic table (root-directory in FAT32 or sub-directory)
    UINT ic = _MAX_SS / SZ_DIRE * fs->csize;  // entries per cluster
    while (idx >= ic) {
      // follow cluster chain,  get next cluster
      clst = getFat (fs, clst);
      if (clst == 0xFFFFFFFF)
        return FR_DISK_ERR;
      if (clst < 2 || clst >= fs->n_fatent) // reached end of table or internal error
        return FR_INT_ERR;
      idx -= ic;
      }
    sect1 = clusterToSector(fs, clst);
    }

  clust = clst;
  if (!sect1)
    return FR_INT_ERR;

  // Sector# of the directory entry
  sect = sect1 + idx / (_MAX_SS / SZ_DIRE);

  // Ptr to the entry in the sector
  dir = fs->win.d8 + (idx % (_MAX_SS / SZ_DIRE)) * SZ_DIRE;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::dir_next (int stretch) {

  UINT i = index + 1;
  if (!(i & 0xFFFF) || !sect) /* Report EOT when index has reached 65535 */
    return FR_NO_FILE;

  if (!(i % (_MAX_SS / SZ_DIRE))) {
    sect++;
    if (!clust) {
      /* Static table */
      if (i >= fs->n_rootdir) /* Report EOT if it reached end of static table */
        return FR_NO_FILE;
      }
    else {
      /* Dynamic table */
      if (((i / (_MAX_SS / SZ_DIRE)) & (fs->csize - 1)) == 0) {
        /* Cluster changed? */
        DWORD clst = getFat (fs, clust);        /* Get next cluster */
        if (clst <= 1)
          return FR_INT_ERR;
        if (clst == 0xFFFFFFFF)
          return FR_DISK_ERR;
        if (clst >= fs->n_fatent) {
          /* If it reached end of dynamic table, */
          if (!stretch)
            return FR_NO_FILE;      /* If do not stretch, report EOT */
          clst = createChain (fs, clust);   /* Stretch cluster chain */
          if (clst == 0)
            return FR_DENIED;      /* No free cluster */
          if (clst == 1)
            return FR_INT_ERR;
          if (clst == 0xFFFFFFFF)
            return FR_DISK_ERR;

          /* Clean-up stretched table */
          if (syncWindow(fs))
            return FR_DISK_ERR;/* Flush disk access window */

          /* Clear window buffer */
          memset (fs->win.d8, 0, _MAX_SS);

          /* Cluster start sector */
          fs->winsect = clusterToSector (fs, clst);

          UINT c;
          for (c = 0; c < fs->csize; c++) {
            /* Fill the new cluster with 0 */
            fs->wflag = 1;
            if (syncWindow(fs))
              return FR_DISK_ERR;
            fs->winsect++;
            }
          fs->winsect -= c;           /* Rewind window offset */
          }

        /* Initialize data for new cluster */
        clust = clst;
        sect = clusterToSector (fs, clst);
        }
      }
    }

  /* Current index */
  index = (WORD)i;

  /* Current entry in the window */
  dir = fs->win.d8 + (i % (_MAX_SS / SZ_DIRE)) * SZ_DIRE;

  return FR_OK;
  }
//}}}
//{{{
FRESULT cDirectory::dir_register() {

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
      gen_numname (fn1, sn, lfn1, n);  /* Generate a numbered name */
      res = dir_find();       /* Check if the name collides with existing SFN */
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

  FRESULT res = dir_alloc (nent);    /* Allocate entries */
  if (res == FR_OK && --nent) {
    /* Set LFN entry if needed */
    res = dir_sdi (index - nent);
    if (res == FR_OK) {
      BYTE sum = sum_sfn (fn);  /* Sum value of the SFN tied to the LFN */
      do {
        /* Store LFN entries in bottom first */
        res = moveWindow (fs, sect);
        if (res != FR_OK)
          break;
        fit_lfn (lfn, dir, (BYTE)nent, sum);
        fs->wflag = 1;
        res = dir_next (0);  /* Next entry */
        } while (res == FR_OK && --nent);
      }
    }

  if (res == FR_OK) {
    /* Set SFN entry */
    res = moveWindow (fs, sect);
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
FRESULT cDirectory::dir_alloc (UINT nent) {

  UINT n;
  FRESULT res = dir_sdi (0);
  if (res == FR_OK) {
    n = 0;
    do {
      res = moveWindow (fs, sect);
      if (res != FR_OK)
        break;
      if (dir[0] == DDEM || dir[0] == 0) {
        // free entry
        if (++n == nent)
          break; /* A block of contiguous free entries is found */
        }
      else
        n = 0;          /* Not a blank entry. Restart to search */
      res = dir_next (1);    /* Next entry with table stretch enabled */
      } while (res == FR_OK);
    }

  if (res == FR_NO_FILE)
    res = FR_DENIED; /* No directory entry to allocate */

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::dir_find() {

  BYTE c, *dir1;
  BYTE a, sum;

  FRESULT res = dir_sdi (0);     /* Rewind directory object */
  if (res != FR_OK)
    return res;

  BYTE ord = sum = 0xFF;
  lfn_idx = 0xFFFF; /* Reset LFN sequence */
  do {
    res = moveWindow (fs, sect);
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
          ord = (c == ord && sum == dir1[LDIR_Chksum] && cmp_lfn (lfn, dir1)) ? ord - 1 : 0xFF;
          }
        }
      else {          /* An SFN entry is found */
        if (!ord && sum == sum_sfn (dir1))
          break; /* LFN matched? */
        if (!(fn[NSFLAG] & NS_LOSS) && !memcmp (dir1, fn, 11))
          break;  /* SFN matched? */
        ord = 0xFF;
        lfn_idx = 0xFFFF; /* Reset LFN sequence */
        }
      }

    res = dir_next (0);    /* Next entry */
    } while (res == FR_OK);

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::dir_read (int vol) {

  BYTE a, c, *dir1;
  BYTE ord = 0xFF;
  BYTE sum = 0xFF;

  FRESULT res = FR_NO_FILE;
  while (sect) {
    res = moveWindow (fs, sect);
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
        ord = (c == ord && sum == dir1[LDIR_Chksum] && pick_lfn (lfn, dir1)) ? ord - 1 : 0xFF;
        }
      else {          /* An SFN entry is found */
        if (ord || sum != sum_sfn (dir1)) /* Is there a valid LFN? */
          lfn_idx = 0xFFFF;   /* It has no LFN. */
        break;
        }
      }

    res = dir_next (0);        /* Next entry */
    if (res != FR_OK)
      break;
    }

  if (res != FR_OK)
    sect = 0;

  return res;
  }
//}}}
//{{{
FRESULT cDirectory::dir_remove() {

  // SFN index
  UINT i = index;

  FRESULT res = dir_sdi ((lfn_idx == 0xFFFF) ? i : lfn_idx); /* Goto the SFN or top of the LFN entries */
  if (res == FR_OK) {
    do {
      res = moveWindow (fs, sect);
      if (res != FR_OK)
        break;
      memset (dir, 0, SZ_DIRE); /* Clear and mark the entry "deleted" */

      *dir = DDEM;
      fs->wflag = 1;
      if (index >= i)
        break;  /* When reached SFN, all entries of the object has been deleted. */

      res = dir_next (0);    /* Next entry */
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
#if _LFN_UNICODE
      c = ff_convert (c, 1); // OEM -> Unicode
      if (!c)
        c = '?';
#endif
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
#if !_LFN_UNICODE
        w = ff_convert (w, 0);  // Unicode -> OEM
        if (!w) {
          // No lfn if it could not be converted
          i = 0;
          break;
          }
#endif
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

//{{{
FRESULT f_getcwd (TCHAR* buff, UINT len) {

  cDirectory dj;
  UINT i, n;
  DWORD ccl;
  TCHAR *tp;
  cFileInfo fileInfo;

  *buff = 0;
  /* Get logical drive number */
  FRESULT res = findVolume (&dj.fs, 0); /* Get current volume */
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    i = len;      /* Bottom of buffer (directory stack base) */
    dj.sclust = dj.fs->cdir;      /* Start to follow upper directory from current directory */
    while ((ccl = dj.sclust) != 0) {  /* Repeat while current directory is a sub-directory */
      res = dj.dir_sdi (1);      /* Get parent directory */
      if (res != FR_OK)
        break;

      res = dj.dir_read (0);
      if (res != FR_OK)
        break;

      dj.sclust = loadCluster (dj.fs, dj.dir);  /* Goto parent directory */
      res = dj.dir_sdi (0);
      if (res != FR_OK)
        break;

      do {              /* Find the entry links to the child directory */
        res = dj.dir_read (0);
        if (res != FR_OK)
          break;
        if (ccl == loadCluster (dj.fs, dj.dir))
          break;  /* Found the entry */
        res = dj.dir_next (0);
        } while (res == FR_OK);
      if (res == FR_NO_FILE)
        res = FR_INT_ERR;/* It cannot be 'not found'. */
      if (res != FR_OK)
        break;

      fileInfo.lfname = buff;
      fileInfo.lfsize = i;
      dj.getFileInfo (&fileInfo);    /* Get the directory name and push it to the buffer */
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
//}}}
//{{{
FRESULT f_mkdir (const TCHAR* path) {

  cDirectory dj;
  BYTE *dir, n;
  DWORD dsc, dcl, pcl;

  DWORD tm = get_fattime();

  /* Get logical drive number */
  FRESULT res = findVolume (&dj.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = dj.followPath (path);
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
        res = dj.dir_register();  /* Register the object to the directoy */
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
//}}}
//{{{
FRESULT f_chdir (const TCHAR* path) {

  /* Get logical drive number */
  cDirectory dj;
  FRESULT res = findVolume (&dj.fs, 0);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = dj.followPath (path);

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
//}}}
//{{{
FRESULT f_getfree (const TCHAR* path, DWORD* nclst) {

  DWORD n, clst, sect, stat;
  UINT i;
  BYTE fat, *p;

  /* Get logical drive number */
  FRESULT res = findVolume (FatFs, 0);
  cFatFs* fs = FatFs[0];
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
//}}}
//{{{
FRESULT f_getlabel (const TCHAR* path, TCHAR* label, DWORD* vsn) {

#if _LFN_UNICODE
  WCHAR w;
#endif

  /* Get logical drive number */
  cDirectory dj;
  FRESULT res = findVolume (&dj.fs, 0);

  /* Get volume label */
  if (res == FR_OK && label) {
    dj.sclust = 0;        /* Open root directory */
    res = dj.dir_sdi (0);
    if (res == FR_OK) {
      res = dj.dir_read (1); /* Get an entry with AM_VOL */
      if (res == FR_OK) {
        /* A volume label is exist */
#if _LFN_UNICODE
        UINT i = 0;
        UINT j = 0;
        do {
          w = (i < 11) ? dj.dir[i++] : ' ';
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
//}}}
//{{{
FRESULT f_setlabel (const TCHAR* label) {

  cDirectory dj;
  BYTE vn[11];
  UINT i, j, sl;
  WCHAR w;
  DWORD tm;

  /* Get logical drive number */
  FRESULT res = findVolume (&dj.fs, 1);
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
      w = ff_convert (ff_wtoupper (ff_convert(w, 1)), 0);
#endif
      if (!w || strchr ("\"*+,.:;<=>\?[]|\x7F", w) || j >= (UINT)((w >= 0x100) ? 10 : 11)) /* Reject invalid characters for volume label */
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
  res = dj.dir_sdi (0);
  if (res == FR_OK) {
    res = dj.dir_read (1);   /* Get an entry with AM_VOL */
    if (res == FR_OK) {     /* A volume label is found */
      if (vn[0]) {
        memcpy(dj.dir, vn, 11);  /* Change the volume label name */
        tm = get_fattime();
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
          res = dj.dir_alloc (1);  /* Allocate an entry for volume label */
          if (res == FR_OK) {
            memset(dj.dir, 0, SZ_DIRE);  /* Set volume label */
            memcpy(dj.dir, vn, 11);
            dj.dir[DIR_Attr] = AM_VOL;
            tm = get_fattime();
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
//}}}
//{{{
FRESULT f_unlink (const TCHAR* path) {

  cDirectory dj, sdj;
  BYTE *dir;
  DWORD dclst = 0;

  /* Get logical drive number */
  FRESULT res = findVolume (&dj.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = dj.followPath (path);
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
            memcpy(&sdj, &dj, sizeof (cDirectory)); /* Open the sub-directory */
            sdj.sclust = dclst;
            res = sdj.dir_sdi (2);
            if (res == FR_OK) {
              res = sdj.dir_read (0);      /* Read an item (excluding dot entries) */
              if (res == FR_OK) res = FR_DENIED;  /* Not empty? (cannot remove) */
              if (res == FR_NO_FILE) res = FR_OK; /* Empty? (can remove) */
              }
            }
          }
        }

      if (res == FR_OK) {
        res = dj.dir_remove();    /* Remove the directory entry */
        if (res == FR_OK && dclst)  /* Remove the cluster chain if exist */
          res = removeChain(dj.fs, dclst);
        if (res == FR_OK)
          res = syncFs (dj.fs);
        }
      }
    }

  LEAVE_FF(dj.fs, res);
  }
//}}}
//{{{
FRESULT f_stat (const TCHAR* path, cFileInfo* fileInfo) {

  cDirectory dj;

  /* Get logical drive number */
  FRESULT res = findVolume (&dj.fs, 0);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;

    res = dj.followPath (path);
    if (res == FR_OK) {
      if (dj.dir) {
        /* Found an object */
        if (fileInfo)
          dj.getFileInfo (fileInfo);
        }
      else
        /* It is root directory */
        res = FR_INVALID_NAME;
      }
    }

  LEAVE_FF(dj.fs, res);
  }
//}}}
//{{{
FRESULT f_chmod (const TCHAR* path, BYTE attr, BYTE mask) {

  cDirectory dj;
  BYTE* dir;

  /* Get logical drive number */
  FRESULT res = findVolume (&dj.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = dj.followPath (path);

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
//}}}
//{{{
FRESULT f_rename (const TCHAR* path_old, const TCHAR* path_new) {

  cDirectory djo, djn;
  BYTE buf[21], *dir;
  DWORD dw;

  // get logical drive number of the source object
  FRESULT res = findVolume (&djo.fs, 1);
  if (res == FR_OK) {
    djn.fs = djo.fs;
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    djo.lfn = lfn;
    djo.fn = sfn;
    res = djo.followPath (path_old);
    if (res == FR_OK && (djo.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_OK)
      res = chk_lock(&djo, 2);
    if (res == FR_OK) {
      // old object is found
      if (!djo.dir)  // Is root dir?
        res = FR_NO_FILE;
      else {
        memcpy (buf, djo.dir + DIR_Attr, 21); /* Save information about object except name */
        memcpy (&djn, &djo, sizeof (cDirectory));    /* Duplicate the directory object */
        res = djn.followPath (path_new);  /* and make sure if new object name is not conflicting */
        if (res == FR_OK)
          res = FR_EXIST;   /* The new object name is already existing */
        if (res == FR_NO_FILE) {        /* It is a valid path and no name collision */
          res = djn.dir_register();     /* Register the new entry */
          if (res == FR_OK) {
/* Start of critical section where any interruption can cause a cross-link */
            dir = djn.dir;          /* Copy information about object except name */
            memcpy (dir + 13, buf + 2, 19);
            dir[DIR_Attr] = buf[0] | AM_ARC;
            djo.fs->wflag = 1;
            if ((dir[DIR_Attr] & AM_DIR) && djo.sclust != djn.sclust) {
              /* Update .. entry in the sub-directory if needed */
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
              res = djo.dir_remove();   /* Remove old entry */
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
//}}}
//{{{
FRESULT f_utime (const TCHAR* path, const cFileInfo* fileInfo) {

  cDirectory dj;
  BYTE *dir;

  // Get logical drive number
  FRESULT res = findVolume (&dj.fs, 1);
  if (res == FR_OK) {
    BYTE sfn[12];
    WCHAR lfn [(_MAX_LFN + 1) * 2];
    dj.lfn = lfn;
    dj.fn = sfn;
    res = dj.followPath (path);
    if (res == FR_OK && (dj.fn[NSFLAG] & NS_DOT))
      res = FR_INVALID_NAME;
    if (res == FR_OK) {
      dir = dj.dir;
      if (!dir) // root dir
        res = FR_INVALID_NAME;
      else { // file or subDir
        ST_WORD(dir + DIR_WrtTime, fileInfo->ftime);
        ST_WORD(dir + DIR_WrtDate, fileInfo->fdate);
        dj.fs->wflag = 1;
        res = syncFs (dj.fs);
        }
      }
    }

  LEAVE_FF(dj.fs, res);
  }
//}}}
//{{{
FRESULT f_mkfs (const TCHAR* path, BYTE sfd, UINT au) {

  int vol;
  BYTE fmt, md, sys, *tbl;
  DWORD n_clst, vs, n, wsect;
  UINT i;
  DWORD b_vol, b_fat, b_dir, b_data;  /* LBA */
  DWORD n_vol, n_rsv, n_fat, n_dir; /* Size */
  cFatFs* fs;
  DSTATUS stat;

#if _USE_TRIM
  DWORD eb[2];
#endif

  /* Check mounted drive and clear work area */
  if (sfd > 1)
    return FR_INVALID_PARAMETER;

  vol = 0;
  fs = FatFs[vol];
  if (!fs)
    return FR_NOT_ENABLED;
  fs->fs_type = 0;

  /* Get disk statics */
  stat = disk_initialize (vol);
  if (stat & STA_NOINIT)
    return FR_NOT_READY;
  if (stat & STA_PROTECT)
    return FR_WRITE_PROTECTED;

  /* Create a partition in this function */
  if (disk_ioctl (vol, GET_SECTOR_COUNT, &n_vol) != RES_OK || n_vol < 128)
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

  b_fat = b_vol + n_rsv;           /* FAT area start sector */
  b_dir = b_fat + n_fat * N_FATS;  /* Directory area start sector */
  b_data = b_dir + n_dir;          /* Data area start sector */
  if (n_vol < b_data + au - b_vol)
    return FR_MKFS_ABORTED;  /* Too small volume */

  /* Align data start sector to erase block boundary (for flash memory media) */
  if (disk_ioctl (vol, GET_BLOCK_SIZE, &n) != RES_OK || !n || n > 32768)
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
    memset (fs->win.d8, 0, _MAX_SS);
    tbl = fs->win.d8 + MBR_Table;  /* Create partition table for single partition in the drive */
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
    ST_WORD(fs->win.d8 + BS_55AA, 0xAA55);  /* MBR signature */
    if (disk_write (vol, fs->win.d8, 0, 1) != RES_OK) /* Write it to the MBR */
      return FR_DISK_ERR;
    md = 0xF8;
    }

  /* Create BPB in the VBR */
  tbl = fs->win.d8;             /* Clear sector */
  memset (tbl, 0, _MAX_SS);
  memcpy (tbl, "\xEB\xFE\x90" "MSDOS5.0", 11); /* Boot jump code, OEM name */

  i = _MAX_SS; /* Sector size */
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

  n = get_fattime();                     /* Use current time as VSN */
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
  if (disk_write (vol, tbl, b_vol, 1) != RES_OK)  /* Write it to the VBR sector */
    return FR_DISK_ERR;
  if (fmt == FS_FAT32)          /* Write backup VBR if needed (VBR + 6) */
    disk_write (vol, tbl, b_vol + 6, 1);

  /* Initialize FAT area */
  wsect = b_fat;
  for (i = 0; i < N_FATS; i++) {
    /* Initialize each FAT copy */
    memset (tbl, 0, _MAX_SS);   /* 1st sector of the FAT  */
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

    if (disk_write (vol, tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;

    memset(tbl, 0, _MAX_SS); /* Fill following FAT entries with zero */
    for (n = 1; n < n_fat; n++) {
      /* This loop may take a time on FAT32 volume due to many single sector writes */
      if (disk_write (vol, tbl, wsect++, 1) != RES_OK)
        return FR_DISK_ERR;
      }
    }

  /* Initialize root directory */
  i = (fmt == FS_FAT32) ? au : (UINT)n_dir;
  do {
    if (disk_write (vol, tbl, wsect++, 1) != RES_OK)
      return FR_DISK_ERR;
    } while (--i);

#if _USE_TRIM /* Erase data area if needed */
  {
    eb[0] = wsect; eb[1] = wsect + (n_clst - ((fmt == FS_FAT32) ? 1 : 0)) * au - 1;
    disk_ioctl (vol, CTRL_TRIM, eb);
  }
#endif

  /* Create FSINFO if needed */
  if (fmt == FS_FAT32) {
    ST_DWORD(tbl + FSI_LeadSig, 0x41615252);
    ST_DWORD(tbl + FSI_StrucSig, 0x61417272);
    ST_DWORD(tbl + FSI_Free_Count, n_clst - 1); /* Number of free clusters */
    ST_DWORD(tbl + FSI_Nxt_Free, 2);            /* Last allocated cluster# */
    ST_WORD(tbl + BS_55AA, 0xAA55);
    disk_write (vol, tbl, b_vol + 1, 1);        /* Write original (VBR + 1) */
    disk_write (vol, tbl, b_vol + 7, 1);        /* Write backup (VBR + 7) */
    }

  return (disk_ioctl (vol, CTRL_SYNC, 0) == RES_OK) ? FR_OK : FR_DISK_ERR;
  }
//}}}