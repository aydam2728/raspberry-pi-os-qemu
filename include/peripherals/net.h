#ifndef _P_NET_H
#define _P_NET_H

#include "base.h"
#include <stdint.h>

#define ETH_ALEN 6
#define ETH_FRAME_LEN 1536 
#define NET_STATE_DOWN 0
#define NET_STATE_UP   1

struct net_device {
    uint8_t mac_addr[ETH_ALEN];
    uint8_t usb_addr;
    int state;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_errors;
    uint32_t tx_errors;
};

int net_init(void);
int net_send_packet(const uint8_t *packet, uint32_t length);
int net_receive_packet(uint8_t *packet, uint32_t max_length);
struct net_device *net_get_device(void);
void net_tick(void); // Pour IRQ Timer
void handle_usb_irq(void); // Pour IRQ vector

#endif