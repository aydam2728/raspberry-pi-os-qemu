// kernel/net/icmp.c
#include "icmp.h"
#include "arp.h"
#include "printf.h"
#include "string.h"

extern struct net_device netdev;  // ton device dans net.c

static const uint8_t my_ip[4] = {192, 168, 1, 42};

// Checksum IP/ICMP (standard 1's complement)
static uint16_t checksum(uint16_t *data, int words) {
    uint32_t sum = 0;
    while (words--) sum += *data++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

void icmp_init(void) {
    printf("[ICMP] Ready - will reply to ping on %d.%d.%d.%d\n",
           my_ip[0], my_ip[1], my_ip[2], my_ip[3]);
}

void icmp_receive(uint8_t *packet, uint32_t len) {
    if (len < 14 + 20 + 8) return;  // Eth + IP + ICMP header min

    uint8_t *ip_header = packet + 14;
    if (ip_header[0] != 0x45) return;  // IPv4, no options
    if (ip_header[9] != 1) return;     // protocol != ICMP

    uint32_t dst_ip = *(uint32_t*)(ip_header + 16);
    if (dst_ip != ((my_ip[0]<<24)|(my_ip[1]<<16)|(my_ip[2]<<8)|my_ip[3]))
        return;

    struct icmp_echo *icmp = (struct icmp_echo*)(ip_header + 20);

    if (icmp->type == ICMP_TYPE_ECHO_REQUEST && icmp->code == 0) {
        printf("[ICMP] Ping received! (seq=%d)\n", __builtin_bswap16(icmp->seq));

        // Construire la réponse
        uint8_t reply[1536];
        memcpy(reply, packet, len);  // copier tout (eth + ip + icmp + data)

        // Inverser MAC src/dst
        uint8_t tmp_mac[6];
        memcpy(tmp_mac,          reply + 0, 6);  // ancien dst → nouveau src
        memcpy(reply + 0,        reply + 6, 6);  // ancien src → nouveau dst
        memcpy(reply + 6,        tmp_mac,   6);

        // Ethernet type reste 0x0800
        reply[12] = 0x08; reply[13] = 0x00;

        // IP: inverser adresses
        uint32_t tmp_ip = *(uint32_t*)(reply + 14 + 12);  // src IP
        *(uint32_t*)(reply + 14 + 12) = *(uint32_t*)(reply + 14 + 16);  // dst devient src
        *(uint32_t*)(reply + 14 + 16) = tmp_ip;

        // ICMP: Echo Request → Echo Reply
        icmp = (struct icmp_echo*)(reply + 14 + 20);
        icmp->type = ICMP_TYPE_ECHO_REPLY;
        icmp->checksum = 0;
        int icmp_len = len - 14 - 20;
        icmp->checksum = checksum((uint16_t*)icmp, icmp_len / 2);

        // Envoi
        extern void usbnet_send(uint8_t *data, int len);
        usbnet_send(reply, len);

        printf("[ICMP] Ping replied!\n");
    }
}

// Fonction générique d’envoi (utilisée par ARP et ICMP)
void net_send_packet(uint8_t *dst_mac, uint16_t ethertype, uint8_t *payload, uint32_t payload_len) {
    uint8_t frame[1536];
    struct net_device *dev = &netdev;

    memcpy(frame + 0,  dst_mac, 6);
    memcpy(frame + 6,  dev->mac_addr, 6);
    frame[12] = ethertype >> 8;
    frame[13] = ethertype & 0xff;
    memcpy(frame + 14, payload, payload_len);

    extern void usbnet_send(uint8_t *data, int len);
    usbnet_send(frame, payload_len + 14 < 60 ? 60 : payload_len + 14);
}