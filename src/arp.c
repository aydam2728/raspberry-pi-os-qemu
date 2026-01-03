#include "net/arp.h"
#include "peripherals/net.h"
#include "printf.h"
#include <stddef.h>
#include <stdint.h>

// --- LIENS VERS LE DRIVER ---
extern int net_send_packet(const uint8_t *packet, uint32_t length);
extern struct net_device *net_get_device(void);

// MAC QEMU par défaut (52:54:00:12:34:56)
static uint8_t my_mac[ETH_ALEN] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint8_t my_ip[4]         = {10, 0, 2, 15};

// --- UTILITAIRES ---
static void net_memcpy(void *dst, const void *src, int n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--) *d++ = *s++;
}

// Comparaison mémoire simple (pour IP et MAC)
static int net_memcmp(const void *s1, const void *s2, int n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

static inline uint16_t htons(uint16_t v) { return (v << 8) | (v >> 8); }
#define ntohs htons

void arp_init(void) {
    printf("[ARP] Ready. IP: %d.%d.%d.%d\n", my_ip[0], my_ip[1], my_ip[2], my_ip[3]);
}

void arp_receive(uint8_t *packet, uint32_t len) {
    if (len < 14 + sizeof(struct arp_packet)) {
        printf("[ARP ERROR] Packet too small: %d\n", len);
        return;
    }

    struct arp_packet *arp = (struct arp_packet *)(packet + 14);
    
    uint16_t hw_type = ntohs(arp->htype);
    uint16_t proto_type = ntohs(arp->ptype);
    uint16_t op_code = ntohs(arp->oper);


    if (hw_type != ARP_HTYPE_ETHERNET || 
        proto_type != ARP_PTYPE_IPV4 ||
        arp->hlen != ARP_HLEN_ETHERNET || 
        arp->plen != ARP_PLEN_IPV4) {
            printf("[ARP] Ignored: Bad Protocol/Hardware type.\n");
            return;
    }


    if (op_code == ARP_OP_REQUEST) {
        // Comparaison IP octet par octet 
        if (net_memcmp(arp->tpa, my_ip, 4) == 0) {
            
            printf("[ARP] It's for ME! Sending Reply...\n");
            arp_send_reply(arp->sha, arp->spa);
            
        }
    }
}

void arp_send_reply(uint8_t *target_mac, uint8_t *target_ip) {
    uint8_t frame[64];
    
    for(int i=0; i<64; i++) frame[i] = 0;

    // Header Ethernet
    net_memcpy(frame + 0,  target_mac, ETH_ALEN); // Dest = Demandeur
    net_memcpy(frame + 6,  my_mac,     ETH_ALEN); // Src  = Nous
    frame[12] = 0x08; frame[13] = 0x06;           // Type = ARP

    // Header ARP
    struct arp_packet *arp = (struct arp_packet *)(frame + 14);
    arp->htype = htons(ARP_HTYPE_ETHERNET);
    arp->ptype = htons(ARP_PTYPE_IPV4);
    arp->hlen  = ARP_HLEN_ETHERNET;
    arp->plen  = ARP_PLEN_IPV4;
    arp->oper  = htons(ARP_OP_REPLY); // REPLY (2)

    // Remplissage "Sender"
    net_memcpy(arp->sha, my_mac, ETH_ALEN);
    net_memcpy(arp->spa, my_ip,  4);

    // Remplissage "Target" 
    net_memcpy(arp->tha, target_mac, ETH_ALEN);
    net_memcpy(arp->tpa, target_ip,  4);

    // Envoi
    net_send_packet(frame, 60); // Padding auto jusqu'à 60
}

// Fonction memset déplacée ici (assurez-vous qu'elle n'est pas dans net.c)
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}