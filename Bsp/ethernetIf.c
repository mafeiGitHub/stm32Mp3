// ethernetif.c
/*{{{  includes*/
#include <string.h>
#include "os/ethernetif.h"

#include "memory.h"
#include "cmsis_os.h"

#include "stm32f7xx_hal.h"

#include "lwip/opt.h"
#include "lwip/lwip_timers.h"
#include "netif/etharp.h"
/*}}}*/
/*{{{  defines*/
#define LAN8742A_PHY_ADDRESS  0x00

// MAC ADDRESS - MAC_ADDR0:MAC_ADDR1:MAC_ADDR2:MAC_ADDR3:MAC_ADDR4:MAC_ADDR5
#define MAC_ADDR0   2
#define MAC_ADDR1   0
#define MAC_ADDR2   0x11
#define MAC_ADDR3   0x22
#define MAC_ADDR4   0x33
#define MAC_ADDR5   0x44

#define TIME_WAITING_FOR_INPUT       ( 100 )
#define INTERFACE_THREAD_STACK_SIZE  ( 350 )
/*}}}*/

// vars
ETH_HandleTypeDef EthHandle;
osSemaphoreId s_xSemaphore = NULL;

/*{{{*/
static err_t low_level_output (struct netif* netif, struct pbuf* p) {
// This function should do the actual transmission of the packet. The packet is
//  contained in the pbuf that is passed to the function. This pbuf  might be chained.
//  @param netif the lwip network interface structure for this ethernetif
//  @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
//  @return ERR_OK if the packet could be sent
//          an err_t value if the packet couldn't be sent
//  @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
//        strange results. You might consider waiting for space in the DMA queue
//        to become available since the stack doesn't retry to send a packet
//        dropped because of memory failure (except for the TCP timers).

  err_t errval;

  struct pbuf* q;
  uint8_t* buffer = (uint8_t *)(EthHandle.TxDesc->Buffer1Addr);

  __IO ETH_DMADescTypeDef* DmaTxDesc;
  uint32_t framelength = 0;
  uint32_t bufferoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t payloadoffset = 0;

  DmaTxDesc = EthHandle.TxDesc;
  bufferoffset = 0;

  /* copy frame from pbufs to driver buffers */
  for (q = p; q != NULL; q = q->next) {
    /* Is this buffer available? If not, goto error */
    if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
      errval = ERR_USE;
      goto error;
      }

    /* Get bytes in current lwIP buffer */
    byteslefttocopy = q->len;
    payloadoffset = 0;

    /* Check if the length of data to copy is bigger than Tx buffer size*/
    while ((byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE) {
      /* Copy data to Tx buffer*/
      memcpy ((uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), (ETH_TX_BUF_SIZE - bufferoffset) );

      /* Point to next descriptor */
      DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);

      /* Check if the buffer is available */
      if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
        errval = ERR_USE;
        goto error;
        }

      buffer = (uint8_t*)(DmaTxDesc->Buffer1Addr);

      byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - bufferoffset);
      payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - bufferoffset);
      framelength = framelength + (ETH_TX_BUF_SIZE - bufferoffset);
      bufferoffset = 0;
      }

    /* Copy the remaining bytes */
    memcpy ((uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), byteslefttocopy );
    bufferoffset = bufferoffset + byteslefttocopy;
    framelength = framelength + byteslefttocopy;
    }

  /* Prepare transmit descriptors to give to DMA */
  HAL_ETH_TransmitFrame (&EthHandle, framelength);
  errval = ERR_OK;

error:
  /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
  if ((EthHandle.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET) {
    /* Clear TUS ETHERNET DMA flag */
    EthHandle.Instance->DMASR = ETH_DMASR_TUS;

    /* Resume DMA transmission*/
    EthHandle.Instance->DMATPDR = 0;
    }

  return errval;
  }
