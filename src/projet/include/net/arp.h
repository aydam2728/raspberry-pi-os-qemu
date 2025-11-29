// kernel/net/arp.h
#ifndef NET_ARP_H
#define NET_ARP_H

#include "../peripherals/base.h"
#include <stdint.h>
#include <net.h>

// Déjà définis dans ton net.h → on les réutilise
// #define ETH_ALEN 6

#define ARP_HTYPE_ETHERNET  1
#define ARP_PTYPE_IPV4      0x0800
#define ARP_HLEN_ETHERNET   6
#define ARP_PLEN_IPV4       4

#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

struct arp_packet {
    uint16_t htype;                    // Hardware type
    uint16_t ptype;                    // Protocol type
    uint8_t  hlen;                     // Hardware address length
    uint8_t  plen;                     // Protocol address length
    uint16_t oper;                     // Operation
    uint8_t  sha[ETH_ALEN];            // Sender hardware address (MAC)
    uint32_t spa;                      // Sender protocol address (IP)
    uint8_t  tha[ETH_ALEN];            // Target hardware address (MAC)
    uint32_t tpa;                      // Target protocol address (IP)
} __attribute__((packed));

// Fonctions publiques
void arp_init(void);
void arp_receive(uint8_t *packet, uint32_t len);
void arp_send_reply(uint8_t *dst_mac, uint32_t dst_ip);

#endif