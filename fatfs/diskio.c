#include "diskio.h"
#include "ff_gen_drv.h"

extern Disk_drvTypeDef disk;
/*{{{*/
__weak DWORD get_fattime (void) {
	return 0;
	}
/*}}}*/

/*{{{*/
DSTATUS disk_status ( BYTE pdrv ) {
	return disk.drv[pdrv]->disk_status (disk.lun[pdrv]);
	}
/*}}}*/
/*{{{*/
DSTATUS disk_initialize (BYTE pdrv) {
	DSTATUS stat = RES_OK;
	if (disk.is_initialized[pdrv] == 0) {
		disk.is_initialized[pdrv] = 1;
		stat = disk.drv[pdrv]->disk_initialize (disk.lun[pdrv]);
		}
	return stat;
	}
/*}}}*/

#if _USE_IOCTL == 1
	/*{{{*/
	DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
		return disk.drv[pdrv]->disk_ioctl (disk.lun[pdrv], cmd, buff);
		}
	/*}}}*/
#endif

/*{{{*/
DRESULT disk_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count ) {
	return disk.drv[pdrv]->disk_read (disk.lun[pdrv], buff, sector, count);
	}

/*}}}*/
#if _USE_WRITE == 1
	/*{{{*/
	DRESULT disk_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count ) {
		return disk.drv[pdrv]->disk_write (disk.lun[pdrv], buff, sector, count);
		}
	/*}}}*/
#endif
