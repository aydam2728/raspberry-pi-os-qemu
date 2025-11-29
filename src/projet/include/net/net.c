#include "peripherals/net.h"
#include "peripherals/base.h"
#include "utils.h"
#include "printf.h"
#include <stddef.h>
#include <stdint.h>
#include "arp.h" //////// added 
#include "icmp.h" /////////// added
static struct net_device net_dev;
static uint8_t default_mac[ETH_ALEN] = {0xB8, 0x27, 0xEB, 0x00, 0x00, 0x01};

/////////////////////////////



// Structure alignée sur 16 octets pour le DMA
struct usb_setup_packet {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed, aligned(16)));

// --- Helpers ---
static inline uint32_t usb_read(uint32_t reg) {
    return *(volatile uint32_t *)(USB_BASE + reg);
}
static inline void usb_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(USB_BASE + reg) = value;
}

// --- DWC2 Functions ---

static int usb_core_reset(void) {
    uint32_t timeout = 10000;
    while (!(usb_read(USB_GRSTCTL) & USB_GRSTCTL_AHBIDLE)) {
        if (--timeout == 0) return -1;
        delay(10);
    }
    usb_write(USB_GRSTCTL, USB_GRSTCTL_CSFTRST);
    timeout = 10000;
    while (usb_read(USB_GRSTCTL) & USB_GRSTCTL_CSFTRST) {
        if (--timeout == 0) return -1;
        delay(10);
    }
    delay(10000); 
    return 0;
}

static int usb_host_wait_xfer_complete(uint32_t chan_num) {
    uint32_t timeout = 100000; 
    uint32_t hcint_reg = HCINT(chan_num); 
    volatile uint32_t hcint;

    while (timeout > 0) {
        hcint = usb_read(hcint_reg);

        if (hcint) {
            usb_write(hcint_reg, hcint); // Ack

            if (hcint & HCINT_XFRC) return 0; // Success
            
            if (hcint & HCINT_CHH) return 0; // Halt is ok
            
            if (hcint & HCINT_STALL) { printf("USB: STALL\r\n"); return -1; }
            if (hcint & HCINT_TXERR) { printf("USB: TXERR\r\n"); return -1; }
            if (hcint & HCINT_NAK)   { printf("USB: NAK\r\n"); return -1; }
            if (hcint & HCINT_AHBERR){ printf("USB: AHB Error\r\n"); return -1; }
        }
        delay(1);
        timeout--;
    }
    printf("USB: Timeout (HCINT=0x%x)\r\n", usb_read(hcint_reg));
    return -1;
}

static int usb_control_transfer(uint8_t dev_addr, struct usb_setup_packet *setup, uint8_t *data_buf, uint32_t data_len) {
    uint32_t hcchar;
    uintptr_t dma_addr = (uintptr_t)setup;
    const uint32_t chan_num = 0;
    int is_data_in = (setup->bmRequestType & 0x80) ? 1 : 0;

    // Unmask Interrupts
    usb_write(HCINTMSK(chan_num), 0xFFFFFFFF);

    // 1. SETUP PHASE
    // printf("USB: SETUP...\r\n");
    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | 
             HCCHAR_EPTYPE(EPTYPE_CTRL) | HCCHAR_EPDIR_OUT | HCCHAR_MPS(64);
    
    usb_write(HCCHAR(chan_num), hcchar); 
    usb_write(HCTSIZ(chan_num), HCTSIZ_PID_SETUP | HCTSIZ_PKTCNT(1) | HCTSIZ_XFRSIZ(8));
    usb_write(HCDMA(chan_num), dma_addr);
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);

    if (usb_host_wait_xfer_complete(chan_num) < 0) return -1;

    // 2. DATA PHASE
    if (data_len > 0 && data_buf != NULL) {
        // printf("USB: DATA...\r\n");
        uint32_t pid = HCTSIZ_PID_DATA1; // First data packet is always DATA1
        
        if (is_data_in) {
             hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | 
                      HCCHAR_EPTYPE(EPTYPE_CTRL) | HCCHAR_EPDIR_IN | HCCHAR_MPS(64);
        } else {
             hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | 
                      HCCHAR_EPTYPE(EPTYPE_CTRL) | HCCHAR_EPDIR_OUT | HCCHAR_MPS(64);
        }

        usb_write(HCCHAR(chan_num), hcchar);
        
        uint32_t pkt_cnt = (data_len + 63) / 64;
        if (pkt_cnt == 0) pkt_cnt = 1;
        
        usb_write(HCTSIZ(chan_num), pid | HCTSIZ_PKTCNT(pkt_cnt) | HCTSIZ_XFRSIZ(data_len));
        usb_write(HCDMA(chan_num), (uintptr_t)data_buf);
        usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);

        if (usb_host_wait_xfer_complete(chan_num) < 0) {
            printf("USB: Data Phase Failed\r\n");
            return -1;
        }
    }

    // 3. STATUS PHASE
    // printf("USB: STATUS...\r\n");
    // If Data was IN, Status is OUT (and vice versa). If no Data, Setup was OUT, so Status is IN.
    uint32_t status_dir = (data_len > 0 && is_data_in) ? HCCHAR_EPDIR_OUT : HCCHAR_EPDIR_IN;
    
    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | 
             HCCHAR_EPTYPE(EPTYPE_CTRL) | status_dir | HCCHAR_MPS(64);
             
    usb_write(HCCHAR(chan_num), hcchar);
    usb_write(HCTSIZ(chan_num), HCTSIZ_PID_DATA1 | HCTSIZ_PKTCNT(1) | HCTSIZ_XFRSIZ(0));
    usb_write(HCDMA(chan_num), 0); 
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);

    if (usb_host_wait_xfer_complete(chan_num) < 0) return -1;

    return 0;
}

