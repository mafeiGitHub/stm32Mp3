#pragma once
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}

extern uint32_t SystemCoreClock;
extern const uint8_t AHBPrescTable[16];
extern const uint8_t APBPrescTable[8];

extern void SystemInit();
extern void SystemCoreClockUpdate();

//{{{
#ifdef __cplusplus
}
#endif
//}}}
