#ifndef _P_NET_H
#define _P_NET_H

#include "base.h"
#include <stdint.h>

int net_init(void);
/*added*/
int net_send_packet(const uint8_t *packet, uint32_t length);
int net_receive_packet(uint8_t *packet, uint32_t max_length);
struct net_device *net_get_device(void);

// --- CONSTANTES ---
#define ETH_ALEN 6
#define ETH_FRAME_LEN 1536 

#define NET_STATE_DOWN      0
#define NET_STATE_UP        1
#define NET_STATE_LINK_DOWN 2
#define NET_STATE_LINK_UP   3


// --- STRUCTURES ---
// --- STRUCTURES ---
struct net_device {
    uint8_t mac_addr[ETH_ALEN];
    uint8_t usb_addr;
    int state;
    uint32_t link_speed;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_errors;
    uint32_t tx_errors;
};

// Descripteur de Périphérique (AJOUT CRITIQUE)
struct usb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed, aligned(4)));

struct usb_config_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed, aligned(4)));

#define USB_REQ_SET_CONFIGURATION   0x09

// Types de descripteurs 
#define USB_DT_DEVICE           0x01
#define USB_DT_CONFIGURATION    0x02

// --- DWC2 REGISTRES ---
#define USB_BASE        (PBASE + 0x00980000)

// Registres Globaux
#define USB_GOTGCTL     0x000
#define USB_GOTGINT     0x004
#define USB_GAHBCFG     0x008
#define USB_GUSBCFG     0x00C
#define USB_GRSTCTL     0x010
#define USB_GINTSTS     0x014
#define USB_GINTMSK     0x018
#define USB_GRXFSIZ     0x024
#define USB_GNPTXFSIZ   0x028
#define USB_HAINT       0x414  // Host All Interrupts Register
#define USB_HAINTMSK    0x418  // Host All Interrupts Mask Register

#define USB_GAHBCFG_GLBL_INTR_EN    (1 << 0)
#define USB_GAHBCFG_HBSTLEN_INCR4   (1 << 1)
#define USB_GAHBCFG_DMA_EN          (1 << 5)
#define USB_GRSTCTL_CSFTRST         (1 << 0)
#define USB_GRSTCTL_AHBIDLE         (1 << 31)
#define USB_GUSBCFG_FHMOD           (1 << 8)

// Masques d'interruption
#define GINTMSK_HCIM    (1 << 25) 
#define GINTMSK_PRTIM   (1 << 24)
#define GINTMSK_DISCINT (1 << 29)
#define GINTMSK_SOF     (1 << 3)
#define GINTMSK_RXFLVLM (1 << 4)

// Registres Hôtes
#define USB_HCFG        0x400
#define USB_HPRT        0x440

#define HPRT_PRTPWR     (1 << 12)
#define HPRT_PRTRST     (1 << 8)
#define HPRT_PRTENA     (1 << 2)
#define HPRT_PRTCONNS   (1 << 0) /* Bit 0: Connected */

// HPRT Change Bits (W1C)
#define HPRT_PRTCONCHG      (1 << 1)
#define HPRT_PRTENCHG       (1 << 3)
#define HPRT_PRTOVRCURRCHG  (1 << 5)

// --- REGISTRES CANAUX HÔTES ---
#define HCCHAR(n)   (0x500 + (0x20 * (n)))
#define HCSPLT(n)   (0x504 + (0x20 * (n)))
#define HCINT(n)    (0x508 + (0x20 * (n)))  
#define HCINTMSK(n) (0x50C + (0x20 * (n)))  
#define HCTSIZ(n)   (0x510 + (0x20 * (n)))  
#define HCDMA(n)    (0x514 + (0x20 * (n)))  

// HCCHAR Bits
#define HCCHAR_CHENA        (1 << 31)
#define HCCHAR_CHDIS        (1 << 30)
#define HCCHAR_ODDFRM       (1 << 29)
#define HCCHAR_DEVADDR(x)   ((x & 0x7F) << 22)
#define HCCHAR_EPTYPE(x)    ((x & 0x3) << 18)
#define HCCHAR_LSPDDEV      (1 << 17)
#define HCCHAR_EPDIR_IN     (1 << 15)
#define HCCHAR_EPDIR_OUT    (0 << 15)
#define HCCHAR_EPNUM(x)     ((x & 0xF) << 11)
#define HCCHAR_MPS(x)       ((x & 0x7FF) << 0)

#define EPTYPE_CTRL 0
#define EPTYPE_ISO  1
#define EPTYPE_BULK 2
#define EPTYPE_INTR 3

// HCTSIZ Bits
#define HCTSIZ_PID_DATA0    (0 << 29)
#define HCTSIZ_PID_DATA1    (2 << 29)
#define HCTSIZ_PID_SETUP    (3 << 29)
#define HCTSIZ_PKTCNT(x)    ((x & 0x3FF) << 19)
#define HCTSIZ_XFRSIZ(x)    ((x & 0x7FFFF) << 0)
#define HCTSIZ_XFRSIZ_MASK  0x7FFFF

// HCINT Bits
#define HCINT_XFRC          (1 << 0) 
#define HCINT_CHH           (1 << 1) 
#define HCINT_AHBERR        (1 << 2) 
#define HCINT_STALL         (1 << 3)
#define HCINT_NAK           (1 << 4)
#define HCINT_ACK           (1 << 5)
#define HCINT_TXERR         (1 << 10)
#define HCINTMSK_XFRCM      (1 << 0)  // Transfer Complete Mask
#define HCINTMSK_CHHM       (1 << 1)  // Channel Halted Mask
#define HCINTMSK_AHBERRM    (1 << 2)  // AHB Error Mask
#define HCINTMSK_STALLM     (1 << 3)  // STALL Response Mask
#define HCINTMSK_NAKM       (1 << 4)  // NAK Response Mask
#define HCINTMSK_ACKM       (1 << 5)  // ACK Response Mask
#define HCINTMSK_TXERRM     (1 << 10) // Transaction Error Mask
#define HCINTMSK_BBLERRM    (1 << 11) // Babble Error Mask
#define HCINTMSK_FRMORM     (1 << 12) // Frame Overrun Mask
#define HCINTMSK_DATATGLERRM (1 << 13) // Data Toggle Error Mask

// Adresses des Endpoints
#define EP_BULK_IN      1
#define EP_BULK_OUT     2

#endif /* _P_NET_H */