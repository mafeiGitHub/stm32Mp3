// fatFs.h
#pragma once
//{{{  includes, defines
#include <string>
#include "cmsis_os.h"

class cFile;
class cDirectory;

// 8 bit type
typedef unsigned char   BYTE;

// 16 bit type
typedef short           SHORT;
typedef unsigned short  WORD;
typedef unsigned short  WCHAR;

// 16 bit or 32 bit type
typedef int             INT;
typedef unsigned int    UINT;

// 32 bit type
typedef long            LONG;
typedef unsigned long   DWORD;

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
  FR_INVALID_PARAMETER,   // 19  Given parameter is invalid
  FR_UNUSED,              // 20  initialised but unused
  } FRESULT;
//}}}

//{{{
class cFileInfo {
public:
  cFileInfo() : mLongFileNameSize(MAX_LFN + 1) {}

  std::string getName() { return mLongFileName[0] ? mLongFileName : (char*)&mShortFileName; }
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
  //{{{  public vars
  DWORD mFileSize;  // File size
  WORD  mDate;      // Last modified date
  WORD  mTime;      // Last modified time
  BYTE  mAttribute; // Attribute

  char  mShortFileName[13]; // 8.3 format
  UINT  mLongFileNameSize;
  char  mLongFileName[MAX_LFN + 1];
  //}}}
  };
//}}}

class cFatFs {
public:
  //{{{  singleton create, instance, constructor
  static cFatFs* create();
  //{{{
  static cFatFs* instance() {
    return mFatFs;
    }
  //}}}
  cFatFs();
  //}}}
  //{{{  gets
  bool isOk() { return mResult == FR_OK; }
  FRESULT getResult() { return mResult; }
  std::string getLabel() { return mLabel; }
  int getVolumeSerialNumber() { return mVolumeSerialNumber; }
  int getFreeSectors() { return mFreeClusters * mSectorsPerCluster; }
  //}}}
  FRESULT mount();
  FRESULT getCwd (char* buff, UINT len);                        // Get current directory
  FRESULT setLabel (const char* label);                         // Set volume label
  FRESULT mkDir (const char* path);                             // Create a sub directory
  FRESULT chDir (const char* path);                             // Change current directory
  FRESULT rename (const char* path_old, const char* path_new);  // Rename/Move a file or directory
  FRESULT chMod (const char* path, BYTE attr, BYTE mask);       // Change attribute of the file/dir
  FRESULT stat (const char* path, cFileInfo& fileInfo);         // Get file status
  FRESULT utime (const char* path, const cFileInfo& fileInfo);  // Change timestamp of the file/dir
  FRESULT unlink (const char* path);                            // Delete an existing file or directory
  FRESULT mkfs (BYTE sfd, UINT au);                             // Create a file system on the volume
//{{{  private
friend class cFile;
friend class cDirectory;

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
  FRESULT checkFileLock (cDirectory* directory, int acc);
  UINT incFileLock (cDirectory* directory, int acc);
  FRESULT decFileLock (UINT i);
  void clearFileLock();

  bool lock();
  void unlock (FRESULT result);

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
  BYTE* mWindowBuffer = nullptr; // Disk access window for Directory, FAT
  osSemaphoreId mSemaphore;      // Identifier of sync object
  FRESULT mResult = FR_UNUSED;   // Pointer to the related file system

  char  mLabel[13];
  int   mVolumeSerialNumber = 0;
  BYTE  mFsType = 0;             // FAT sub-type (0:Not mounted)
  WORD  mMountId = 0;            // File system mount ID

  BYTE  mSectorsPerCluster = 0;  // Sectors per cluster (1,2,4...128)
  BYTE  mNumFatCopies = 0;       // Number of FAT copies (1 or 2)
  BYTE  mWindowFlag = 0;         // win[] flag (b0:dirty)
  BYTE  mFsiFlag = 0;            // FSINFO flags (b7:disabled, b0:dirty)

  WORD  mNumRootdir = 0;         // Number of root directory entries (FAT12/16)
  DWORD mLastCluster = 0;        // Last allocated cluster
  DWORD mFreeClusters = 0;       // Number of free clusters
  DWORD mCurDirSector = 0;       // Current directory start cluster (0:root)
  DWORD mNumFatEntries = 0;      // Number of FAT entries, = number of clusters + 2
  DWORD mSectorsPerFAT = 0;      // Sectors per FAT

