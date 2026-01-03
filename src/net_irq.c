#include "peripherals/net.h"
#include "peripherals/usb.h"
#include "utils.h"
#include "printf.h"

// Déclarée dans net_rx.c
extern void handle_rx_complete(void);
extern void submit_rx_request(void);

void handle_usb_irq(void) {
    uint32_t gintsts = usb_read(USB_GINTSTS);
    usb_write(USB_GINTSTS, gintsts);

    if (gintsts & GINTMSK_HCIM) {
        uint32_t haint = usb_read(USB_HAINT);
        
        for (int i=0; i < 8; i++) {
            if (haint & (1 << i)) {
                uint32_t hcint = usb_read(HCINT(i));
                usb_write(HCINT(i), hcint);

                if (i == CHAN_RX) {
                    if (hcint & HCINT_XFRC) {
                        handle_rx_complete();
                    }
                    submit_rx_request(); 
                }
            }
        }
    }
}

void net_irq_handler(void) {}