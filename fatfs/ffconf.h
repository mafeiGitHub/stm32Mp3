// FatFs - FAT file system module configuration file  R0.11 (C)ChaN, 2015
#pragma once
#define _FFCONF 32020 /* Revision ID */
#include "cmsis_os.h"
#include "stm32f7xx_hal.h"

#define _FS_TINY             0      /* 0:Normal or 1:Tiny */
#define _FS_READONLY         0      /* 0:Read/Write or 1:Read only */

#define _FS_MINIMIZE         0      /* 0 to 3 */
/* The _FS_MINIMIZE option defines minimization level to remove some functions.
/   0: Full function.
/   1: f_stat, f_getfree, f_unlink, f_mkdir, f_chmod, f_truncate, f_utime and f_rename are removed.
/   2: f_opendir and f_readdir are removed in addition to 1.
/   3: f_lseek is removed in addition to 2. */

#define _USE_STRFUNC         2      // 0:Disable or 1-2:Enable - To enable string functions, set _USE_STRFUNC to 1 or 2
#define _USE_FIND            0      // f_findfirst() and f_findnext(). (0:Disable or 1:Enable)
#define _USE_MKFS            1      // 0:Disable or 1:Enable and _FS_READONLY to 0
#define _USE_FASTSEEK        1      // 0:Disable or 1:Enable fast seek feature
#define _USE_LABEL           0      // 0:Disable or 1:Enable volume label functions, set _USE_LAVEL to 1
#define _USE_FORWARD         0      // 0:Disable or 1:Enable f_forward function, set _USE_FORWARD to 1 and set _FS_TINY to 1
#define _CODE_PAGE         1252

#define _USE_LFN     3  /* 0 to 3 */
#define _MAX_LFN     255  /* Maximum LFN length to handle (12 to 255) */
/* The _USE_LFN option switches the LFN feature.
/   0: Disable LFN feature. _MAX_LFN has no effect.
/   1: Enable LFN with static working buffer on the BSS. Always NOT reentrant.
/   2: Enable LFN with dynamic working buffer on the STACK.
/   3: Enable LFN with dynamic working buffer on the HEAP.
/  To enable LFN feature, Unicode handling functions ff_convert() and ff_wtoupper()
/  function must be added to the project.
/  The LFN working buffer occupies (_MAX_LFN + 1) * 2 bytes. When use stack for the
/  working buffer, take care on stack overflow. When use heap memory for the working
/  buffer, memory management functions, ff_memalloc() and ff_memfree(), must be added
/  to the project. */

#define _LFN_UNICODE    0 /* 0:ANSI/OEM or 1:Unicode */
#define _STRF_ENCODE    3 /* 0:ANSI/OEM, 1:UTF-16LE, 2:UTF-16BE, 3:UTF-8 */

#define _FS_RPATH       0 /* 0 to 2 */
//   0: Disable relative path feature and remove related functions.
//   1: Enable relative path. f_chdrive() and f_chdir() function are available.
//   2: f_getcwd() function is available in addition to 1.
//  Note that output of the f_readdir() fnction is affected by this option

#define _VOLUMES        1    // Number of volumes (logical drives) to be used

#define _MULTI_PARTITION  0 /* 0:Single partition, 1:Enable multiple partition */
/* When set to 0, each volume is bound to the same physical drive number and
/ it can mount only first primaly partition. When it is set to 1, each volume
/ is tied to the partitions listed in VolToPart[]. */

#define _MIN_SS                 512
#define _MAX_SS                 512
/* These options configure the range of sector size to be supported. (512, 1024, 2048 or
/  4096) Always set both 512 for most systems, all memory card and harddisk. But a larger
/  value may be required for on-board flash memory and some type of optical media.
/  When _MAX_SS is larger than _MIN_SS, FatFs is configured to variable sector size and
/  GET_SECTOR_SIZE command must be implemented to the disk_ioctl() function. */

#define _USE_TRIM                0
/* This option switches ATA-TRIM feature. (0:Disable or 1:Enable)
/  To enable Trim feature, also CTRL_TRIM command should be implemented to the disk_ioctl() function. */

#define _FS_NOFSINFO    0 /* 0 or 1 */
// If you need to know the correct free space on the FAT32 volume, set this
// option to 1 and f_getfree() function at first time after volume mount will force a full FAT scan.
// 0: Load all informations in the FSINFO if available.
// 1: Do not trust free cluster count in the FSINFO

#define _FS_NORTC 0
#define _NORTC_MON  2
#define _NORTC_MDAY 1
#define _NORTC_YEAR 2015
/* The _FS_NORTC option switches timestamp feature. If the system does not have
/  an RTC function or valid timestamp is not needed, set _FS_NORTC to 1 to disable
/  the timestamp feature. All objects modified by FatFs will have a fixed timestamp
/  defined by _NORTC_MON, _NORTC_MDAY and _NORTC_YEAR.
/  When timestamp feature is enabled (_FS_NORTC == 0), get_fattime() function need
/  to be added to the project to read current time form RTC. _NORTC_MON,
/  _NORTC_MDAY and _NORTC_YEAR have no effect.
/  These options have no effect at read-only configuration (_FS_READONLY == 1). */

#define _WORD_ACCESS    0 /* 0 or 1 */
/* The _WORD_ACCESS option is an only platform dependent option. It defines
/  which access method is used to the word data on the FAT volume.
/   0: Byte-by-byte access. Always compatible with all platforms.
/   1: Word access. Do not choose this unless under both the following conditions.
/  * Byte order on the memory is little-endian.
/  * Address miss-aligned word access is always allowed for all instructions.
/  If it is the case, _WORD_ACCESS can also be set to 1 to improve performance
/  and reduce code size */

#define _FS_REENTRANT    1  /* 0:Disable or 1:Enable */
#define _FS_TIMEOUT      1000 /* Timeout period in unit of time ticks */
#define _SYNC_t          osSemaphoreId /* O/S dependent type of sync object. e.g. HANDLE, OS_EVENT*, ID and etc.. */
/* The _FS_REENTRANT option switches the re-entrancy (thread safe) of the FatFs module.
/   0: Disable re-entrancy. _SYNC_t and _FS_TIMEOUT have no effect.
/   1: Enable re-entrancy. Also user provided synchronization handlers,
/      ff_req_grant(), ff_rel_grant(), ff_del_syncobj() and ff_cre_syncobj()
/      function must be added to the project. */

#define _FS_LOCK    2      /* 0:Disable or >=1:Enable */
/* To enable file lock control feature, set _FS_LOCK to 1 or greater.
   The value defines how many files can be opened simultaneously. */