  DWORD mVolBase = 0;            // Volume start sector
  DWORD mFatBase = 0;            // FAT start sector
  DWORD mDirBase = 0;            // Root directory start sector (FAT32:Cluster#)
  DWORD mDataBase = 0;           // Data start sector
  DWORD mWindowSector = 0;       // Current sector appearing in the win[]
//}}}
  };

class cDirectory {
public:
  //{{{  constructor, destructor
  cDirectory() {}
  cDirectory (std::string path);
  ~cDirectory();
  //}}}
  //{{{  gets
  bool isOk() { return mResult == FR_OK; }
  FRESULT getResult() { return mResult; }
  //}}}
  FRESULT read (cFileInfo& fileInfo);
  FRESULT findFirst (cFileInfo& fileInfo, const char* path, const char* pattern);
  FRESULT findNext (cFileInfo& fileInfo);
//{{{  private
friend class cFatFs;
friend class cFile;

private:
  bool validate();
  bool followPath (const char* path);
  FRESULT createName (const char** path);

  FRESULT find();
  FRESULT setIndex (UINT index);
  FRESULT next (int stretch);
  FRESULT read (int vol);
  FRESULT registerNewEntry();
  FRESULT allocate (UINT nent);
  FRESULT remove();
  void getFileInfo (cFileInfo& fileInfo);

  FRESULT mResult = FR_UNUSED;       // Pointer to the related file system
  cFatFs* mFs = 0;                   // pointer to the owner fileSystem
  WORD mMountId = 0;                 // owner fileSystem mountId
  UINT mLockId = 0;                  // file lockId (index of file semaphore table Files[])

  WORD mIndex = 0;                   // Current read/write index number
  DWORD mStartCluster = 0;           // Table start cluster (0:Root dir)
  DWORD mCluster = 0;                // Current cluster
  DWORD mSector = 0;                 // Current sector

  BYTE* mDirShortFileName = nullptr; // pointer to the current SFN entry in the win[]
  BYTE mShortFileName[12];           // shortFileName - file[8],ext[3],status[1]
  WCHAR* mLongFileName = nullptr;    // pointer to longFileName working buffer
  WORD mLongFileNameIndex = 0;       // Last matched longFileName index number (0xFFFF:No longFileName)

  const char* mPattern = nullptr;    // pointer to the name matching pattern
//}}}
  };

class cFile {
public:
  //{{{  constructor, destructor
  cFile (std::string path, BYTE mode);
  ~cFile();
  //}}}
  //{{{  gets
  bool isOk() { return mResult == FR_OK; }
  FRESULT getResult() { return mResult; }

  int getPosition() { return mPosition; }
  int getSize() { return mFileSize; }
  //}}}
  FRESULT read (void* readBuffer, int bytestoRead, int& bytesRead);
  FRESULT write (const void* buff, UINT btw, UINT* bw);
  FRESULT seek (DWORD position);
  FRESULT truncate();
  FRESULT sync();
  //{{{  stream
  int putCh (char c);
  int putStr (const char* str);
  int printf (const char* str, ...);
  char* gets (char* buff, int len);
  //}}}
 //{{{  private
 friend class cFatFs;

 private:
   bool validate();
   DWORD clmtCluster (DWORD ofs);

   // vars
   BYTE* fileBuffer = nullptr;  // File data read/write buffer
   FRESULT mResult = FR_UNUSED; // Pointer to the related file system

   cFatFs* mFs = 0;             // Pointer to the related file system
   WORD mMountId = 0;           // Owner file system mount ID
   UINT mLockId = 0;            // File lock ID origin from 1 (index of file semaphore table Files[])

   BYTE mFlag = 0;              // Status flags

   DWORD mPosition = 0;         // File read/write pointer (Zeroed on file open)
   DWORD mFileSize = 0;         // File size

   DWORD mStartCluster = 0;     // File start cluster (0:no cluster chain, always 0 when fsize is 0)
   DWORD mCluster= 0;           // Current cluster of fpter (not valid when fprt is 0)
   DWORD mCachedSector = 0;     // Sector number appearing in buf[] (0:invalid)

   DWORD mDirSectorNum = 0;     // Sector number containing the directory entry
   BYTE* mDirPtr = 0;           // Pointer to the directory entry in the win[]

   DWORD* mClusterTable = nullptr; // Pointer to the cluster link map table (Nulled on file open)
 //}}}
  };

