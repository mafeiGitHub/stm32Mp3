#pragma once
//{{{
#ifdef __cplusplus
extern "C" {
#endif
//}}}

#define JD_SZBUF    512 // size of stream input buffer

//{{{  integer.h
/* These types must be 16-bit, 32-bit or larger integer */
typedef int INT;
typedef unsigned int UINT;

/* These types must be 8-bit integer */
typedef char CHAR;
typedef unsigned char UCHAR;
typedef unsigned char BYTE;

/* These types must be 16-bit integer */
typedef short SHORT;
typedef unsigned short USHORT;
typedef unsigned short WORD;
typedef unsigned short WCHAR;

/* These types must be 32-bit integer */
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
// endof integer.h
//}}}
//{{{  JRESULT
typedef enum {
  JDR_OK = 0, /* 0: Succeeded */
  JDR_INTR,   /* 1: Interrupted by output function */
  JDR_INP,    /* 2: Device error or wrong termination of input stream */
  JDR_MEM1,   /* 3: Insufficient memory pool for the image */
  JDR_MEM2,   /* 4: Insufficient stream input buffer */
  JDR_PAR,    /* 5: Parameter error */
  JDR_FMT1,   /* 6: Data format error (may be damaged data) */
  JDR_FMT2,   /* 7: Right format but not supported */
  JDR_FMT3    /* 8: Not supported JPEG standard */
  } JRESULT;
//}}}
//{{{  JRECT
typedef struct {
  WORD left, right, top, bottom;
  } JRECT;
//}}}
//{{{  JDEC
typedef struct JDEC JDEC;
struct JDEC {
  UINT dctr;        /* Number of bytes available in the input buffer */
  BYTE* dptr;       /* Current data read ptr */
  BYTE* inbuf;      /* Bit stream input buffer */
  BYTE dmsk;        /* Current bit in the current read byte */
  BYTE scale;       /* Output scaling ratio */
  BYTE msx, msy;      /* MCU size in unit of block (width, height) */
  BYTE qtid[3];     /* Quantization table ID of each component */
  SHORT dcv[3];     /* Previous DC element of each component */
  WORD nrst;        /* Restart inverval */
  UINT width, height;   /* Size of the input image (pixel) */
  BYTE* huffbits[2][2]; /* Huffman bit distribution tables [id][dcac] */
  WORD* huffcode[2][2]; /* Huffman code word tables [id][dcac] */
  BYTE* huffdata[2][2]; /* Huffman decoded data tables [id][dcac] */
  LONG* qttbl[4];     /* Dequaitizer tables [id] */
  void* workbuf;      /* Working buffer for IDCT and RGB output */
  BYTE* mcubuf;     /* Working buffer for the MCU */
  void* pool;       /* Pointer to available memory pool */
  UINT sz_pool;     /* Size of momory pool (bytes available) */
  UINT (*infunc)(JDEC*, BYTE*, UINT);/* Pointer to jpeg stream input function */
  void* device;     /* Pointer to I/O device identifiler for the session */
  };
//}}}

JRESULT jd_prepare (JDEC*, UINT(*)(JDEC*, BYTE*, UINT), void*, UINT, void*);
JRESULT jd_decomp (JDEC*, UINT(*)(JDEC*, BYTE*, JRECT*), BYTE);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
