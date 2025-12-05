#include "net/arp.h"
#include "peripherals/net.h"
#include "printf.h"
#include <stddef.h>
#include <stdint.h>

// --- LIENS VERS LE DRIVER ---
extern int net_send_packet(const uint8_t *packet, uint32_t length);
extern struct net_device *net_get_device(void);

static uint8_t my_mac[ETH_ALEN] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint8_t my_ip[4]         = {10, 0, 2, 15};

// --- UTILITAIRES LOCAUX ---
static void net_memcpy(void *dst, const void *src, int n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--) {
        *d++ = *s++;
    }
}

// Helpers Endianness
static inline uint16_t htons(uint16_t v) { return (v << 8) | (v >> 8); }
static inline uint32_t htonl(uint32_t v) {
    return (v << 24) | ((v << 8) & 0x00FF0000) |
           ((v >> 8) & 0x0000FF00) | (v >> 24);
}
#define ntohs htons
#define ntohl htonl

static uint32_t ip_to_u32(const uint8_t ip[4]) {
    return (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
}

void arp_init(void) {
    // On met à jour notre MAC locale avec celle du driver pour être sûr
    struct net_device *dev = net_get_device();
    if(dev) {
        net_memcpy(my_mac, dev->mac_addr, 6);
    }
    printf("[ARP] Ready.\n");
}

void arp_receive(uint8_t *packet, uint32_t len) {
    if (len < 14 + sizeof(struct arp_packet)) return;

    struct arp_packet *arp = (struct arp_packet *)(packet + 14);

    if (ntohs(arp->htype) != ARP_HTYPE_ETHERNET ||
        ntohs(arp->ptype) != ARP_PTYPE_IPV4 ||
        arp->hlen != ARP_HLEN_ETHERNET ||
        arp->plen != ARP_PLEN_IPV4)
        return;

    uint32_t target_ip = ntohl(arp->tpa);

    // Si c'est pour NOUS
    if (ntohs(arp->oper) == ARP_OP_REQUEST &&
        target_ip == ip_to_u32(my_ip)) {

        printf("[ARP] Who has %d.%d.%d.%d? It's me!\n",
               my_ip[0], my_ip[1], my_ip[2], my_ip[3]);

        arp_send_reply(arp->sha, arp->spa);
    }
}

void arp_send_reply(uint8_t *target_mac, uint32_t target_ip_be) {
    uint8_t frame[64] = {0}; // Zero init

    // 1. Header Ethernet
    net_memcpy(frame + 0,  target_mac, ETH_ALEN);
    net_memcpy(frame + 6,  my_mac,     ETH_ALEN);
    frame[12] = 0x08; frame[13] = 0x06; // Ethertype ARP

    // 2. Header ARP
    struct arp_packet *arp = (struct arp_packet *)(frame + 14);
    arp->htype = htons(ARP_HTYPE_ETHERNET);
    arp->ptype = htons(ARP_PTYPE_IPV4);
    arp->hlen  = ARP_HLEN_ETHERNET;
    arp->plen  = ARP_PLEN_IPV4;
    arp->oper  = htons(ARP_OP_REPLY);

    net_memcpy(arp->sha, my_mac,      ETH_ALEN);
    //arp->spa = htonl(ip_to_u32(my_ip));
    net_memcpy(&arp->spa, my_ip, 4);
    net_memcpy(arp->tha, target_mac,  ETH_ALEN);
    arp->tpa = target_ip_be;

    // 3. Envoi via Driver
    net_send_packet(frame, 60); // 60 bytes min

    printf("[ARP] Reply sent.\n");
}
// Ajout de memset 
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}