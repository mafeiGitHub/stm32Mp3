// ff.h
#pragma once
#include "cmsis_os.h"
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

// chars
#define _LFN_UNICODE 0   /* 0:ANSI/OEM or 1:Unicode */

// Type of path name strings on FatFs API
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
//{{{  defines
#define _MAX_SS      512
#define _MAX_LFN     255  /* Maximum LFN length to handle (12 to 255) */
#define _CODE_PAGE   1252
#define _STRF_ENCODE 3   /* 0:ANSI/OEM, 1:UTF-16LE, 2:UTF-16BE, 3:UTF-8 */

// attribute flag defines
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

class cFatFs;
//{{{
class cFileInfo {
public:
  DWORD fsize;      // File size
  WORD  fdate;      // Last modified date
  WORD  ftime;      // Last modified time
  BYTE  fattrib;    // Attribute
  TCHAR fname[13];  // Short file name (8.3 format)
  TCHAR* lfname;    // Pointer to the LFN buffer
  UINT  lfsize;     // Size of LFN buffer in TCHAR
  };
//}}}
//{{{
class cFile {
public:
  FRESULT f_open (const TCHAR* path, BYTE mode);
  int f_size() { return fsize; }
  FRESULT f_lseek (DWORD ofs);
  FRESULT f_read (void* buff, UINT btr, UINT* br);
  FRESULT f_write (const void* buff, UINT btw, UINT* bw);
  FRESULT f_truncate();
  FRESULT f_sync();
  FRESULT f_close ();

  int f_putc (TCHAR c);
  int f_puts (const TCHAR* str);
  int f_printf (const TCHAR* str, ...);
  TCHAR* f_gets (TCHAR* buff, int len);

private:
  FRESULT validateFile();
  DWORD clmtCluster (DWORD ofs);

  // vars
  union {
    UINT  d32[_MAX_SS/4]; // Force 32bits alignement
    BYTE   d8[_MAX_SS];   // File data read/write buffer
    } buf;

  cFatFs* fs;       // Pointer to the related file system
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
  };
//}}}
//{{{
class cDirectory {
public:
  FRESULT f_opendir (const TCHAR* path);
  FRESULT f_findfirst (cFileInfo* fileInfo, const TCHAR* path, const TCHAR* pattern);
  FRESULT f_readdir (cFileInfo* fileInfo);
  FRESULT f_findnext (cFileInfo* fileInfo);
  FRESULT f_closedir();

  FRESULT followPath (const TCHAR* path);
  FRESULT dir_sdi (UINT idx);
  FRESULT dir_next (int stretch);
  FRESULT dir_read (int vol);
  FRESULT dir_register();
  FRESULT dir_alloc (UINT nent);
  FRESULT dir_remove();
  void getFileInfo (cFileInfo* fileInfo);

  union {
    UINT d32[_MAX_SS/4];  // Force 32bits alignement
    BYTE  d8[_MAX_SS];    // File data read/write buffer
    } buf;

  cFatFs* fs;    // Pointer to the owner file system
  WORD index;    // Current read/write index number

  DWORD sclust;  // Table start cluster (0:Root dir)
  DWORD clust;   // Current cluster
  DWORD sect;    // Current sector

  BYTE* dir;     // Pointer to the current SFN entry in the win[]
  BYTE* fn;      // Pointer to the SFN (in/out) {file[8],ext[3],status[1]}
  WCHAR* lfn;    // Pointer to the LFN working buffer
  WORD lfn_idx;  // Last matched LFN index number (0xFFFF:No LFN)

private:
  FRESULT validateDir();
  FRESULT createName (const TCHAR** path);
  FRESULT dir_find();

  WORD id;       // Owner file system mount ID
  UINT lockid;  // File lock ID (index of file semaphore table Files[])
  const TCHAR* pat;  // Pointer to the name matching pattern
  };
//}}}

FRESULT f_mount();                                                   // Mount fatFs only drive

FRESULT f_getcwd (TCHAR* buff, UINT len);                            // Get current directory
FRESULT f_mkdir (const TCHAR* path);                                 // Create a sub directory
FRESULT f_chdir (const TCHAR* path);                                 // Change current directory
FRESULT f_getfree (const TCHAR* path, DWORD* nclst);                 // Get number of free clusters on the drive
FRESULT f_getlabel (const TCHAR* path, TCHAR* label, DWORD* vsn);    // Get volume label
FRESULT f_setlabel (const TCHAR* label);                             // Set volume label
FRESULT f_unlink (const TCHAR* path);                                // Delete an existing file or directory
FRESULT f_stat (const TCHAR* path, cFileInfo* fileInfo);               // Get file status
FRESULT f_chmod (const TCHAR* path, BYTE attr, BYTE mask);           // Change attribute of the file/dir
FRESULT f_rename (const TCHAR* path_old, const TCHAR* path_new);     // Rename/Move a file or directory
FRESULT f_utime (const TCHAR* path, const cFileInfo* fileInfo);        // Change times-tamp of the file/dir
FRESULT f_chdrive (const TCHAR* path);                               // Change current drive
FRESULT f_mkfs (const TCHAR* path, BYTE sfd, UINT au);               // Create a file system on the volume
