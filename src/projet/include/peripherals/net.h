#ifndef _P_NET_H
#define _P_NET_H

#include "base.h"

/* USB Controller Base Address pour BCM2837*/
#define USB_BASE        (PBASE + 0x00980000)

/*Registers */

/* MAC Address registers */
#define MAC_CR          0x100  /* MAC Control Register */
#define ADDRH           0x104  /* MAC Address High */
#define ADDRL           0x108  /* MAC Address Low */

/* PHY registers */
#define PHY_BCR         0x00   /* Basic Control Register */
#define PHY_BSR         0x01   /* Basic Status Register */
#define PHY_ID1         0x02   /* PHY Identifier 1 */
#define PHY_ID2         0x03   /* PHY Identifier 2 */

/* Control flags */
#define MAC_CR_TXEN     (1 << 3)   /* Transmit Enable */
#define MAC_CR_RXEN     (1 << 2)   /* Receive Enable */

/* PHY Control flags */
#define PHY_BCR_RESET           (1 << 15)  /* PHY Reset */
#define PHY_BCR_LOOPBACK        (1 << 14)  /* Loopback mode */
#define PHY_BCR_SPEED_SEL       (1 << 13)  /* Speed selection (1=100Mb/s) */
#define PHY_BCR_AN_ENABLE       (1 << 12)  /* Auto-negotiation enable */
#define PHY_BCR_POWER_DOWN      (1 << 11)  /* Power down */
#define PHY_BCR_ISOLATE         (1 << 10)  /* Isolate */
#define PHY_BCR_RESTART_AN      (1 << 9)   /* Restart auto-negotiation */
#define PHY_BCR_DUPLEX_MODE     (1 << 8)   /* Duplex mode (1=full) */

/* PHY Status flags */
#define PHY_BSR_100BASE_T4      (1 << 15)  /* 100BASE-T4 capable */
#define PHY_BSR_100BASE_TX_FD   (1 << 14)  /* 100BASE-TX full duplex capable */
#define PHY_BSR_100BASE_TX_HD   (1 << 13)  /* 100BASE-TX half duplex capable */
#define PHY_BSR_10BASE_T_FD     (1 << 12)  /* 10BASE-T full duplex capable */
#define PHY_BSR_10BASE_T_HD     (1 << 11)  /* 10BASE-T half duplex capable */
#define PHY_BSR_AN_COMPLETE     (1 << 5)   /* Auto-negotiation complete */
#define PHY_BSR_REMOTE_FAULT    (1 << 4)   /* Remote fault detected */
#define PHY_BSR_AN_ABILITY      (1 << 3)   /* Auto-negotiation ability */
#define PHY_BSR_LINK_STATUS     (1 << 2)   /* Link status */
#define PHY_BSR_JABBER_DETECT   (1 << 1)   /* Jabber detect */
#define PHY_BSR_EXTENDED_CAP    (1 << 0)   /* Extended capability */

/* USB DWC2 (DesignWare Core 2) Registers */
#define USB_GOTGCTL     0x000  /* OTG Control and Status Register */
#define USB_GOTGINT     0x004  /* OTG Interrupt Register */
#define USB_GAHBCFG     0x008  /* AHB Configuration Register */
#define USB_GUSBCFG     0x00C  /* USB Configuration Register */
#define USB_GRSTCTL     0x010  /* Reset Register */
#define USB_GINTSTS     0x014  /* Interrupt Register */
#define USB_GINTMSK     0x018  /* Interrupt Mask Register */
#define USB_GRXSTSR     0x01C  /* Receive Status Debug Read Register */
#define USB_GRXSTSP     0x020  /* Receive Status Read/Pop Register */
#define USB_GRXFSIZ     0x024  /* Receive FIFO Size Register */
#define USB_GNPTXFSIZ   0x028  /* Non-Periodic Transmit FIFO Size Register */

/* USB GAHBCFG Register bits */
#define USB_GAHBCFG_GLBL_INTR_EN    (1 << 0)   /* Global Interrupt Enable */
#define USB_GAHBCFG_HBSTLEN_INCR4   (1 << 1)   /* Burst length */
#define USB_GAHBCFG_DMA_EN          (1 << 5)   /* DMA Enable */
#define USB_GAHBCFG_TXFEMPTYLVL     (1 << 7)   /* TxFIFO Empty Level */
#define USB_GAHBCFG_PTXFEMPTYLVL    (1 << 8)   /* Periodic TxFIFO Empty Level */

/* USB GRSTCTL Register bits */
#define USB_GRSTCTL_CSFTRST         (1 << 0)   /* Core Soft Reset */
#define USB_GRSTCTL_HSFTRST         (1 << 1)   /* HCLK Soft Reset */
#define USB_GRSTCTL_FRMCNTRRST      (1 << 2)   /* Frame Counter Reset */
#define USB_GRSTCTL_RXFFLSH         (1 << 4)   /* RxFIFO Flush */
#define USB_GRSTCTL_TXFFLSH         (1 << 5)   /* TxFIFO Flush */
#define USB_GRSTCTL_AHBIDLE         (1 << 31)  /* AHB Master Idle */

#endif