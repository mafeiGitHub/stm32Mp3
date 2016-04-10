// ff.h
#pragma once
#include "cmsis_os.h"
//{{{
#ifdef __cplusplus
  extern "C" {
#endif
//}}}
//{{{  integer.h typedefs
// 8 bit
typedef unsigned char   BYTE;

// 16 bit
typedef short           SHORT;
typedef unsigned short  WORD;
typedef unsigned short  WCHAR;

// 16 bit or 32 bit
typedef int             INT;
typedef unsigned int    UINT;

// 32 bit
typedef long            LONG;
typedef unsigned long   DWORD;
//}}}
//{{{  ff_conf defines
#define _MAX_SS      512
#define _MAX_LFN     255  /* Maximum LFN length to handle (12 to 255) */
#define _CODE_PAGE   1252
#define _LFN_UNICODE 0   /* 0:ANSI/OEM or 1:Unicode */
#define _STRF_ENCODE 3   /* 0:ANSI/OEM, 1:UTF-16LE, 2:UTF-16BE, 3:UTF-8 */
//}}}
//{{{  Type of path name strings on FatFs API
#if _LFN_UNICODE
  /* Unicode string */
  #ifndef _INC_TCHAR
    typedef WCHAR TCHAR;
    #define _T(x) L ## x
    #define _TEXT(x) L ## x
  #endif

#else
  /* ANSI/OEM string */
  #ifndef _INC_TCHAR
    typedef char TCHAR;
    #define _T(x) x
    #define _TEXT(x) x
  #endif

#endif
//}}}
//{{{  attribute flags
#define FA_OPEN_EXISTING  0x00
#define FA_READ           0x01
#define FA_WRITE          0x02
#define FA_CREATE_NEW     0x04
#define FA_CREATE_ALWAYS  0x08
#define FA_OPEN_ALWAYS    0x10
#define FA__WRITTEN       0x20
#define FA__DIRTY         0x40

// File attribute bits for directory entry
#define AM_RDO  0x01  // Read only
#define AM_HID  0x02  // Hidden
#define AM_SYS  0x04  // System
#define AM_VOL  0x08  // Volume label
#define AM_LFN  0x0F  // LFN entry
#define AM_DIR  0x10  // Directory
#define AM_ARC  0x20  // Archive
#define AM_MASK 0x3F  // Mask of defined bits

// Fast seek feature
#define CREATE_LINKMAP  0xFFFFFFFF
//}}}

//{{{  enum FRESULT
typedef enum {
  FR_OK = 0,              //  0  Succeeded
  FR_DISK_ERR,            //  1  A hard error occurred in the low level disk I/O layer
  FR_INT_ERR,             //  2  Assertion failed
  FR_NOT_READY,           //  3  The physical drive cannot work
  FR_NO_FILE,             //  4  Could not find the file
  FR_NO_PATH,             //  5  Could not find the path
  FR_INVALID_NAME,        //  6  The path name format is invalid
  FR_DENIED,              //  7  Access denied due to prohibited access or directory full
  FR_EXIST,               //  8  Access denied due to prohibited access
  FR_INVALID_OBJECT,      //  9  The file/directory object is invalid
  FR_WRITE_PROTECTED,     // 10  The physical drive is write protected
  FR_INVALID_DRIVE,       // 11  The logical drive number is invalid
  FR_NOT_ENABLED,         // 12  The volume has no work area
  FR_NO_FILESYSTEM,       // 13  There is no valid FAT volume
  FR_MKFS_ABORTED,        // 14  The f_mkfs() aborted due to any parameter error
  FR_TIMEOUT,             // 15  Could not get a grant to access the volume within defined period
  FR_LOCKED,              // 16  The operation is rejected according to the file sharing policy
  FR_NOT_ENOUGH_CORE,     // 17  LFN working buffer could not be allocated
  FR_TOO_MANY_OPEN_FILES, // 18  Number of open files > _FS_SHARE
  FR_INVALID_PARAMETER    // 19  Given parameter is invalid
  } FRESULT;
//}}}
//{{{  struct FATFS fileSystem
typedef struct {
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
  } FATFS;
//}}}
//{{{  struct FIL file
typedef struct {
  union {
    UINT  d32[_MAX_SS/4]; // Force 32bits alignement
    BYTE   d8[_MAX_SS];   // File data read/write buffer
    } buf;

  FATFS* fs;       // Pointer to the related file system
  WORD id;         // Owner file system mount ID
  BYTE flag;       // Status flags
  BYTE err;        // Abort flag (error code)

  DWORD fptr;      // File read/write pointer (Zeroed on file open)
  DWORD fsize;     // File size
  DWORD sclust;    // File start cluster (0:no cluster chain, always 0 when fsize is 0)
  DWORD clust;     // Current cluster of fpter (not valid when fprt is 0)
  DWORD dsect;     // Sector number appearing in buf[] (0:invalid)

  DWORD dir_sect;  // Sector number containing the directory entry
  BYTE* dir_ptr;   // Pointer to the directory entry in the win[]

  DWORD* cltbl;    // Pointer to the cluster link map table (Nulled on file open)
  UINT lockid;     // File lock ID origin from 1 (index of file semaphore table Files[])
  } FIL;
//}}}
//{{{  struct DIR directory
typedef struct {
  union {
    UINT d32[_MAX_SS/4];  // Force 32bits alignement
    BYTE  d8[_MAX_SS];    // File data read/write buffer
    } buf;

  FATFS* fs;     // Pointer to the owner file system
  WORD id;       // Owner file system mount ID
  WORD index;    // Current read/write index number

  DWORD sclust;  // Table start cluster (0:Root dir)
  DWORD clust;   // Current cluster
  DWORD sect;    // Current sector

  BYTE* dir;     // Pointer to the current SFN entry in the win[]
  BYTE* fn;      // Pointer to the SFN (in/out) {file[8],ext[3],status[1]}
  WCHAR* lfn;    // Pointer to the LFN working buffer
  WORD lfn_idx;  // Last matched LFN index number (0xFFFF:No LFN)

  UINT  lockid;  // File lock ID (index of file semaphore table Files[])
  const TCHAR* pat;  // Pointer to the name matching pattern
  } DIR;
//}}}
//{{{  struct FILINFO fileInformation
typedef struct {
  DWORD fsize;      // File size
  WORD  fdate;      // Last modified date
  WORD  ftime;      // Last modified time
  BYTE  fattrib;    // Attribute
  TCHAR fname[13];  // Short file name (8.3 format)
  TCHAR* lfname;    // Pointer to the LFN buffer
  UINT  lfsize;     // Size of LFN buffer in TCHAR
  } FILINFO;
//}}}

FRESULT f_mount ();                                                  // Mount/Unmount a logical drive
FRESULT f_open (FIL* file, const TCHAR* path, BYTE mode);            // Open or create a file
FRESULT f_read (FIL* file, void* buff, UINT btr, UINT* br);          // Read data from a file
FRESULT f_write (FIL* file, const void* buff, UINT btw, UINT* bw);   // Write data to a file
FRESULT f_sync (FIL* file);                                          // Flush cached data of a writing file
FRESULT f_close (FIL* file);                                         // Close an open file object
FRESULT f_lseek (FIL* file, DWORD ofs);                              // Move file pointer of a file object
FRESULT f_truncate (FIL* file);                                      // Truncate file

FRESULT f_opendir (DIR* dir, const TCHAR* path);                     // Open a directory
FRESULT f_closedir (DIR* dir);                                       // Close an open directory
FRESULT f_readdir (DIR* dir, FILINFO* fileInfo);                     // Read a directory item
FRESULT f_findfirst (DIR* dir, FILINFO* fileInfo, const TCHAR* path, const TCHAR* pattern); // Find first file
FRESULT f_findnext (DIR* dir, FILINFO* fileInfo);                    // Find next file

FRESULT f_getcwd (TCHAR* buff, UINT len);                            // Get current directory
FRESULT f_mkdir (const TCHAR* path);                                 // Create a sub directory
FRESULT f_chdir (const TCHAR* path);                                 // Change current directory

FRESULT f_getfree (const TCHAR* path, DWORD* nclst);                 // Get number of free clusters on the drive
FRESULT f_getlabel (const TCHAR* path, TCHAR* label, DWORD* vsn);    // Get volume label
FRESULT f_setlabel (const TCHAR* label);                             // Set volume label
FRESULT f_unlink (const TCHAR* path);                                // Delete an existing file or directory
FRESULT f_stat (const TCHAR* path, FILINFO* fileInfo);               // Get file status
FRESULT f_chmod (const TCHAR* path, BYTE attr, BYTE mask);           // Change attribute of the file/dir
FRESULT f_rename (const TCHAR* path_old, const TCHAR* path_new);     // Rename/Move a file or directory
FRESULT f_utime (const TCHAR* path, const FILINFO* fileInfo);        // Change times-tamp of the file/dir

FRESULT f_chdrive (const TCHAR* path);                               // Change current drive

FRESULT f_mkfs (const TCHAR* path, BYTE sfd, UINT au);               // Create a file system on the volume
FRESULT f_fdisk (BYTE pdrv, const DWORD szt[], void* work);          // Divide a physical drive into some partitions */

int f_putc (TCHAR c, FIL* fp);                    // Put a character to the file
int f_puts (const TCHAR* str, FIL* cp);           // Put a string to the file
int f_printf (FIL* file, const TCHAR* str, ...);    // Put a formatted string to the file
TCHAR* f_gets (TCHAR* buff, int len, FIL* fp);    // Get a string from the file

#define f_eof(fp) ((int)((fp)->fptr == (fp)->fsize))
#define f_error(fp) ((fp)->err)
#define f_tell(fp) ((fp)->fptr)
#define f_size(fp) ((fp)->fsize)
#define f_rewind(fp) f_lseek((fp), 0)
#define f_rewinddir(dp) f_readdir((dp), 0)

#ifndef EOF
  #define EOF (-1)
#endif

//{{{
#ifdef __cplusplus
  }
#endif
//}}}
