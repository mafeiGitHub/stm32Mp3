// cpuUsage.c
/*{{{  includes*/
#include "cpuUsage.h"

#include "cmsis_os.h"
/*}}}*/

// const
#define CALCULATION_PERIOD 1000

// vars
static xTaskHandle xIdleHandle = NULL;
static uint32_t osCPU_IdleStartTime = 0;
static uint32_t osCPU_IdleSpentTime = 0;
static uint32_t osCPU_TotalIdleTime = 0;

static int tick = 0;
static volatile uint32_t osCPU_Usage = 0;

/*{{{*/
void vApplicationIdleHook() {

  if (xIdleHandle == NULL)
    // Store the handle to the idle task
    xIdleHandle = xTaskGetCurrentTaskHandle();
  }
/*}}}*/
/*{{{*/
void vApplicationTickHook() {

  if (tick++ > CALCULATION_PERIOD) {
    tick = 0;

    if (osCPU_TotalIdleTime > 1000)
      osCPU_TotalIdleTime = 1000;

    osCPU_Usage = (osCPU_TotalIdleTime * 100) / CALCULATION_PERIOD;
    osCPU_TotalIdleTime = 0;
    }
  }
/*}}}*/

/*{{{*/
void StartIdleMonitor() {

  if (xTaskGetCurrentTaskHandle() == xIdleHandle)
    osCPU_IdleStartTime = xTaskGetTickCountFromISR();
  }
/*}}}*/
/*{{{*/
void EndIdleMonitor() {

  if (xTaskGetCurrentTaskHandle() == xIdleHandle) {
    osCPU_IdleSpentTime = xTaskGetTickCountFromISR() - osCPU_IdleStartTime;
    osCPU_TotalIdleTime += osCPU_IdleSpentTime;
    }
  }
/*}}}*/

/*{{{*/
unsigned short osGetCPUUsage() {

  return (unsigned short)osCPU_Usage;
  }
/*}}}*/
