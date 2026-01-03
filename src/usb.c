#include "peripherals/usb.h"
#include "utils.h"
#include "printf.h"
#include <stddef.h>

static int bulk_out_toggle = 0; 

// Timeout court pour QEMU (ne pas bloquer), plus long pour vrai hardware
static int usb_host_wait_xfer_complete(uint32_t chan_num, int tolerate_failure) {
    uint32_t timeout = 500000;  // Réduit pour ne pas bloquer sur QEMU
    uint32_t hcint_reg = HCINT(chan_num); 
    volatile uint32_t hcint;
    int xfrc_seen = 0;
    
    while (timeout-- > 0) {
        hcint = usb_read(hcint_reg);
        
        if (hcint) {
            usb_write(hcint_reg, hcint);

            if (hcint & HCINT_XFRC) {
                xfrc_seen = 1;
                return 0;
            }
            
            if (hcint & HCINT_CHH) {
                if (xfrc_seen) {
                    return 0;
                }
                
                // CHH sans XFRC
                if (tolerate_failure) {
                    // Mode tolérant (RNDIS/QEMU)
                    return 0;
                }
                
                return -1;
            }
            
            if (hcint & HCINT_STALL) {
                printf("USB: STALL ch%d\r\n", chan_num);
                return -1;
            }
            if (hcint & HCINT_TXERR) {
                if (!tolerate_failure) {
                    printf("USB: TXERR ch%d\r\n", chan_num);
                }
                return -1;
            }
            if (hcint & HCINT_AHBERR) {
                printf("USB: AHBERR ch%d\r\n", chan_num);
                return -1;
            }
        }
        
        delay(1); 
    }
    
    // Timeout
    if (!tolerate_failure) {
        printf("USB: Timeout ch%d\r\n", chan_num);
    }
    return -1;
}

static int usb_core_reset(void) {
    uint32_t timeout = 100000;
    
    while (!(usb_read(USB_GRSTCTL) & USB_GRSTCTL_AHBIDLE)) { 
        if (--timeout == 0) {
            printf("USB: AHB idle timeout!\r\n");
            return -1;
        }
        delay(10); 
    }
    
    usb_write(USB_GRSTCTL, USB_GRSTCTL_CSFTRST);
    timeout = 100000;
    while (usb_read(USB_GRSTCTL) & USB_GRSTCTL_CSFTRST) { 
        if (--timeout == 0) {
            printf("USB: Core reset timeout!\r\n");
            return -1;
        }
        delay(10); 
    }
    
    delay(10000); 
    return 0;
}

static int usb_host_init(void) {
    uint32_t reg; 
    uint32_t timeout;
    
    usb_write(USB_GRXFSIZ, 0x400); 
    usb_write(USB_GNPTXFSIZ, (0x200 << 16) | 0x400); 
    
    reg = usb_read(USB_HPRT); 
    if (!(reg & HPRT_PRTPWR)) { 
        reg |= HPRT_PRTPWR; 
        usb_write(USB_HPRT, reg); 
        delay(100000); 
    }
    
    reg = usb_read(USB_HPRT); 
    reg |= HPRT_PRTRST; 
    usb_write(USB_HPRT, reg); 
    delay(100000); 
    
    reg &= ~HPRT_PRTRST; 
    usb_write(USB_HPRT, reg); 
    delay(50000);

    timeout = 500000;
    while (!(usb_read(USB_HPRT) & HPRT_PRTCONNS)) {
        if (--timeout == 0) {
            printf("USB: No device!\r\n");
            return -1;
        }
        delay(10);
    }
    
    delay(50000); 
    
    reg = usb_read(USB_HPRT);
    printf("USB: Connected (HPRT=0x%x)\r\n", reg);
    
    return 0;
}

int usb_init(void) {
    if (usb_core_reset() < 0) return -1;
    
    usb_write(USB_GAHBCFG, USB_GAHBCFG_GLBL_INTR_EN | USB_GAHBCFG_HBSTLEN_INCR4 | USB_GAHBCFG_DMA_EN);
    usb_write(USB_GUSBCFG, usb_read(USB_GUSBCFG) | USB_GUSBCFG_FHMOD);
    usb_write(USB_GINTMSK, GINTMSK_HCIM | GINTMSK_DISCINT);
    usb_write(USB_HAINTMSK, 0xFFFFFFFF);
    
    return usb_host_init();
}

