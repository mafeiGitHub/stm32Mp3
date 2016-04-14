// fatFs.h
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
typedef char TCHAR;
#define _T(x) x
#define _TEXT(x) x
//}}}
//{{{  defines
#define FS_LOCK         2  // 0:Disable or >=1:Enable
#define SECTOR_SIZE   512
#define MAX_LFN       255  // Maximum LFN length to handle (12 to 255)

// attribute flag defines
#define FA_OPEN_EXISTING  0x00
#define FA_READ           0x01
#define FA_WRITE          0x02
#define FA_CREATE_NEW     0x04
#define FA_CREATE_ALWAYS  0x08
#define FA_OPEN_ALWAYS    0x10
#define FA__WRITTEN       0x20
#define FA__DIRTY         0x40

// Fast seek feature
#define CREATE_LINKMAP  0xFFFFFFFF

// File attribute bits for directory entry
#define AM_RDO  0x01  // Read only
#define AM_HID  0x02  // Hidden
#define AM_SYS  0x04  // System
#define AM_VOL  0x08  // Volume label
#define AM_LFN  0x0F  // LFN entry
#define AM_DIR  0x10  // Directory
#define AM_ARC  0x20  // Archive
#define AM_MASK 0x3F  // Mask of defined bits
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

class cFile;
class cDirectory;
//{{{
class cFileInfo {
public:
  cFileInfo() : mLongFileNameSize(MAX_LFN + 1) {}

  char* getName() { return mLongFileName[0] ? mLongFileName : (char*)&mShortFileName; }
  bool getEmpty() { return mShortFileName[0] == 0; }
  bool getBack() { return mShortFileName[0] == '.'; }
  bool isDirectory() { return mAttribute & AM_DIR; }
  //{{{
  bool matchExtension (const char* extension) {
    auto i = 0;
    while ((i < 9) && mShortFileName[i++] != '.');
    return (mShortFileName[i] == extension[0]) && (mShortFileName[i+1] == extension[1]) && (mShortFileName[i+2] == extension[2]);
    }
  //}}}

  // public vars
  DWORD mFileSize;  // File size
  WORD  mDate;      // Last modified date
  WORD  mTime;      // Last modified time
  BYTE  mAttribute; // Attribute

  TCHAR mShortFileName[13]; // 8.3 format
  UINT  mLongFileNameSize;
  TCHAR mLongFileName[MAX_LFN + 1];
  };
//}}}
//{{{
class cFatFs {
friend class cFile;
friend class cDirectory;
public :
  cFatFs();
  //{{{
  static cFatFs* instance() {
    if (!mFatFs)
      mFatFs = new cFatFs();
    return mFatFs;
    }
  //}}}

  FRESULT getLabel (TCHAR* label, DWORD* vsn);                   // Get volume label
  FRESULT getFree (DWORD* numClusters, DWORD* clusterSize);      // Get number of free clusters on the drive
  FRESULT getCwd (TCHAR* buff, UINT len);                        // Get current directory
  FRESULT setLabel (const TCHAR* label);                         // Set volume label
  FRESULT mkDir (const TCHAR* path);                             // Create a sub directory
  FRESULT chDir (const TCHAR* path);                             // Change current directory
  FRESULT stat (const TCHAR* path, cFileInfo* fileInfo);         // Get file status
  FRESULT rename (const TCHAR* path_old, const TCHAR* path_new); // Rename/Move a file or directory
  FRESULT chMod (const TCHAR* path, BYTE attr, BYTE mask);       // Change attribute of the file/dir
  FRESULT utime (const TCHAR* path, const cFileInfo* fileInfo);  // Change timestamp of the file/dir
  FRESULT unlink (const TCHAR* path);                            // Delete an existing file or directory
  FRESULT mkfs (const TCHAR* path, BYTE sfd, UINT au);           // Create a file system on the volume

private:
  FRESULT findVolume (cFatFs** fs, BYTE wmode);

  FRESULT syncWindow();
  FRESULT moveWindow (DWORD sector);

  BYTE checkFs (DWORD sect);
  FRESULT syncFs();

  DWORD getFat (DWORD cluster);
  FRESULT putFat (DWORD cluster, DWORD val);

  DWORD clusterToSector (DWORD cluster);
  DWORD loadCluster (BYTE* dir);

  DWORD createChain (DWORD cluster);
  FRESULT removeChain (DWORD cluster);

  int enquireFileLock();
  FRESULT checkFileLock (cDirectory* dp, int acc);
  UINT incFileLock (cDirectory* dp, int acc);
  FRESULT decFileLock (UINT i);
  void clearFileLock();

  bool lock();
  void unlock (FRESULT res);

  // static vars
  static cFatFs* mFatFs;

  //{{{
  class cFileSem {
  public:
    cFatFs* mFs; // Object ID 1, volume (NULL:blank entry)
    DWORD clu;  // Object ID 2, directory (0:root)
    WORD idx;   // Object ID 3, directory index
    WORD ctr;   // Object open counter, 0:none, 0x01..0xFF:read mode open count, 0x100:write mode
    };
  //}}}
  static cFileSem mFiles[FS_LOCK];