static int usb_enumerate_set_address(uint8_t new_addr) {
    static struct usb_setup_packet setup;
    
    setup.bmRequestType = 0x00; 
    setup.bRequest = 0x05;      // SET_ADDRESS
    setup.wValue = new_addr;
    setup.wIndex = 0;
    setup.wLength = 0;
    
    if (usb_control_transfer(0, &setup, NULL, 0) < 0) return -1;
    
    printf("USB: Address set to %d.\r\n", new_addr);
    net_dev.usb_addr = new_addr;
    delay(20000); 
    return 0;
}

static int usb_get_descriptor(uint8_t addr, int type, int index, void *buf, int size) {
    static struct usb_setup_packet setup;
    
    setup.bmRequestType = 0x80; // Device to Host (IN)
    setup.bRequest = 0x06;      // GET_DESCRIPTOR
    setup.wValue = (type << 8) | index;
    setup.wIndex = 0;
    setup.wLength = size;
    
    return usb_control_transfer(addr, &setup, (uint8_t*)buf, size);
}

static int usb_host_init(void) {
    uint32_t reg;
    uint32_t timeout = 10000;

    // Init FIFO
    usb_write(USB_GRXFSIZ, 0x400); 
    usb_write(USB_GNPTXFSIZ, (0x200 << 16) | 0x400); 
    
    // Power On Port
    reg = usb_read(USB_HPRT); 
    if (!(reg & HPRT_PRTPWR)) {
        reg |= HPRT_PRTPWR;
        usb_write(USB_HPRT, reg); 
        delay(100000); 
    }

    // Reset Port
    reg = usb_read(USB_HPRT); 
    reg |= HPRT_PRTRST;
    usb_write(USB_HPRT, reg); 
    delay(100000); 
    
    reg &= ~HPRT_PRTRST;
    usb_write(USB_HPRT, reg);
    delay(50000);
    
    while(usb_read(USB_HPRT) & HPRT_PRTRST) {
         if (--timeout == 0) return -1;
         delay(10);
    }
    
    // Check
    if (usb_read(USB_HPRT) & HPRT_PRTCONNS) { 
        printf("USB: Device detected.\r\n");
    } else {
        printf("USB: No device. HPRT=0x%x\r\n", usb_read(USB_HPRT));
        return -1;
    }
    return 0;
}

static int usb_init(void) {
    uint32_t reg;
    printf("USB: Init...\r\n");
    if (usb_core_reset() < 0) return -1;
    
    reg = USB_GAHBCFG_GLBL_INTR_EN | USB_GAHBCFG_HBSTLEN_INCR4 | USB_GAHBCFG_DMA_EN;
    usb_write(USB_GAHBCFG, reg);
    
    // Force Host Mode
    reg = usb_read(USB_GUSBCFG);
    reg |= USB_GUSBCFG_FHMOD;
    usb_write(USB_GUSBCFG, reg);
    
    // Unmask Global Interrupts
    usb_write(USB_GINTMSK, 0xFFFFFFFF); 
    
    if (usb_host_init() < 0) return -1;
    return 0;
}
static int usb_set_configuration(uint8_t addr, uint8_t config_value) {
    static struct usb_setup_packet setup;
    
    setup.bmRequestType = 0x00; // Host to Device, Standard, Device
    setup.bRequest = USB_REQ_SET_CONFIGURATION;
    setup.wValue = config_value;
    setup.wIndex = 0;
    setup.wLength = 0;
    
    if (usb_control_transfer(addr, &setup, NULL, 0) < 0) return -1;
    
    printf("USB: Configuration %d active.\r\n", config_value);
    delay(10000); // Laisser le temps au device de s'activer
    return 0;
}
/*
 * Transfère des données via un Endpoint BULK.
 * dir: HCCHAR_EPDIR_IN ou HCCHAR_EPDIR_OUT
 */
