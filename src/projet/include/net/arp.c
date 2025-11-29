// kernel/net/arp.c
#include "arp.h"
#include "net.h"
#include "printf.h"
#include "string.h"

// CONFIGURATION RÉSEAU — À ADAPTER À TON RÉSEAU
static uint8_t my_mac[ETH_ALEN] = {0xb8, 0x27, 0xeb, 0x12, 0x34, 0x56};  // MAC du Pi (change si besoin)
static uint8_t my_ip[4]         = {192, 168,   1,   42};                 // IP du Pi

extern struct net_device netdev;  // Définie dans net.c

// Conversion IP → uint32_t (big endian)
static uint32_t ip_to_u32(const uint8_t ip[4]) {
    return (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
}

// htons / htonl / ntohs / ntohl (simples versions inline)
static inline uint16_t htons(uint16_t v) { return (v << 8) | (v >> 8); }
static inline uint32_t htonl(uint32_t v) {
    return (v << 24) | ((v << 8) & 0x00FF0000) |
           ((v >> 8) & 0x0000FF00) | (v >> 24);
}
#define ntohs htons
#define ntohl htonl

void arp_init(void) {
    printf("[ARP] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
    printf("[ARP] IP : %d.%d.%d.%d\n", my_ip[0], my_ip[1], my_ip[2], my_ip[3]);
}

// Appelée depuis ton driver quand tu reçois une trame Ethernet
void arp_receive(uint8_t *packet, uint32_t len) {
    if (len < 14 + sizeof(struct arp_packet)) return;

    struct arp_packet *arp = (struct arp_packet *)(packet + 14);  // +14 = après header Ethernet

    if (ntohs(arp->htype) != ARP_HTYPE_ETHERNET ||
        ntohs(arp->ptype) != ARP_PTYPE_IPV4 ||
        arp->hlen != ARP_HLEN_ETHERNET ||
        arp->plen != ARP_PLEN_IPV4)
        return;

    uint32_t target_ip = ntohl(arp->tpa);

    // C'est une requête ARP pour NOTRE IP ?
    if (ntohs(arp->oper) == ARP_OP_REQUEST &&
        target_ip == ip_to_u32(my_ip)) {

        printf("[ARP] Request: Who has %d.%d.%d.%d? → It's me!\n",
               my_ip[0], my_ip[1], my_ip[2], my_ip[3]);

        arp_send_reply(arp->sha, arp->spa);
    }
}

// Envoie une réponse ARP
void arp_send_reply(uint8_t *target_mac, uint32_t target_ip_be) {
    uint8_t frame[64] = {0};  // 60 min Ethernet + padding

    // Header Ethernet
    memcpy(frame + 0,  target_mac,           ETH_ALEN);           // dst MAC
    memcpy(frame + 6,  my_mac,               ETH_ALEN);           // src MAC
    frame[12] = 0x08; frame[13] = 0x06;  // EtherType = ARP

    // Header ARP
    struct arp_packet *arp = (struct arp_packet *)(frame + 14);
    arp->htype = htons(ARP_HTYPE_ETHERNET);
    arp->ptype = htons(ARP_PTYPE_IPV4);
    arp->hlen  = ARP_HLEN_ETHERNET;
    arp->plen  = ARP_PLEN_IPV4;
    arp->oper  = htons(ARP_OP_REPLY);

    
    memcpy(arp->sha, my_mac,      ETH_ALEN);
    arp->spa = ip_to_u32(my_ip);
    memcpy(arp->tha, target_mac,  ETH_ALEN);
    arp->tpa = target_ip_be;

    // Envoi via ton driver (tu as déjà une fonction ?)
    // → à adapter selon ton driver actuel
    extern void usbnet_send(uint8_t *data, uint32_t len);
    usbnet_send(frame, 60);

    printf("[ARP] Reply sent to %02x:%02x:%02x:%02x:%02x:%02x\n",
           target_mac[0], target_mac[1], target_mac[2],
           target_mac[3], target_mac[4], target_mac[5]);
}