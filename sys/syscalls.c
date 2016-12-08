// syscalls.c Support files for GNU libc
/*{{{  includes*/
#include <_ansi.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>
#include <reent.h>
#include <unistd.h>
#include <sys/wait.h>

#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"
/*}}}*/

#define FreeRTOS
#define MAX_STACK_SIZE 0x2000

#ifndef FreeRTOS
  register char* stack_ptr asm("sp");
#endif

extern UART_HandleTypeDef DebugUartHandle;

/*{{{*/
caddr_t _sbrk (int incr) {

  extern char end asm("end");
  static char* heap_end;

  char* prev_heap_end;
  char* min_stack_ptr;

  if (heap_end == 0)
    heap_end = &end;

  prev_heap_end = heap_end;

#ifdef FreeRTOS
  /* Use the NVIC offset register to locate the main stack pointer. */
  min_stack_ptr = (char*)(*(unsigned int *)*(unsigned int *)0xE000ED08);
  /* Locate the STACK bottom address */
  min_stack_ptr -= MAX_STACK_SIZE;

  if (heap_end + incr > min_stack_ptr)
#else
  if (heap_end + incr > stack_ptr)
#endif
  {
    //write(1, "Heap and stack collision\n", 25);
    //abort();
    errno = ENOMEM;
    return (caddr_t) -1;
    }

  heap_end += incr;

  return (caddr_t) prev_heap_end;
  }
/*}}}*/

/*{{{*/
int _gettimeofday (struct timeval * tp, struct timezone * tzp) {
  /* Return fixed data for the timezone.  */

  if (tzp) {
    tzp->tz_minuteswest = 0;
    tzp->tz_dsttime = 0;
    }

  return 0;
  }
/*}}}*/
void initialise_monitor_handles() {}

/*{{{*/
int _read (int file, char *ptr, int len) {

  //for (int DataIdx = 0; DataIdx < len; DataIdx++)
  //  *ptr++ = __io_getchar();
  return len;
  }
/*}}}*/
/*{{{*/
int _write (int file, char *ptr, int len) {

  for (auto i = 0; i < len ; i++) {
    ITM_SendChar ((*ptr) & 0xff);
    HAL_UART_Transmit (&DebugUartHandle, ptr, 1, 0xFFFF);
    ptr++;
    }

  return len;
  }
/*}}}*/

int _open (char *path, int flags, ...) { return -1; }
int _fstat (int file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty (int file) { return 1; }
int _lseek (int file, int ptr, int dir) { return 0; }
int _stat (char *file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _wait (int *status) { errno = ECHILD; return -1; }
int _close (int file) { return -1; }

int _unlink (char *name) { errno = ENOENT; return -1; }
int _times (struct tms *buf) { return -1; }
int _link (char *old, char *new) { errno = EMLINK; return -1; }

int _getpid() { return 1; }
int _kill (int pid, int sig) { errno = EINVAL; return -1; }
void _exit (int status) { _kill(status, -1); while (1) {} }

int _fork() { errno = EAGAIN; return -1; }
int _execve (char* name, char** argv, char** env) { errno = ENOMEM; return -1; }