static int usb_bulk_transfer(uint8_t dev_addr, uint8_t ep_num, uint8_t *buf, uint32_t len, int dir) {
    const uint32_t chan_num = 1; // On utilise le Canal 1 pour le Bulk (Canal 0 réservé au Control)
    uint32_t hcchar;
    uint32_t pid;
    
    // Démasquer les interruptions pour ce canal aussi
    usb_write(HCINTMSK(chan_num), 0xFFFFFFFF);

    // 1. Configuration HCCHAR
    // Note: EPTYPE_BULK = 2
    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(ep_num) | 
             HCCHAR_EPTYPE(EPTYPE_BULK) | dir | HCCHAR_MPS(64);
    
    usb_write(HCCHAR(chan_num), hcchar);

    // 2. Configuration HCTSIZ
    // Pour le Bulk, on commence généralement avec DATA0. 
    // (Dans un vrai driver complet, il faudrait gérer le "Data Toggle" bit)
    pid = HCTSIZ_PID_DATA0; 
    
    uint32_t pkt_cnt = (len + 63) / 64;
    if (pkt_cnt == 0) pkt_cnt = 1;

    usb_write(HCTSIZ(chan_num), pid | HCTSIZ_PKTCNT(pkt_cnt) | HCTSIZ_XFRSIZ(len));
    
    // 3. Configuration DMA
    usb_write(HCDMA(chan_num), (uintptr_t)buf);

    // 4. Activation
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);

    // 5. Attente
    if (usb_host_wait_xfer_complete(chan_num) < 0) {
        printf("USB: Bulk Transfer Failed (EP%d)\r\n", ep_num);
        return -1;
    }

    return 0;
}
int net_init(void) {
    printf("NET: Starting...\r\n");
    net_dev.state = NET_STATE_DOWN;
    
    if (usb_init() < 0) {
        printf("NET: USB Init Failed\r\n");
        return -1;
    }
    
    // 1. Set Address
    if (usb_enumerate_set_address(1) < 0) {
        printf("NET: Enum Failed\r\n");
        return -1;
    }
    
    // 2. Get Descriptor
    static struct usb_device_descriptor desc;
    printf("NET: Reading Device Descriptor...\r\n");
    
    if (usb_get_descriptor(1, USB_DT_DEVICE, 0, &desc, sizeof(desc)) < 0) {
         printf("NET: Failed to read descriptor\r\n");
         return -1;
    }
    
    printf("USB Device: Vendor=0x%x Product=0x%x\r\n", desc.idVendor, desc.idProduct);

    // 3. Set Configuration (Activation)
    // On active la configuration 1 (standard pour la plupart des périphériques simples)
    printf("NET: Setting Configuration 1...\r\n");
    if (usb_set_configuration(1, 1) < 0) {
        printf("NET: Failed to set configuration\r\n");
        return -1;
    }
    
    net_dev.state = NET_STATE_UP;
    printf("NET: Ready.\r\n");

    arp_init();
    icmp_init();
    return 0;
}

struct net_device *net_get_device(void) { return &net_dev; }
int net_send_packet(const uint8_t *packet, uint32_t length) {
    if (net_dev.state != NET_STATE_UP) {
        printf("NET: Error, device not ready.\r\n");
        return -1;
    }
    
    if (length > ETH_FRAME_LEN) return -2;
    
    // printf("NET: Sending %d bytes...\r\n", length);
    
    // Envoi via Endpoint 2 (OUT) vers l'adresse USB 1
    // Note: On cast le const away pour le DMA, mais c'est une lecture seule pour le contrôleur en mode OUT
    if (usb_bulk_transfer(net_dev.usb_addr, EP_BULK_OUT, (uint8_t*)packet, length, HCCHAR_EPDIR_OUT) < 0) {
        net_dev.tx_errors++;
        return -1;
    }
    
    net_dev.tx_packets++;
    printf("NET: Packet Sent!\r\n");
    return 0;
}
/* Receive packet */
int net_receive_packet(uint8_t *packet, uint32_t max_length) {
    if (net_dev.state != NET_STATE_UP) return -1;

    // Tenter une lecture sur l'Endpoint 1 (IN)
    // Note: usb_bulk_transfer renvoie -1 en cas de NAK (pas de données) ou Timeout
    // Dans un vrai OS, cela serait géré par interruption, ici on "poll".
    if (usb_bulk_transfer(net_dev.usb_addr, EP_BULK_IN, packet, max_length, HCCHAR_EPDIR_IN) < 0) {
        return 0; // Pas de paquet reçu (ou erreur silencieuse)
    }

    // Si on arrive ici, le transfert a réussi, mais on ne connaît pas la taille exacte reçue
    // car notre usb_bulk_transfer simplifié ne retourne pas bytes_transferred.
    // Pour ce test, on suppose qu'on a reçu quelque chose.
    
    net_dev.rx_packets++;
    return 1; // 1 paquet reçu (taille inconnue dans cette implémentation simplifiée)
}
int net_link_status(void) { return 1; }
void net_get_mac_address(uint8_t *mac) { }
int net_set_state(int enable) { return 0; }
void net_print_stats(void) { }
void net_irq_handler(void) { }

void ethernet_input(uint8_t *packet, uint32_t len) {
    if (len < 14) return;

    uint16_t type = (packet[12] << 8) | packet[13];

    if (type == 0x0806) {
        arp_receive(packet, len);
    } else if (type == 0x0800) {
        // On ne traite que ICMP pour l’instant
        uint8_t *ip = packet + 14;
        if (ip[9] == 1) {  // protocol == ICMP
            icmp_receive(packet, len);
        }
    }
}