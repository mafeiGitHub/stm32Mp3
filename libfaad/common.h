#pragma once

#include <stddef.h>
#include <string.h>
#include "neaacdec.h"

#ifdef __cplusplus
  extern "C" {
#endif

typedef unsigned long long uint64_t;
typedef signed long long int64_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef signed long int32_t;
typedef signed short int16_t;
typedef signed char int8_t;
typedef float float32_t;
char *strchr(), *strrchr();

#define INLINE __inline
#define ALIGN

uint32_t get_sample_rate (const uint8_t sr_index);

void* faad_malloc (size_t size);
void faad_free (void* b);

//#define FIXED_POINT
#define ERROR_RESILIENCE
#define MAIN_DEC
#define LTP_DEC
#define SBR_DEC
#define PS_DEC
#define LD_DEC
#define ALLOW_SMALL_FRAMELENGTH
//#define BIG_IQ_TABLE
//#define SSR_DEC
//#define DRM
//#define DRM_PS
//#define SBR_LOW_POWER
//#define LC_ONLY_DECODER
//{{{  option define interlocks
#ifdef LD_DEC
  #ifndef ERROR_RESILIENCE
    #define ERROR_RESILIENCE
  #endif
  #ifndef LTP_DEC
    #define LTP_DEC
  #endif
#endif

#ifdef LC_ONLY_DECODER
  #undef LD_DEC
  #undef LTP_DEC
  #undef MAIN_DEC
  #undef SSR_DEC
  #undef DRM
  #undef ALLOW_SMALL_FRAMELENGTH
  #undef ERROR_RESILIENCE
#endif

#ifdef SBR_LOW_POWER
  #undef PS_DEC
#endif

#ifdef FIXED_POINT
  #undef MAIN_DEC
  #undef SSR_DEC
#endif

#ifdef DRM
  #ifndef ALLOW_SMALL_FRAMELENGTH
    #define ALLOW_SMALL_FRAMELENGTH
  #endif
  #undef LD_DEC
  #undef LTP_DEC
  #undef MAIN_DEC
  #undef SSR_DEC
#endif

#ifndef SBR_LOW_POWER
  #define qmf_t complex_t
  #define QMF_RE(A) RE(A)
  #define QMF_IM(A) IM(A)
#else
  #define qmf_t real_t
  #define QMF_RE(A) (A)
  #define QMF_IM(A)
#endif

#ifdef WORDS_BIGENDIAN
  #define ARCH_IS_BIG_ENDIAN
#endif
//}}}

#ifdef FIXED_POINT
  #include "fixed.h"

  #define LOG2_MIN_INF REAL_CONST(-10000)
  #define DIV_R(A, B) (((int64_t)A << REAL_BITS)/B)
  #define DIV_C(A, B) (((int64_t)A << COEF_BITS)/B)

  int32_t log2_int(uint32_t val);
  int32_t log2_fix(uint32_t val);
  int32_t pow2_int(real_t val);
  real_t pow2_fix(real_t val);
#else
  #include <math.h>

  typedef float real_t;

  #define DIV_R(A, B) ((A)/(B))
  #define DIV_C(A, B) ((A)/(B))
  #define MUL_R(A,B)  ((A)*(B))
  #define MUL_C(A,B)  ((A)*(B))
  #define MUL_F(A,B)  ((A)*(B))

  #define REAL_CONST(A) ((real_t)(A))
  #define COEF_CONST(A) ((real_t)(A))
  #define Q2_CONST(A)   ((real_t)(A))
  #define FRAC_CONST(A) ((real_t)(A))
  //{{{
  /* Complex multiplication */
  static INLINE void ComplexMult (real_t *y1, real_t *y2,
                                  real_t x1, real_t x2, real_t c1, real_t c2) {
    *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
    *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
    }
  //}}}

  #define M_PI 3.14159265358979323846
  #define M_PI_2 1.57079632679489661923
#endif

typedef real_t complex_t[2];
typedef real_t complex_t[2];

#define RE(A) A[0]
#define IM(A) A[1]

#define lrintf(f) ((int32_t)(f))

#ifndef max
  #define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
  #define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef __cplusplus
  }
#endif
