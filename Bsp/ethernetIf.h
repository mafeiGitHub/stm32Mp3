#pragma once
//{{{
#ifdef __cplusplus
extern "C" {
#endif
//}}}

#include "lwip/err.h"
#include "lwip/netif.h"

err_t ethernetif_init (struct netif* netif);

void ETHERNET_IRQHandler();

//{{{
#ifdef __cplusplus
}
#endif
//}}}
