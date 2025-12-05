#include "peripherals/net.h"
#include "peripherals/base.h"
#include "utils.h"
#include "printf.h"
#include <stddef.h>
#include <stdint.h>
#include "net/arp.h"
#include "net/icmp.h"

#define CHAN_CONTROL 0
#define CHAN_RX      1  // Dédie au Bulk IN (Interruption)
#define CHAN_TX      2  // Dédie au Bulk OUT (Bloquant)

// --- GLOBALES RX ---
// Buffer de réception permanent pour le DMA
static uint8_t net_rx_buffer[2048] __attribute__((aligned(16)));


#define CHAN_CONTROL 0
#define CHAN_RX      1  // Dédie au Bulk IN (Interruption)
#define CHAN_TX      2  // Dédie au Bulk OUT (Bloquant)

// --- GLOBALES RX ---
// Buffer de réception permanent pour le DMA
static uint8_t net_rx_buffer[2048] __attribute__((aligned(16)));
static struct net_device net_dev;

// --- VARIABLES GLOBALES POUR LE TOGGLE (CRITIQUE) ---
// Doivent être ici pour ne pas être perdues entre deux appels
static int bulk_in_toggle = 0;
static int bulk_out_toggle = 0;
// ----------------------------------------------------
static uint8_t tx_rndis_buffer[2048] __attribute__((aligned(16)));
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
/////
// Prépare le contrôleur à recevoir un paquet sur le canal RX
void submit_rx_request(void) {
    uint32_t hcchar;
    uint32_t hctsiz;

    // 1. Masquer les interruptions du canal avant modif (sécurité)
    usb_write(HCINTMSK(CHAN_RX), 0);

    // 2. Configurer la taille (Max Packet Size)
    // On demande jusqu'à 1536 bytes (taille frame eth), découpé en paquets de 64 bytes (USB FS/HS)
    // PKTCNT doit être suffisant pour contenir la frame
    uint32_t len = ETH_FRAME_LEN;
    uint32_t pkt_cnt = (len + 63) / 64; 
    
    // DATA0 par défaut pour démarrer (le contrôleur gère souvent le toggle data, 
    // sinon il faudrait tracker data0/1 dans une variable static)
    hctsiz = HCTSIZ_PID_DATA0 | HCTSIZ_PKTCNT(pkt_cnt) | HCTSIZ_XFRSIZ(len);
    usb_write(HCTSIZ(CHAN_RX), hctsiz);

    // 3. Configurer le DMA vers notre buffer statique
    usb_write(HCDMA(CHAN_RX), (uintptr_t)net_rx_buffer);

    // 4. Configurer le Canal (HCCHAR)
    // EPTYPE_BULK, EP_DIR_IN, Device Address, EP Number
    hcchar = HCCHAR_DEVADDR(net_dev.usb_addr) | 
             HCCHAR_EPNUM(EP_BULK_IN) | 
             HCCHAR_EPTYPE(EPTYPE_BULK) | 
             HCCHAR_EPDIR_IN | 
             HCCHAR_MPS(64); // 64 est standard pour FS, 512 pour HS
             
    // 5. IMPORTANT : Activer les interruptions spécifiques pour ce canal
    // On veut être notifié sur : Transfer Complete (Succès), NAK (Rien recu), ou Erreurs
    usb_write(HCINTMSK(CHAN_RX), HCINTMSK_XFRCM | HCINTMSK_NAKM | 
                                 HCINTMSK_STALLM | HCINTMSK_TXERRM | 
                                 HCINTMSK_AHBERRM | HCINTMSK_CHHM);

    // 6. Activer le canal
    usb_write(HCCHAR(CHAN_RX), hcchar | HCCHAR_CHENA);
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
            if (hcint & HCINT_AHBERR){ printf("USB: AHB Error\r\n"); return -1; }
            
            // Si NAK (Device occupé ou pas de données), on retourne un code spécial
            if (hcint & HCINT_NAK)   { return -2; } 
        }
        // Retrait du delay(1) ici pour accélérer le polling réseau
        timeout--;
    }
    return -1;
}

static int usb_control_transfer(uint8_t dev_addr, struct usb_setup_packet *setup, uint8_t *data_buf, uint32_t data_len) {
    uint32_t hcchar;
    uintptr_t dma_addr = (uintptr_t)setup;
    const uint32_t chan_num = 0;
    int is_data_in = (setup->bmRequestType & 0x80) ? 1 : 0;

    usb_write(HCINTMSK(chan_num), 0xFFFFFFFF);

    // 1. SETUP PHASE
    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(0) | 
             HCCHAR_EPTYPE(EPTYPE_CTRL) | HCCHAR_EPDIR_OUT | HCCHAR_MPS(64);
    
    usb_write(HCCHAR(chan_num), hcchar); 
    usb_write(HCTSIZ(chan_num), HCTSIZ_PID_SETUP | HCTSIZ_PKTCNT(1) | HCTSIZ_XFRSIZ(8));
    usb_write(HCDMA(chan_num), dma_addr);
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);

    if (usb_host_wait_xfer_complete(chan_num) < 0) return -1;

    // 2. DATA PHASE
    if (data_len > 0 && data_buf != NULL) {
        uint32_t pid = HCTSIZ_PID_DATA1; 
        
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

        if (usb_host_wait_xfer_complete(chan_num) < 0) return -1;
    }

    // 3. STATUS PHASE
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
    
    net_dev.usb_addr = new_addr;
    delay(20000); 
    return 0;
}