int usb_control_transfer(uint8_t dev_addr, struct usb_setup_packet *setup, uint8_t *data_buf, uint32_t data_len) {
    uint32_t hcchar; 
    uintptr_t dma_addr = (uintptr_t)setup; 
    const uint32_t chan_num = CHAN_CONTROL;
    int is_data_in = (setup->bmRequestType & 0x80) ? 1 : 0;
    
    // Détecter requêtes RNDIS (Class)
    int is_rndis = ((setup->bmRequestType & 0x60) == 0x20);
    
    clean_dcache_range(setup, sizeof(*setup));
    if (data_len > 0 && !is_data_in) clean_dcache_range(data_buf, data_len);
    if (data_len > 0 && is_data_in) invalidate_dcache_range(data_buf, data_len);

    // --- PHASE SETUP ---
    usb_write(HCINTMSK(chan_num), 0xFFFFFFFF);
    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | HCCHAR_EPTYPE(EPTYPE_CTRL) | HCCHAR_EPDIR_OUT | HCCHAR_MPS(64);
    usb_write(HCCHAR(chan_num), hcchar); 
    usb_write(HCTSIZ(chan_num), HCTSIZ_PID_SETUP | HCTSIZ_PKTCNT(1) | HCTSIZ_XFRSIZ(8));
    usb_write(HCDMA(chan_num), dma_addr); 
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);
    
    if (usb_host_wait_xfer_complete(chan_num, is_rndis) < 0) {
        return -1;
    }
    
    // --- PHASE DATA ---
    if (data_len > 0 && data_buf != NULL) {
        uint32_t pid = HCTSIZ_PID_DATA1; 
        hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | HCCHAR_EPTYPE(EPTYPE_CTRL) | 
                 (is_data_in ? HCCHAR_EPDIR_IN : HCCHAR_EPDIR_OUT) | HCCHAR_MPS(64);
        usb_write(HCCHAR(chan_num), hcchar);
        uint32_t pkt_cnt = (data_len + 63) / 64; 
        if (pkt_cnt == 0) pkt_cnt = 1;
        usb_write(HCTSIZ(chan_num), pid | HCTSIZ_PKTCNT(pkt_cnt) | HCTSIZ_XFRSIZ(data_len));
        usb_write(HCDMA(chan_num), (uintptr_t)data_buf); 
        usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);
        
        if (usb_host_wait_xfer_complete(chan_num, is_rndis) < 0) {
            return -1;
        }
    }
    
    // --- PHASE STATUS ---
    uint32_t status_dir = (data_len > 0 && is_data_in) ? HCCHAR_EPDIR_OUT : HCCHAR_EPDIR_IN;
    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | HCCHAR_EPTYPE(EPTYPE_CTRL) | status_dir | HCCHAR_MPS(64);
    usb_write(HCCHAR(chan_num), hcchar); 
    usb_write(HCTSIZ(chan_num), HCTSIZ_PID_DATA1 | HCTSIZ_PKTCNT(1) | HCTSIZ_XFRSIZ(0));
    usb_write(HCDMA(chan_num), 0); 
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);
    
    // Toujours tolérer CHH dans STATUS
    if (usb_host_wait_xfer_complete(chan_num, 1) < 0) {
        return -1;
    }
    
    if (data_len > 0 && is_data_in) {
        invalidate_dcache_range(data_buf, data_len);
    }
    
    usb_write(HCINTMSK(chan_num), 0);
    return 0;
}

int usb_enumerate_set_address(uint8_t new_addr) {
    struct usb_setup_packet setup = {0, 5, 0, 0, 0}; 
    setup.wValue = new_addr;
    int result = usb_control_transfer(0, &setup, NULL, 0);
    if (result < 0) return -1;
    delay(20000);
    return 0;
}

int usb_set_configuration(uint8_t addr, uint8_t config_value) {
    struct usb_setup_packet setup = {0, 9, 0, 0, 0}; 
    setup.wValue = config_value;
    int result = usb_control_transfer(addr, &setup, NULL, 0);
    if (result < 0) return -1;
    delay(10000);
    return 0;
}

int usb_bulk_transfer(uint8_t dev_addr, uint8_t ep_num, uint8_t *buf, uint32_t len, int dir) {
    const uint32_t chan_num = CHAN_TX; 
    uint32_t hcchar, pid;
    
    if (dir == HCCHAR_EPDIR_OUT) clean_dcache_range(buf, len);

    usb_write(HCINTMSK(chan_num), 0xFFFFFFFF);
    pid = (bulk_out_toggle) ? HCTSIZ_PID_DATA1 : HCTSIZ_PID_DATA0;
    
    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(ep_num) | HCCHAR_EPTYPE(EPTYPE_BULK) | dir | HCCHAR_MPS(512);
    usb_write(HCCHAR(chan_num), hcchar);
    uint32_t pkt_cnt = (len + 511) / 512; 
    if (pkt_cnt == 0) pkt_cnt = 1;
    usb_write(HCTSIZ(chan_num), pid | HCTSIZ_PKTCNT(pkt_cnt) | HCTSIZ_XFRSIZ(len));
    usb_write(HCDMA(chan_num), (uintptr_t)buf);
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);
    
    int res = usb_host_wait_xfer_complete(chan_num, 0);
    usb_write(HCINTMSK(chan_num), 0);

    if (res < 0) return -1;
    if (dir == HCCHAR_EPDIR_OUT) bulk_out_toggle = !bulk_out_toggle;
    
    uint32_t xfrsiz_remaining = usb_read(HCTSIZ(chan_num)) & HCTSIZ_XFRSIZ_MASK;
    return len - xfrsiz_remaining;
}