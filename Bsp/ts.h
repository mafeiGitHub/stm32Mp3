//  ts.h version V4.0.1  21-July-2015
#pragma once
#include <stdint.h>
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}

typedef struct {
  void       (*Init)(uint16_t);
  uint16_t   (*ReadID)(uint16_t);
  void       (*Reset)(uint16_t);
  void       (*Start)(uint16_t);
  uint8_t    (*DetectTouch)(uint16_t);
  void       (*GetXY)(uint16_t, uint16_t*, uint16_t*);
  void       (*EnableIT)(uint16_t);
  void       (*ClearIT)(uint16_t);
  uint8_t    (*GetITStatus)(uint16_t);
  void       (*DisableIT)(uint16_t);
  } TS_DrvTypeDef;

//{{{
#ifdef __cplusplus
  }
#endif
//}}}