static int usb_get_descriptor(uint8_t addr, int type, int index, void *buf, int size) {
    static struct usb_setup_packet setup;
    setup.bmRequestType = 0x80; 
    setup.bRequest = 0x06;      // GET_DESCRIPTOR
    setup.wValue = (type << 8) | index;
    setup.wIndex = 0;
    setup.wLength = size;
    return usb_control_transfer(addr, &setup, (uint8_t*)buf, size);
}

static int usb_host_init(void) {
    uint32_t reg;
    uint32_t timeout = 10000;

    usb_write(USB_GRXFSIZ, 0x400); 
    usb_write(USB_GNPTXFSIZ, (0x200 << 16) | 0x400); 
    
    reg = usb_read(USB_HPRT); 
    if (!(reg & HPRT_PRTPWR)) {
        reg |= HPRT_PRTPWR;
        usb_write(USB_HPRT, reg); 
        delay(100000); 
    }

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
    
    reg = usb_read(USB_GUSBCFG);
    reg |= USB_GUSBCFG_FHMOD;
    usb_write(USB_GUSBCFG, reg);
    
    usb_write(USB_GINTMSK, 0xFFFFFFFF); 
    
    if (usb_host_init() < 0) return -1;
    return 0;
}
static int usb_set_configuration(uint8_t addr, uint8_t config_value) {
    static struct usb_setup_packet setup;
    setup.bmRequestType = 0x00;
    setup.bRequest = USB_REQ_SET_CONFIGURATION;
    setup.wValue = config_value;
    setup.wIndex = 0;
    setup.wLength = 0;
    
    if (usb_control_transfer(addr, &setup, NULL, 0) < 0) return -1;
    
    printf("USB: Configuration %d active.\r\n", config_value);
    delay(10000);
    return 0;
}

// CORRECTION MAJEURE : Gestion du Toggle et Retour de taille
static int usb_bulk_transfer(uint8_t dev_addr, uint8_t ep_num, uint8_t *buf, uint32_t len, int dir) {
    const uint32_t chan_num = CHAN_TX; 
    uint32_t hcchar;
    uint32_t pid;
    
    usb_write(HCINTMSK(chan_num), 0xFFFFFFFF);

    // GESTION DU TOGGLE DATA0/DATA1
    if (dir == HCCHAR_EPDIR_IN) {
        pid = (bulk_in_toggle) ? HCTSIZ_PID_DATA1 : HCTSIZ_PID_DATA0;
    } else {
        pid = (bulk_out_toggle) ? HCTSIZ_PID_DATA1 : HCTSIZ_PID_DATA0;
    }

    hcchar = HCCHAR_DEVADDR(dev_addr) | HCCHAR_EPNUM(ep_num) | 
             HCCHAR_EPTYPE(EPTYPE_BULK) | dir | HCCHAR_MPS(64);
    
    usb_write(HCCHAR(chan_num), hcchar);

    uint32_t pkt_cnt = (len + 63) / 64;
    if (pkt_cnt == 0) pkt_cnt = 1;

    usb_write(HCTSIZ(chan_num), pid | HCTSIZ_PKTCNT(pkt_cnt) | HCTSIZ_XFRSIZ(len));
    usb_write(HCDMA(chan_num), (uintptr_t)buf);
    usb_write(HCCHAR(chan_num), hcchar | HCCHAR_CHENA);

    int res = usb_host_wait_xfer_complete(chan_num);
    if (res < 0) {
        if (res == -2) return -2; // NAK
        return -1; // Erreur
    }

    // Si succès, on inverse le toggle
    if (dir == HCCHAR_EPDIR_IN) bulk_in_toggle = !bulk_in_toggle;
    else bulk_out_toggle = !bulk_out_toggle;

    // Calcul de la taille réelle reçue
    uint32_t remaining = usb_read(HCTSIZ(chan_num)) & 0x7FFFF;
    return len - remaining;
}
void ethernet_input(uint8_t *packet, uint32_t len) {
    if (len < 14) return;

    uint16_t type = (packet[12] << 8) | packet[13];

    if (type == 0x0806) {
        arp_receive(packet, len);
    } else if (type == 0x0800) {
        uint8_t *ip = packet + 14;
        if (ip[9] == 1) { 
            icmp_receive(packet, len);
        }
    }
}
void handle_usb_irq(void) {
   if (usb_read(USB_GINTSTS) & GINTMSK_HCIM) {
        if (usb_read(USB_HAINT) & (1 << CHAN_RX)) {
            uint32_t hcint = usb_read(HCINT(CHAN_RX));
            usb_write(HCINT(CHAN_RX), hcint); // Ack

            if (hcint & HCINT_XFRC) {
                bulk_in_toggle = !bulk_in_toggle;
                uint32_t len = ETH_FRAME_LEN - (usb_read(HCTSIZ(CHAN_RX)) & HCTSIZ_XFRSIZ_MASK);
                
                // --- FILTRE RNDIS ---
                // Le paquet reçu contient un en-tête RNDIS (44 bytes min). Il faut le sauter.
                if (len > 44) {
                    uint32_t *rndis = (uint32_t*)net_rx_buffer;
                    if (rndis[0] == 2) { // RNDIS Packet
                        uint32_t data_offset = 8 + rndis[2];
                        uint32_t data_len = rndis[3];
                        if (data_len > 0 && (data_offset + data_len <= len)) {
                            // On envoie le vrai paquet Ethernet à l'OS
                            ethernet_input(net_rx_buffer + data_offset, data_len);
                        }
                    }
                }
                submit_rx_request(); // On relance l'écoute
            } else if (hcint & (HCINT_NAK | HCINT_STALL | HCINT_TXERR)) {
                submit_rx_request(); // On relance en cas d'erreur/NAK
            }
        }
    }
    usb_write(USB_GINTSTS, 0xFFFFFFFF); // Ack Global
}
int net_init(void) {
    printf("NET: Starting...\r\n");
    net_dev.state = NET_STATE_DOWN;
    
    // Reset toggles
    bulk_in_toggle = 0;
    bulk_out_toggle = 0;

    if (usb_init() < 0) return -1;
    if (usb_enumerate_set_address(1) < 0) return -1;
    
    static struct usb_device_descriptor desc;
    printf("NET: Reading Device Descriptor...\r\n");
    if (usb_get_descriptor(1, USB_DT_DEVICE, 0, &desc, sizeof(desc)) < 0) return -1;
    
    printf("USB Device: Vendor=0x%x Product=0x%x\r\n", desc.idVendor, desc.idProduct);

    printf("NET: Setting Configuration 1...\r\n");
    if (usb_set_configuration(1, 1) < 0) return -1;
    
    net_dev.state = NET_STATE_UP;
    printf("NET: Ready.\r\n");

    arp_init();
    icmp_init();
    submit_rx_request();
    return 0;
}

