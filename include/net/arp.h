#ifndef _NET_ARP_H
#define _NET_ARP_H

#include <stdint.h>

#define ARP_HTYPE_ETHERNET  1
#define ARP_PTYPE_IPV4      0x0800
#define ARP_HLEN_ETHERNET   6
#define ARP_PLEN_IPV4       4
#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

struct arp_packet {
    uint16_t htype; // Hardware type
    uint16_t ptype; // Protocol type
    uint8_t  hlen;  // Hardware address length
    uint8_t  plen;  // Protocol address length
    uint16_t oper;  // Operation
    uint8_t  sha[6]; // Sender hardware address
    uint8_t  spa[4]; // Sender protocol address (IP) 
    uint8_t  tha[6]; // Target hardware address
    uint8_t  tpa[4]; // Target protocol address (IP) 
} __attribute__((packed));

void arp_init(void);
void arp_receive(uint8_t *packet, uint32_t len);
void arp_send_reply(uint8_t *target_mac, uint8_t *target_ip);

#endif