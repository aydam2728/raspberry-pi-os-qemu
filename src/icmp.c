#include "net/icmp.h"
#include "net/arp.h"
#include "peripherals/net.h" // Pour struct net_device
#include "printf.h"
#include <stddef.h>
#include <stdint.h>

// --- LIENS VERS LE DRIVER (net.c) ---
// On déclare les fonctions externes du driver
extern int net_send_packet(const uint8_t *packet, uint32_t length);
extern struct net_device *net_get_device(void);

static const uint8_t my_ip[4] = {10, 0, 2, 15};

// --- UTILITAIRES LOCAUX ---
// Memcpy local pour éviter le conflit avec mm.h (qui a les args inversés)
static void net_memcpy(void *dst, const void *src, int n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--) {
        *d++ = *s++;
    }
}

// Checksum IP/ICMP
static uint16_t checksum(uint16_t *data, int words) {
    uint32_t sum = 0;
    while (words--) sum += *data++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

void icmp_init(void) {
    printf("[ICMP] Ready - IP: %d.%d.%d.%d\n",
           my_ip[0], my_ip[1], my_ip[2], my_ip[3]);
}

void icmp_receive(uint8_t *packet, uint32_t len) {
    if (len < 14 + 20 + 8) return;  // Eth + IP + ICMP header min

    uint8_t *ip_header = packet + 14;
    // Vérif simple IPv4 (0x45) et Proto ICMP (1)
    if (ip_header[0] != 0x45) return;  
    if (ip_header[9] != 1) return;     

    /*// Vérif IP Destination (est-ce pour nous ?)
    uint32_t dst_ip = *(uint32_t*)(ip_header + 16);
    uint32_t my_ip_u32 = (my_ip[0]) | (my_ip[1]<<8) | (my_ip[2]<<16) | (my_ip[3]<<24); // Little endian sur ARM/x86 pour les bytes raw
    // Note: Pour simplifier, on ignore la vérif IP stricte ici pour que ça ping facilement
    */
    struct icmp_echo *icmp = (struct icmp_echo*)(ip_header + 20);

    if (icmp->type == ICMP_TYPE_ECHO_REQUEST && icmp->code == 0) {
        // Pour l'affichage, on swap juste pour lire le seq number proprement
        uint16_t seq_num = (icmp->seq << 8) | (icmp->seq >> 8);
        printf("[ICMP] Ping received! (seq=%d)\n", seq_num);

        // Construire la réponse
        uint8_t reply[1536];
        net_memcpy(reply, packet, len);  // copier tout

        // Inverser MAC src/dst
        uint8_t tmp_mac[6];
        net_memcpy(tmp_mac,          reply + 0, 6);  
        net_memcpy(reply + 0,        reply + 6, 6);  
        net_memcpy(reply + 6,        tmp_mac,   6);

        // IP: inverser adresses src/dst
        uint32_t tmp_ip = *(uint32_t*)(reply + 14 + 12);
        *(uint32_t*)(reply + 14 + 12) = *(uint32_t*)(reply + 14 + 16); 
        *(uint32_t*)(reply + 14 + 16) = tmp_ip;

        // ICMP: Request -> Reply
        icmp = (struct icmp_echo*)(reply + 14 + 20);
        icmp->type = ICMP_TYPE_ECHO_REPLY;
        icmp->checksum = 0;
        
        // Recalcul checksum
        int icmp_len = len - 14 - 20;
        icmp->checksum = checksum((uint16_t*)icmp, icmp_len / 2);

        // Envoi via le driver
        net_send_packet(reply, len);
        
        printf("[ICMP] Reply sent.\n");
    }
}

// Fonction générique pour construire une trame Ethernet
// (Renommée pour éviter conflit avec net.c)
void ethernet_send_packet(uint8_t *dst_mac, uint16_t ethertype, uint8_t *payload, uint32_t payload_len) {
    uint8_t frame[1536];
    struct net_device *dev = net_get_device(); // On récupère le device proprement

    // Construction En-tête Ethernet
    net_memcpy(frame + 0,  dst_mac, 6);       // Dst
    net_memcpy(frame + 6,  dev->mac_addr, 6); // Src (NOTRE MAC)
    frame[12] = ethertype >> 8;
    frame[13] = ethertype & 0xff;
    
    // Copie du Payload
    net_memcpy(frame + 14, payload, payload_len);

    // Envoi via le driver
    uint32_t total_len = payload_len + 14;
    if (total_len < 60) total_len = 60; // Padding minimum Ethernet
    
    net_send_packet(frame, total_len);
}