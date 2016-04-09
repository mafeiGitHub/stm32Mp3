// ccsbcs.h
#pragma once

//{{{
#ifdef __cplusplus
  extern "C" {
#endif
//}}}

WCHAR ff_convert (WCHAR chr, UINT dir);  // OEM-Unicode bidirectional conversion
WCHAR ff_wtoupper (WCHAR chr);           // Unicode upper-case conversion

//{{{
#ifdef __cplusplus
  }
#endif
//}}}
