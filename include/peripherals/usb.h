#ifndef _P_USB_H
#define _P_USB_H

#include "base.h"
#include <stdint.h>
#include <stddef.h>

// --- CONSTANTES CANAUX ---
#define CHAN_CONTROL 0
#define CHAN_RX      1  
#define CHAN_TX      2  

// --- STRUCTURES ---
struct usb_setup_packet {
    uint8_t bmRequestType; uint8_t bRequest; uint16_t wValue; uint16_t wIndex; uint16_t wLength;
} __attribute__((packed, aligned(4)));

// --- PROTOTYPES ---
int usb_init(void);
int usb_control_transfer(uint8_t dev_addr, struct usb_setup_packet *setup, uint8_t *data_buf, uint32_t data_len);
int usb_bulk_transfer(uint8_t dev_addr, uint8_t ep_num, uint8_t *buf, uint32_t len, int dir);
int usb_enumerate_set_address(uint8_t new_addr);
int usb_set_configuration(uint8_t addr, uint8_t config_value);

// --- REGISTRES DWC2 ---
#define USB_BASE        (PBASE + 0x00980000)
#define USB_GRSTCTL     0x010
#define USB_GINTSTS     0x014
#define USB_GINTMSK     0x018
#define USB_GRXFSIZ     0x024
#define USB_GNPTXFSIZ   0x028
#define USB_HAINT       0x414
#define USB_HAINTMSK    0x418
#define USB_GAHBCFG     0x008
#define USB_GUSBCFG     0x00C
#define USB_HPRT        0x440

#define USB_GAHBCFG_GLBL_INTR_EN    (1 << 0)
#define USB_GAHBCFG_HBSTLEN_INCR4   (1 << 1)
#define USB_GAHBCFG_DMA_EN          (1 << 5)
#define USB_GRSTCTL_CSFTRST         (1 << 0)
#define USB_GRSTCTL_AHBIDLE         (1 << 31)
#define USB_GUSBCFG_FHMOD           (1 << 8)
#define HPRT_PRTPWR                 (1 << 12)
#define HPRT_PRTRST                 (1 << 8)
#define HPRT_PRTENA                 (1 << 2)
#define HPRT_PRTCONNS               (1 << 0)

// Registres Canaux
#define HCCHAR(n)   (0x500 + (0x20 * (n)))
#define HCINT(n)    (0x508 + (0x20 * (n)))  
#define HCINTMSK(n) (0x50C + (0x20 * (n)))  
#define HCTSIZ(n)   (0x510 + (0x20 * (n)))  
#define HCDMA(n)    (0x514 + (0x20 * (n)))  

#define HCCHAR_CHENA        (1 << 31)
#define HCCHAR_CHDIS        (1 << 30)
#define HCCHAR_ODDFRM       (1 << 29)
#define HCCHAR_DEVADDR(x)   ((x & 0x7F) << 22)
#define HCCHAR_EPTYPE(x)    ((x & 0x3) << 18)
#define HCCHAR_EPDIR_IN     (1 << 15)
#define HCCHAR_EPDIR_OUT    (0 << 15)
#define HCCHAR_EPNUM(x)     ((x & 0xF) << 11)
#define HCCHAR_MPS(x)       ((x & 0x7FF) << 0)

#define EPTYPE_CTRL 0
#define EPTYPE_BULK 2

#define HCTSIZ_PID_DATA0    (0 << 29)
#define HCTSIZ_PID_DATA1    (2 << 29)
#define HCTSIZ_PID_SETUP    (3 << 29)
#define HCTSIZ_PKTCNT(x)    ((x & 0x3FF) << 19)
#define HCTSIZ_XFRSIZ(x)    ((x & 0x7FFFF) << 0)
#define HCTSIZ_XFRSIZ_MASK  0x7FFFF

#define HCINT_XFRC          (1 << 0) 
#define HCINT_CHH           (1 << 1) 
#define HCINT_AHBERR        (1 << 2) 
#define HCINT_STALL         (1 << 3)
#define HCINT_NAK           (1 << 4)
#define HCINT_TXERR         (1 << 10)

#define HCINTMSK_XFRCM      (1 << 0)
#define HCINTMSK_CHHM       (1 << 1)
#define HCINTMSK_AHBERRM    (1 << 2)
#define HCINTMSK_STALLM     (1 << 3)
#define HCINTMSK_TXERRM     (1 << 10)
#define HCINTMSK_NAKM       (1 << 4)

#define GINTMSK_HCIM        (1 << 25) 
#define GINTMSK_DISCINT     (1 << 29)

#define EP_BULK_IN      1
#define EP_BULK_OUT     2

// --- HELPERS INLINE ---
static inline uint32_t usb_read(uint32_t reg) { return *(volatile uint32_t *)(USB_BASE + reg); }
static inline void usb_write(uint32_t reg, uint32_t value) { *(volatile uint32_t *)(USB_BASE + reg) = value; }

// --- CACHE INLINE ---
static inline void invalidate_dcache_range(const void *addr, size_t size) {
    uintptr_t start = (uintptr_t)addr & ~63;
    uintptr_t end = ((uintptr_t)addr + size + 63) & ~63;
    asm volatile("dsb sy");
    while (start < end) { asm volatile("dc ivac, %0" : : "r" (start)); start += 64; }
    asm volatile("dsb sy"); asm volatile("isb");
}
static inline void clean_dcache_range(const void *addr, size_t size) {
    uintptr_t start = (uintptr_t)addr & ~63;
    uintptr_t end = ((uintptr_t)addr + size + 63) & ~63;
    asm volatile("dsb sy");
    while (start < end) { asm volatile("dc cvac, %0" : : "r" (start)); start += 64; }
    asm volatile("dsb sy"); asm volatile("isb");
}


#endif