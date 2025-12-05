#ifndef NET_ARP_H
#define NET_ARP_H

#include <stdint.h>
#include "peripherals/net.h"

// Définitions
#define ARP_HTYPE_ETHERNET  1
#define ARP_PTYPE_IPV4      0x0800
#define ARP_HLEN_ETHERNET   6
#define ARP_PLEN_IPV4       4

#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

struct arp_packet {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[ETH_ALEN];
    uint32_t spa;
    uint8_t  tha[ETH_ALEN];
    uint32_t tpa;
} __attribute__((packed));

void arp_init(void);
void arp_receive(uint8_t *packet, uint32_t len);
void arp_send_reply(uint8_t *dst_mac, uint32_t dst_ip);

#endif