/*}}}*/
/*{{{*/
static struct pbuf* low_level_input (struct netif* netif) {
// Should allocate a pbuf and transfer the bytes of the incoming
// packet from the interface into the pbuf.
// netif the lwip network interface structure for this ethernetif
// return a pbuf filled with the received packet (including MAC header)  NULL on memory error

  struct pbuf *p = NULL, *q = NULL;
  uint16_t len = 0;
  uint8_t *buffer;
  __IO ETH_DMADescTypeDef *dmarxdesc;
  uint32_t bufferoffset = 0;
  uint32_t payloadoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t i=0;

  /* get received frame */
  if (HAL_ETH_GetReceivedFrame_IT(&EthHandle) != HAL_OK)
    return NULL;

  /* Obtain the size of the packet and put it into the "len" variable. */
  len = EthHandle.RxFrameInfos.length;
  buffer = (uint8_t*)EthHandle.RxFrameInfos.buffer;

  if (len > 0)
    /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
    p = pbuf_alloc (PBUF_RAW, len, PBUF_POOL);

  if (p != NULL) {
    dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
    bufferoffset = 0;

    for (q = p; q != NULL; q = q->next) {
      byteslefttocopy = q->len;
      payloadoffset = 0;

      /* Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size */
      while ((byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE) {
        /* Copy data to pbuf */
        memcpy ((uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), (ETH_RX_BUF_SIZE - bufferoffset));

        /* Point to next descriptor */
        dmarxdesc = (ETH_DMADescTypeDef*)(dmarxdesc->Buffer2NextDescAddr);
        buffer = (uint8_t*)(dmarxdesc->Buffer1Addr);

        byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
        payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
        bufferoffset = 0;
        }

      /* Copy remaining data in pbuf */
      memcpy ((uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), byteslefttocopy);
      bufferoffset = bufferoffset + byteslefttocopy;
      }
    }

  /* Release descriptors to DMA, Point to first descriptor */
  dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
  /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
  for (i = 0; i< EthHandle.RxFrameInfos.SegCount; i++) {
    dmarxdesc->Status |= ETH_DMARXDESC_OWN;
    dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
    }

  /* Clear Segment_Count */
  EthHandle.RxFrameInfos.SegCount = 0;

  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((EthHandle.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET) {
    /* Clear RBUS ETHERNET DMA flag */
    EthHandle.Instance->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    EthHandle.Instance->DMARPDR = 0;
    }

  return p;
  }
/*}}}*/
/*{{{*/
static void ethernetif_input (void const* argument ) {
// This function is the ethernetif_input task, it is processed when a packet
// is ready to be read from the interface. It uses the function low_level_input()
// that should handle the actual reception of bytes from the network
// interface. Then the type of the received packet is determined and
// the appropriate input function is called.
// netif the lwip network interface structure for this ethernetif

  struct netif* netif = (struct netif*)argument;

  for (;;) {
    if (osSemaphoreWait (s_xSemaphore, TIME_WAITING_FOR_INPUT) == osOK) {
      struct pbuf* p;
      do {
        p = low_level_input (netif);
        if (p != NULL) {
          if (netif->input (p, netif) != ERR_OK )
            pbuf_free (p);
          }
        } while (p != NULL);
      }
    }
  }
/*}}}*/
/*{{{*/
static void low_level_init (struct netif* netif) {

  uint8_t macaddress[6]= { MAC_ADDR0, MAC_ADDR1, MAC_ADDR2, MAC_ADDR3, MAC_ADDR4, MAC_ADDR5 };

  EthHandle.Instance = ETH;
  EthHandle.Init.MACAddr = macaddress;
  EthHandle.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
  EthHandle.Init.Speed = ETH_SPEED_100M;
  EthHandle.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
  EthHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
  EthHandle.Init.RxMode = ETH_RXINTERRUPT_MODE;
  EthHandle.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  EthHandle.Init.PhyAddress = LAN8742A_PHY_ADDRESS;
  if (HAL_ETH_Init (&EthHandle) == HAL_OK)
    // Set netif link flag
    netif->flags |= NETIF_FLAG_LINK_UP;

  // Initialize Rx Descriptors list: Chain Mode
  HAL_ETH_DMARxDescListInit (&EthHandle, (ETH_DMADescTypeDef*)EthRxDescripSection, (uint8_t*)EthRxBUF, ETH_RXBUFNB);

  // Initialize Tx Descriptors list: Chain Mode
  HAL_ETH_DMATxDescListInit (&EthHandle, (ETH_DMADescTypeDef*)EthTxDescripSection, (uint8_t*)EthTxBUF, ETH_TXBUFNB);

  // set netif MAC hardware address length
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  // set netif MAC hardware address
  netif->hwaddr[0] = MAC_ADDR0;
  netif->hwaddr[1] = MAC_ADDR1;
  netif->hwaddr[2] = MAC_ADDR2;
  netif->hwaddr[3] = MAC_ADDR3;
  netif->hwaddr[4] = MAC_ADDR4;
  netif->hwaddr[5] = MAC_ADDR5;

  // set netif maximum transfer unit
  netif->mtu = 1500;

  // Accept broadcast address and ARP traffic
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  // create a binary semaphore used for informing ethernetif of frame reception
  osSemaphoreDef (SEM);
  s_xSemaphore = osSemaphoreCreate (osSemaphore(SEM), 1);

  // create the task that handles the ETH_MAC
  osThreadDef (EthIf, ethernetif_input, osPriorityRealtime, 0, INTERFACE_THREAD_STACK_SIZE);
  osThreadCreate (osThread(EthIf), netif);

  // Enable MAC and DMA transmission and reception
  HAL_ETH_Start (&EthHandle);
  }
/*}}}*/

/*{{{*/
err_t ethernetif_init (struct netif* netif) {

  LWIP_ASSERT ("netif != NULL", (netif != NULL));

  netif->hostname = "lwip";
  netif->name[0] = 's';
  netif->name[1] = 't';
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  low_level_init (netif);

  return ERR_OK;
  }
/*}}}*/

/*{{{*/
void HAL_ETH_MspInit (ETH_HandleTypeDef* heth) {
// Ethernet pins config
//   RMII_REF_CLK ----------------------> PA1
//   RMII_MDIO -------------------------> PA2
//   RMII_MDC --------------------------> PC1
//   RMII_MII_CRS_DV -------------------> PA7
//   RMII_MII_RXD0 ---------------------> PC4
//   RMII_MII_RXD1 ---------------------> PC5
//   RMII_MII_RXER ---------------------> PG2
//   RMII_MII_TX_EN --------------------> PG11
//   RMII_MII_TXD0 ---------------------> PG13
//   RMII_MII_TXD1 ---------------------> PG14

  // Enable GPIOs clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  // Configure PA1, PA2 and PA7
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStructure.Pull = GPIO_NOPULL;
  GPIO_InitStructure.Alternate = GPIO_AF11_ETH;
  GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
  HAL_GPIO_Init (GPIOA, &GPIO_InitStructure);

  // Configure PC1, PC4 and PC5
  GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
  HAL_GPIO_Init (GPIOC, &GPIO_InitStructure);

  // Configure PG2, PG11, PG13 and PG14
  GPIO_InitStructure.Pin =  GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
  HAL_GPIO_Init (GPIOG, &GPIO_InitStructure);

  // Enable the Ethernet global Interrupt
  HAL_NVIC_SetPriority (ETH_IRQn, 0x7, 0);
  HAL_NVIC_EnableIRQ (ETH_IRQn);

  // Enable ETHERNET clock
  __HAL_RCC_ETH_CLK_ENABLE();
  }
/*}}}*/
/*{{{*/
void HAL_ETH_RxCpltCallback (ETH_HandleTypeDef* heth) {

  osSemaphoreRelease (s_xSemaphore);
  }
/*}}}*/