struct net_device *net_get_device(void) { return &net_dev; }

int net_send_packet(const uint8_t *packet, uint32_t length) {
    if (net_dev.state != NET_STATE_UP) return -1;
    if (length > ETH_FRAME_LEN) return -2;
    

    ///////////// added 
    uint32_t *header = (uint32_t *)tx_rndis_buffer;
    
    // 1. Remplissage de l'en-tête (4 entiers de 32 bits)
    header[0] = 0x00000002;        // REMOTE_NDIS_PACKET_MSG
    header[1] = 44 + length;       // Longueur Totale (Header + Data)
    header[2] = 36;                // Data Offset (depuis l'index 2) -> 36+8=44 bytes
    header[3] = length;            // Data Length (Taille du paquet Ethernet)
    
    // 2. Nettoyage du padding (les octets entre le header et les données)
    // Le header fait 16 octets (4*4), on doit aller jusqu'à 44.
    // Donc on met des zéros de l'index 16 à 44.
    uint8_t *pad_ptr = (uint8_t*)tx_rndis_buffer + 16;
    for(int i=0; i< (44-16); i++) *pad_ptr++ = 0;
    //////////////

    // 3. Copie du paquet Ethernet APRES les 44 octets d'en-tête
    // Note: On utilise une copie manuelle car on n'a pas memcpy ici
    uint8_t *dest = (uint8_t*)tx_rndis_buffer + 44;
    const uint8_t *src = packet;
    for(uint32_t i=0; i<length; i++) dest[i] = src[i];
    ////////////////

    if (usb_bulk_transfer(net_dev.usb_addr, EP_BULK_OUT, (uint8_t*)packet, length, HCCHAR_EPDIR_OUT) < 0) {
        net_dev.tx_errors++;
        return -1;
    }
    
    net_dev.tx_packets++;
    return 0;
}

int net_receive_packet(uint8_t *packet, uint32_t max_length) {
    if (net_dev.state != NET_STATE_UP) return -1;

    int res = usb_bulk_transfer(net_dev.usb_addr, EP_BULK_IN, packet, max_length, HCCHAR_EPDIR_IN);
    
    if (res < 0) return 0; // NAK ou Erreur
    
    net_dev.rx_packets++;
    return res; // Retourne la vraie taille !
}

// Stubs
int net_link_status(void) { return 1; }
void net_get_mac_address(uint8_t *mac) { }
int net_set_state(int enable) { return 0; }
void net_print_stats(void) { }
void net_irq_handler(void) { }

