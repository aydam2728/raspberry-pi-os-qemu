#include "peripherals/net.h"
#include "peripherals/usb.h"
#include "net/rndis.h"
#include "utils.h"
#include "printf.h"
#include "net/arp.h"
#include "net/icmp.h"

// Fonctions des autres modules
extern void net_rx_set_device(struct net_device *dev);
extern void net_rx_reset_toggle(void);
extern void submit_rx_request(void);
extern void net_tx_set_device(struct net_device *dev);

static struct net_device net_dev;

int net_init(void) {
    printf("==================================================\r\n");
    printf("NET: Universal Driver (RPi3 Hardware + QEMU)\r\n");
    printf("==================================================\r\n");
    
    // Initialiser la structure
    net_dev.state = NET_STATE_DOWN;
    net_dev.usb_addr = 0;
    net_dev.rx_packets = 0;
    net_dev.tx_packets = 0;
    net_dev.rx_errors = 0;
    net_dev.tx_errors = 0;
    
    // Passer le pointeur aux modules
    net_rx_set_device(&net_dev);
    net_rx_reset_toggle();
    net_tx_set_device(&net_dev);
    
    // Step 1: USB Init
    printf("NET: Step 1 - USB Host Init\r\n");
    if (usb_init() < 0) {
        printf("USB: Init FAILED!\r\n");
        return -1;
    }
    printf("USB: Init OK\r\n");
    
    // Step 2: Set Address
    printf("NET: Step 2 - Set Address\r\n");
    if (usb_enumerate_set_address(1) < 0) {
        printf("USB: SetAddress FAILED!\r\n");
        return -1;
    }
    net_dev.usb_addr = 1;
    
    // Step 3: Set Configuration
    printf("NET: Step 3 - Set Configuration\r\n");
    if (usb_set_configuration(1, 1) < 0) {
        printf("USB: SetConfig FAILED!\r\n");
        return -1;
    }
    
    // Step 4: Détection Hardware/QEMU
    printf("NET: Step 4 - Detecting hardware type...\r\n");
    printf("NET: Trying RNDIS init (works on real hardware)...\r\n");
    
    if (rndis_init_device(net_dev.usb_addr) == 0) {
        // VRAI HARDWARE
        printf("NET: *** REAL HARDWARE DETECTED (RNDIS working) ***\r\n");
        rndis_set_hardware_mode(1);
    } else {
        // QEMU
        printf("NET: *** QEMU DETECTED (RNDIS failed) ***\r\n");
        printf("NET: Continuing in degraded mode...\r\n");
        rndis_set_hardware_mode(0);
    }
    
    net_dev.state = NET_STATE_UP;
    
    // MAC address
    net_dev.mac_addr[0] = 0x52;
    net_dev.mac_addr[1] = 0x54;
    net_dev.mac_addr[2] = 0x00;
    net_dev.mac_addr[3] = 0x12;
    net_dev.mac_addr[4] = 0x34;
    net_dev.mac_addr[5] = 0x56;
    
    printf("NET: MAC = %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           net_dev.mac_addr[0], net_dev.mac_addr[1], net_dev.mac_addr[2],
           net_dev.mac_addr[3], net_dev.mac_addr[4], net_dev.mac_addr[5]);
    
    // Protocoles
    arp_init(); 
    icmp_init();
    
    // Step 5: RX selon le mode
    if (rndis_is_hardware_mode()) {
        printf("NET: Step 5 - Submit RX request\r\n");
        submit_rx_request();
    } else {
        printf("NET: Step 5 - RX disabled (QEMU mode)\r\n");
        
        // Désactiver IRQ USB pour QEMU
        usb_write(USB_GINTMSK, 0);
        usb_write(USB_HAINTMSK, 0);
    }
    
    printf("==================================================\r\n");
    if (rndis_is_hardware_mode()) {
        printf("NET: READY (REAL HARDWARE MODE)\r\n");
    } else {
        printf("NET: READY (QEMU FALLBACK MODE - RX disabled)\r\n");
    }
    printf("==================================================\r\n");
    
    return 0;
}

struct net_device *net_get_device(void) {
    return &net_dev;
}

// Stubs pour compatibilité
int net_receive_packet(uint8_t *packet, uint32_t max_length) { return 0; }
int net_link_status(void) { return 1; }

void net_get_mac_address(uint8_t *mac) {
    for (int i = 0; i < 6; i++) mac[i] = net_dev.mac_addr[i];
}

int net_set_state(int enable) { return 0; }

void net_print_stats(void) {
    printf("NET: RX=%u TX=%u (Mode: %s)\r\n", 
           net_dev.rx_packets, net_dev.tx_packets,
           rndis_is_hardware_mode() ? "HARDWARE" : "QEMU");
}