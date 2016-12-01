// ethernetif.c
/*{{{  includes*/
#include <string.h>
#include "os/ethernetif.h"

#include "memory.h"
#include "stm32f7xx_hal.h"

#include "lwip/opt.h"
#include "lwip/lwip_timers.h"
#include "netif/etharp.h"
/*}}}*/
static const uint8_t kMacAddress[6] = { 2, 0, 0x11, 0x22, 0x33, 0x44 };

// vars
ETH_HandleTypeDef EthHandle;
static SemaphoreHandle_t mRxSem = NULL;

/*{{{*/
static err_t ethernetOutput (struct netif* netif, struct pbuf* p) {
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

  uint8_t* buffer = (uint8_t *)(EthHandle.TxDesc->Buffer1Addr);
  __IO ETH_DMADescTypeDef* DmaTxDesc = EthHandle.TxDesc;

  /* copy frame from pbufs to driver buffers */
  uint32_t framelength = 0;
  uint32_t bufferoffset = 0;
  struct pbuf* q;
  for (q = p; q != NULL; q = q->next) {
    /* Is this buffer available? If not, goto error */
    if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
      errval = ERR_USE;
      goto error;
      }

    /* Get bytes in current lwIP buffer */
    uint32_t byteslefttocopy = q->len;
    uint32_t payloadoffset = 0;

    /* Check if the length of data to copy is bigger than Tx buffer size*/
    while ((byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE) {
      /* Copy data to Tx buffer*/
      memcpy (buffer + bufferoffset, q->payload + payloadoffset, (ETH_TX_BUF_SIZE - bufferoffset) );

      /* Point to next descriptor, Check if the buffer is available */
      DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
      if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
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
    memcpy (buffer + bufferoffset, q->payload + payloadoffset, byteslefttocopy );
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
static struct pbuf* ethernetInput (struct netif* netif) {
// allocate and return a pbuf filled with the received packet (including MAC header)
// - NULL on memory error

  if (HAL_ETH_GetReceivedFrame_IT (&EthHandle) != HAL_OK)
    return NULL;

  // allocate chain of pbufs from the Lwip buffer pool
  uint8_t* buffer = (uint8_t*)EthHandle.RxFrameInfos.buffer;
  uint16_t len = EthHandle.RxFrameInfos.length;
  struct pbuf* bufHead = len > 0 ? pbuf_alloc (PBUF_RAW, len, PBUF_POOL) : NULL;

  __IO ETH_DMADescTypeDef* dmarxdesc;
  if (bufHead) {
    dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
    uint32_t bufferoffset = 0;
    for (struct pbuf* curBuf = bufHead; curBuf != NULL; curBuf = curBuf->next) {
      // Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size
      uint32_t byteslefttocopy = curBuf->len;
      uint32_t payloadoffset = 0;
      while ((byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE) {
        // Copy data to pbuf
        memcpy (curBuf->payload + payloadoffset, buffer + bufferoffset, (ETH_RX_BUF_SIZE - bufferoffset));

        // Point to next descriptor
        dmarxdesc = (ETH_DMADescTypeDef*)(dmarxdesc->Buffer2NextDescAddr);
        buffer = (uint8_t*)(dmarxdesc->Buffer1Addr);
        byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
        payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
        bufferoffset = 0;
        }

      // Copy remaining data in pbuf
      memcpy (curBuf->payload + payloadoffset, buffer + bufferoffset, byteslefttocopy);
      bufferoffset = bufferoffset + byteslefttocopy;
      }
    }

  // Release descriptors to DMA, Point to first descriptor, Set Own bit in Rx descriptors: gives the buffers back to DMA
  dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
  for (uint32_t i = 0; i< EthHandle.RxFrameInfos.SegCount; i++) {
    dmarxdesc->Status |= ETH_DMARXDESC_OWN;
    dmarxdesc = (ETH_DMADescTypeDef*)(dmarxdesc->Buffer2NextDescAddr);
    }

  // Clear Segment_Count, When Rx Buffer unavailable flag is set: clear it and resume reception
  EthHandle.RxFrameInfos.SegCount = 0;
  if ((EthHandle.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET) {
    // Clear RBUS ETHERNET DMA flag
    EthHandle.Instance->DMASR = ETH_DMASR_RBUS;
    // Resume DMA reception
    EthHandle.Instance->DMARPDR = 0;
    }

  return bufHead;
  }
/*}}}*/

/*{{{*/
static void ethernetInputThread (void const* argument) {

  struct netif *netif = (struct netif*)argument;

  while (1) {
    if (xSemaphoreTake (mRxSem, 100) == pdTRUE) {
      struct pbuf* buf;
      do {
        buf = ethernetInput (netif);
        if (buf) {
          if (netif->input (buf, netif) != ERR_OK)
            pbuf_free (buf);
          }
        } while (buf);
      }
    }
  }
/*}}}*/
/*{{{*/
err_t ethernetif_init (struct netif* netif) {

  EthHandle.Instance = ETH;
  EthHandle.Init.MACAddr = (uint8_t*)kMacAddress;
  EthHandle.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
  EthHandle.Init.Speed = ETH_SPEED_100M;
  EthHandle.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
  EthHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
  EthHandle.Init.RxMode = ETH_RXINTERRUPT_MODE;
  EthHandle.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  EthHandle.Init.PhyAddress = 0; // LAN8742A_PHY_ADDRESS;
  if (HAL_ETH_Init (&EthHandle) == HAL_OK)
    netif->flags |= NETIF_FLAG_LINK_UP;

  // Initialize Rx Descriptors list: Chain Mode
  HAL_ETH_DMARxDescListInit (&EthHandle, (ETH_DMADescTypeDef*)EthRxDescripSection, (uint8_t*)EthRxBUF, ETH_RXBUFNB);

  // Initialize Tx Descriptors list: Chain Mode
  HAL_ETH_DMATxDescListInit (&EthHandle, (ETH_DMADescTypeDef*)EthTxDescripSection, (uint8_t*)EthTxBUF, ETH_TXBUFNB);

  netif->hostname = "colinST";
  netif->name[0] = 's';
  netif->name[1] = 't';
  netif->output = etharp_output;
  netif->linkoutput = ethernetOutput;
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  memcpy (netif->hwaddr, kMacAddress, 6);
  netif->mtu = 1500; // netif maximum transfer unit
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP; // Accept broadcast address and ARP traffic

  vSemaphoreCreateBinary (mRxSem);

  TaskHandle_t handle;
  xTaskCreate ((TaskFunction_t)ethernetInputThread, "eth", 350, netif, 6, &handle);

  // Enable MAC and DMA transmission and reception
  HAL_ETH_Start (&EthHandle);

  return ERR_OK;
  }
/*}}}*/

/*{{{*/
void HAL_ETH_MspInit (ETH_HandleTypeDef* hEth) {
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
void HAL_ETH_RxCpltCallback (ETH_HandleTypeDef* hEth) {
  portBASE_TYPE taskWoken = pdFALSE;
  if (xSemaphoreGiveFromISR (mRxSem, &taskWoken) == pdTRUE)
    portEND_SWITCHING_ISR (taskWoken);
  }
/*}}}*/
