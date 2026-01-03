#include "peripherals/net.h"
#include "peripherals/usb.h"
#include "net/rndis.h"
#include "net/arp.h"
#include "net/icmp.h"
#include "utils.h"
#include "printf.h"
#include <stddef.h>

extern void *memset(void *s, int c, size_t n);

/// RX Code
static uint8_t net_rx_buffer[2048] __attribute__((aligned(64)));
static int bulk_in_toggle = 0;
static struct net_device *net_dev_ptr = NULL;

void net_rx_set_device(struct net_device *dev) {
    net_dev_ptr = dev;
}

void net_rx_reset_toggle(void) {
    bulk_in_toggle = 0;
}

void ethernet_input(uint8_t *packet, uint32_t len) {
    if (len < 14) return;
    uint16_t type = (packet[12] << 8) | packet[13];
    
    printf("ETH: RX type=0x%04x len=%u\r\n", type, len);
    
    if (type == 0x0806) {
        arp_receive(packet, len);
    }
    else if (type == 0x0800 && packet[14+9] == 1) {
        icmp_receive(packet, len);
    }
}

void submit_rx_request(void) {
    if (!net_dev_ptr) return;
    
    memset(net_rx_buffer, 0, 2048); 
    invalidate_dcache_range(net_rx_buffer, 2048);

    usb_write(HCINTMSK(CHAN_RX), 0);
    
    uint32_t len = ETH_FRAME_LEN;
    uint32_t pkt_cnt = (len + 511) / 512; 
    uint32_t pid = (bulk_in_toggle) ? HCTSIZ_PID_DATA1 : HCTSIZ_PID_DATA0;
    
    usb_write(HCTSIZ(CHAN_RX), pid | HCTSIZ_PKTCNT(pkt_cnt) | HCTSIZ_XFRSIZ(len));
    usb_write(HCDMA(CHAN_RX), (uintptr_t)net_rx_buffer);

    uint32_t hcchar = HCCHAR_DEVADDR(net_dev_ptr->usb_addr) | HCCHAR_EPNUM(EP_BULK_IN) | 
                      HCCHAR_EPTYPE(EPTYPE_BULK) | HCCHAR_EPDIR_IN | HCCHAR_MPS(512);
             
    usb_write(HCINTMSK(CHAN_RX), HCINTMSK_XFRCM | HCINTMSK_CHHM | 
                                 HCINTMSK_STALLM | HCINTMSK_TXERRM | HCINTMSK_AHBERRM);
    usb_write(HCCHAR(CHAN_RX), hcchar | HCCHAR_CHENA);
}

void handle_rx_complete(void) {
    if (!net_dev_ptr) return;
    
    bulk_in_toggle = !bulk_in_toggle;
    invalidate_dcache_range(net_rx_buffer, 2048);
    
    if (rndis_is_hardware_mode()) {
        // VRAI HARDWARE: Format RNDIS
        uint32_t *rndis = (uint32_t*)net_rx_buffer;
        uint32_t msg_type = rndis[0];
        uint32_t real_len = rndis[1]; 
        
        if (msg_type == 0x00000001 && real_len >= 40 && real_len < 2000) {
            uint32_t data_offset = 8 + rndis[2];
            uint32_t data_len = rndis[3];
            
            if (data_len > 0 && (data_offset + data_len <= real_len)) {
                net_dev_ptr->rx_packets++;
                ethernet_input(net_rx_buffer + data_offset, data_len);
            }
        }
    } else {
        // QEMU: Essayer RNDIS OU direct
        uint32_t *rndis = (uint32_t*)net_rx_buffer;
        if (rndis[0] == 0x00000001) {
            // Format RNDIS
            uint32_t data_offset = 8 + rndis[2];
            uint32_t data_len = rndis[3];
            if (data_len > 0) {
                net_dev_ptr->rx_packets++;
                ethernet_input(net_rx_buffer + data_offset, data_len);
            }
        } else {
            // Format direct (fallback)
            uint32_t xfrsiz = usb_read(HCTSIZ(CHAN_RX)) & HCTSIZ_XFRSIZ_MASK;
            uint32_t len = ETH_FRAME_LEN - xfrsiz;
            if (len > 14 && len < 2000) {
                net_dev_ptr->rx_packets++;
                ethernet_input(net_rx_buffer, len);
            }
        }
    }
}

/// TX Code
static uint8_t tx_rndis_buffer[2048] __attribute__((aligned(64)));

void net_tx_set_device(struct net_device *dev) {
    net_dev_ptr = dev;
}

int net_send_packet(const uint8_t *packet, uint32_t length) {
    if (!net_dev_ptr || net_dev_ptr->state != NET_STATE_UP) return -1;
    
    if (rndis_is_hardware_mode()) {
        // VRAI HARDWARE: Encapsuler RNDIS
        uint32_t *header = (uint32_t *)tx_rndis_buffer;
        header[0] = 0x00000001;  // RNDIS_MSG_PACKET
        header[1] = 44 + length;  // Message length
        header[2] = 36;           // Data offset
        header[3] = length;       // Data length
        
        // Padding
        uint8_t *pad = (uint8_t*)tx_rndis_buffer + 16; 
        for(int i=0; i<28; i++) *pad++ = 0;
        
        // Copier paquet Ethernet
        uint8_t *dest = (uint8_t*)tx_rndis_buffer + 44; 
        for(uint32_t i=0; i<length; i++) dest[i] = packet[i];

        if (usb_bulk_transfer(net_dev_ptr->usb_addr, EP_BULK_OUT, tx_rndis_buffer, 
                             length + 44, HCCHAR_EPDIR_OUT) < 0) {
            net_dev_ptr->tx_errors++; 
            return -1;
        }
    } else {
        // QEMU: Essayer direct 
        for(uint32_t i=0; i<length; i++) tx_rndis_buffer[i] = packet[i];
        
        if (usb_bulk_transfer(net_dev_ptr->usb_addr, EP_BULK_OUT, tx_rndis_buffer, 
                             length, HCCHAR_EPDIR_OUT) < 0) {
            net_dev_ptr->tx_errors++; 
            return -1;
        }
    }
    
    net_dev_ptr->tx_packets++; 
    return 0;
}