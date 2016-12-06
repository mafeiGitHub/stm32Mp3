// lwipopts.h
#pragma once

#define LWIP_SOCKET   0 // wouldn't compile

#define LWIP_NETCONN  1
#define LWIP_DHCP     1
#define LWIP_TCP      1
#define LWIP_DNS      1
//#define LWIP_ICMP     1
//#define LWIP_UDP      1
#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_NETIF_HOSTNAME  1
#define LWIP_STATS           0
#define LWIP_PROVIDE_ERRNO   1
#define LWIP_NETIF_LINK_CALLBACK  1 // 1 = support callback function from an interface
#define LWIP_SO_RCVTIMEO     1
//#define LWIP_DEBUG           1
//#define TCP_DEBUG            LWIP_DBG_ON
//#define ETHARP_DEBUG         LWIP_DBG_ON
//#define PBUF_DEBUG           LWIP_DBG_ON
//#define IP_DEBUG             LWIP_DBG_ON
//#define TCPIP_DEBUG          LWIP_DBG_ON
//#define DHCP_DEBUG           LWIP_DBG_ON
//#define UDP_DEBUG            LWIP_DBG_ON
//#define ICMP_DEBUG           LWIP_DBG_ON
//#define IGMP_DEBUG           LWIP_DBG_ON

// TCP options
#define TCP_TTL           255
#define TCP_QUEUE_OOSEQ   0            // TCP queues segments that arrive out of order, 0 low memory
#define TCP_MSS           (1500 - 40)  // TCP_MSS = (Ethernet MTU - IP header size - TCP header size)

#define TCP_SND_BUF       (4*TCP_MSS)  // TCP sender buffer space (bytes)

#define TCP_SND_QUEUELEN  (2* TCP_SND_BUF/TCP_MSS) // TCP sender buffer space (pbufs) must be (2 * TCP_SND_BUF/TCP_MSS) for things to work
#define TCP_WND           (10*TCP_MSS) // TCP receive window

#define PBUF_POOL_SIZE    10           // the number of buffers in the pbuf pool
#define PBUF_POOL_BUFSIZE 1524         // the size of each pbuf in the pbuf pool

// UDP options
#define UDP_TTL           255

#define SYS_LIGHTWEIGHT_PROT    0 // = 1 for inter-task protection for certain critical regions
#define ETHARP_TRUST_IP_MAC     0
#define IP_REASSEMBLY           0
#define IP_FRAG                 0
#define ARP_QUEUEING            0
#define NO_SYS                  0 // = 1 provides VERY minimal functionality

#define MEM_ALIGNMENT           4
#define MEM_SIZE                (5*1024) // size of heap memory, high if lot data copied
#define MEMP_NUM_PBUF           100      // number of memp struct pbufs. high if lot of data out of ROM
#define MEMP_NUM_UDP_PCB        6        // number of UDP protocol control blocks. One per active UDP "connection"
#define MEMP_NUM_TCP_PCB        10       // number of simulatenously active TCP connections
#define MEMP_NUM_TCP_PCB_LISTEN 5        // number of listening TCP connections
#define MEMP_NUM_TCP_SEG        12       // number of simultaneously queued TCP segments
#define MEMP_NUM_SYS_TIMEOUT    10       // number of simulateously active timeouts

#define CHECKSUM_GEN_IP     0
#define CHECKSUM_GEN_UDP    0
#define CHECKSUM_GEN_TCP    0
#define CHECKSUM_CHECK_IP   0
#define CHECKSUM_CHECK_UDP  0
#define CHECKSUM_CHECK_TCP  0
#define CHECKSUM_GEN_ICMP   0

// OS options
#define TCPIP_THREAD_NAME          "TCP/IP"
#define TCPIP_THREAD_STACKSIZE     1000
#define TCPIP_MBOX_SIZE            5
#define DEFAULT_UDP_RECVMBOX_SIZE  2000
#define DEFAULT_TCP_RECVMBOX_SIZE  2000
#define DEFAULT_ACCEPTMBOX_SIZE    2000
#define DEFAULT_THREAD_STACKSIZE   500
#define TCPIP_THREAD_PRIO          (configMAX_PRIORITIES - 2)
#define LWIP_COMPAT_MUTEX          1