  // private vars
  BYTE* mWindowBuffer;  // Disk access window for Directory, FAT
  osSemaphoreId mSemaphore; // Identifier of sync object

  BYTE  mFsType = 0;    // FAT sub-type (0:Not mounted)
  WORD  mMountId = 0;   // File system mount ID

  BYTE  mSectorsPerCluster; // Sectors per cluster (1,2,4...128)
  BYTE  mNumFatCopies;  // Number of FAT copies (1 or 2)
  BYTE  mWindowFlag;    // win[] flag (b0:dirty)
  BYTE  mFsiFlag;       // FSINFO flags (b7:disabled, b0:dirty)

  WORD  mNumRootdir;    // Number of root directory entries (FAT12/16)
  DWORD mLastCluster;   // Last allocated cluster
  DWORD mFreeClusters;  // Number of free clusters
  DWORD mCurDirSector;  // Current directory start cluster (0:root)
  DWORD mNumFatEntries; // Number of FAT entries, = number of clusters + 2
  DWORD mSectorsPerFAT; // Sectors per FAT

  DWORD mVolBase;       // Volume start sector
  DWORD mFatBase;       // FAT start sector
  DWORD mDirBase;       // Root directory start sector (FAT32:Cluster#)
  DWORD mDataBase;      // Data start sector
  DWORD mWindowSector;  // Current sector appearing in the win[]
  };
//}}}
//{{{
class cFile {
friend class cFatFs;
public:
  cFile();
  ~cFile();
  FRESULT open (const TCHAR* path, BYTE mode);
  int size() { return mFileSize; }
  FRESULT lseek (DWORD fileOffset);
  FRESULT read (void* readBuffer, UINT bytestoRead, UINT* bytesRead);
  FRESULT write (const void* buff, UINT btw, UINT* bw);
  FRESULT truncate();
  FRESULT sync();
  FRESULT close ();

  int putCh (TCHAR c);
  int putStr (const TCHAR* str);
  int printf (const TCHAR* str, ...);
  TCHAR* gets (TCHAR* buff, int len);

private:
  FRESULT validateFile();
  DWORD clmtCluster (DWORD ofs);

  // vars
  BYTE* fileBuffer = nullptr;  // File data read/write buffer

  cFatFs* mFs;         // Pointer to the related file system
  WORD mMountId;       // Owner file system mount ID
  UINT mLockId;        // File lock ID origin from 1 (index of file semaphore table Files[])

  BYTE mFlag;          // Status flags
  BYTE mError;         // Abort flag (error code)

  DWORD mFilePtr;      // File read/write pointer (Zeroed on file open)
  DWORD mFileSize;     // File size

  DWORD mStartCluster; // File start cluster (0:no cluster chain, always 0 when fsize is 0)
  DWORD mCluster;      // Current cluster of fpter (not valid when fprt is 0)
  DWORD mCachedSector; // Sector number appearing in buf[] (0:invalid)

  DWORD mDirSectorNum; // Sector number containing the directory entry
  BYTE* mDirPtr;       // Pointer to the directory entry in the win[]

  DWORD* mClusterTable; // Pointer to the cluster link map table (Nulled on file open)
  };
//}}}
//{{{
class cDirectory {
friend class cFatFs;
friend class cFile;
public:
  FRESULT open (const TCHAR* path);
  FRESULT read (cFileInfo* fileInfo);
  FRESULT findfirst (cFileInfo* fileInfo, const TCHAR* path, const TCHAR* pattern);
  FRESULT findnext (cFileInfo* fileInfo);
  FRESULT close();

private:
  FRESULT validateDir();
  FRESULT createName (const TCHAR** path);
  FRESULT find();
  FRESULT followPath (const TCHAR* path);
  FRESULT setIndex (UINT index);
  FRESULT next (int stretch);
  FRESULT read (int vol);
  FRESULT registerNewEntry();
  FRESULT allocate (UINT nent);
  FRESULT remove();
  void getFileInfo (cFileInfo* fileInfo);

  cFatFs* mFs;             // pointer to the owner fileSystem
  WORD mMountId;           // owner fileSystem mountId
  UINT mLockId;            // file lockId (index of file semaphore table Files[])

  WORD mIndex;             // Current read/write index number
  DWORD mStartCluster;     // Table start cluster (0:Root dir)
  DWORD mCluster;          // Current cluster
  DWORD mSector;           // Current sector

  BYTE* mDirShortFileName; // pointer to the current SFN entry in the win[]
  BYTE mShortFileName[12]; // shortFileName - file[8],ext[3],status[1]
  WCHAR* mLongFileName;    // pointer to longFileName working buffer
  WORD mLongFileNameIndex; // Last matched longFileName index number (0xFFFF:No longFileName)

  const TCHAR* mPattern;   // pointer to the name matching pattern
  };
//}}}
