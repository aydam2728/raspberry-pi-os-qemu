#ifndef NET_ICMP_H
#define NET_ICMP_H

#include <stdint.h>

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

struct icmp_echo {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
    // données suivent...
} __attribute__((packed));

void icmp_init(void);
void icmp_receive(uint8_t *packet, uint32_t len);

// Renommée pour éviter le conflit avec net.c
void ethernet_send_packet(uint8_t *dst_mac, uint16_t ethertype, uint8_t *payload, uint32_t payload_len);

#endif