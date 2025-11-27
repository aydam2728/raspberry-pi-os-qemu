#include "peripherals/net.h"
#include "peripherals/base.h"
#include "utils.h"
#include "printf.h"
#include <stddef.h>
#include <stdint.h>


// --- Variables Globales ---

static struct net_device net_dev;

/* Default MAC address */
static uint8_t default_mac[ETH_ALEN] = {0xB8, 0x27, 0xEB, 0x00, 0x00, 0x01};

// --- Structures USB ---

// Structure simplifiée du Paquet Setup (8 octets)
// AJOUT : __attribute__((aligned(4))) pour s'assurer que l'adresse est compatible DMA
struct usb_setup_packet {
    uint8_t bmRequestType;  // Type et Direction de la requête (D->H ou H->D)
    uint8_t bRequest;       // Code de la requête (ex: 0x05 pour SET_ADDRESS)
    uint16_t wValue;        // Valeur (ex: la nouvelle adresse)
    uint16_t wIndex;        // Index (souvent 0)
    uint16_t wLength;       // Longueur des données à transférer (0 pour SET_ADDRESS)
} __attribute__((packed, aligned(4))); 


// --- Fonctions d'Accès aux Registres ---

static inline uint32_t usb_read(uint32_t reg) {
    return *(volatile uint32_t *)(USB_BASE + reg);
}

static inline void usb_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(USB_BASE + reg) = value;
}


// --- Fonctions DWC2 Core et Host ---

/* USB Core Reset */
static int usb_core_reset(void) {
    uint32_t timeout = 10000;
    
    /* Wait for AHB master to be idle */
    while (!(usb_read(USB_GRSTCTL) & USB_GRSTCTL_AHBIDLE)) {
        if (--timeout == 0) {
            printf("USB: Timeout waiting for AHB idle\r\n");
            return -1;
        }
        delay(10);
    }
    
    /* Perform core soft reset */
    usb_write(USB_GRSTCTL, USB_GRSTCTL_CSFTRST);
    timeout = 10000;
    
    while (usb_read(USB_GRSTCTL) & USB_GRSTCTL_CSFTRST) {
        if (--timeout == 0) {
            printf("USB: Timeout waiting for core reset\r\n");
            return -1;
        }
        delay(10);
    }
    
    /* Wait a bit after reset */
    delay(1000);
    
    return 0;
}

/* Attend la fin d'un transfert sur un canal (polling). */
static int usb_host_wait_xfer_complete(uint32_t chan_num) {
    uint32_t timeout = 100000; // Augmenté pour sécurité
    // Utilisation des macros HCINT(n) et HCCHAR(n)
    uint32_t hcint_reg = HCCHAR(chan_num) + 0x04; // HCINTn
    uint32_t hcint;

    while (timeout > 0) {
        hcint = usb_read(hcint_reg);

        // Si le transfert est terminé (succès ou erreur)
        if (hcint & (HCINT_XFRC | HCINT_CHH | HCINT_AHBERR)) {
            // Acquittement de l'interruption (Clear on read/write)
            usb_write(hcint_reg, hcint);
            
            if (hcint & HCINT_XFRC) {
                return 0; // Succès (Transfer Complete)
            }
            if (hcint & (HCINT_CHH | HCINT_AHBERR)) {
                printf("USB: Transfert échoué sur HC%d. HCINT=0x%x\r\n", chan_num, hcint);
                return -1; // Échec
            }
        }
        delay(10);
        timeout--;
    }

    printf("USB: Timeout d'attente sur HC%d.\r\n", chan_num);
    return -1;
}

/*
 * Exécute un transfert de contrôle USB sur HC0 (Endpoint 0).
 * data_buf : Buffer pour les données (NULL pour Setup/Status).
 * data_len : Longueur des données.
 * dev_addr : Adresse USB du périphérique (0 au début de l'énumération).
 */
static int usb_control_transfer(uint8_t dev_addr, struct usb_setup_packet *setup, uint8_t *data_buf, uint32_t data_len) {
    uint32_t hcchar;
    uintptr_t dma_addr;
    const uint32_t chan_num = 0; // Utilise toujours le Canal 0 pour EP0
    
    // --- Phase 1 : SETUP (Toujours OUT) ---
    
    printf("USB: Phase SETUP (Addr=%d)...\r\n", dev_addr);
    
    // Conversion d'adresse pour le DMA
    dma_addr = (uintptr_t)setup;

    // 1. Configurer HCCHAR0 (Caractéristiques du Canal 0)
    // CORRECTION ICI : Utilisation des bonnes macros de décalage
    hcchar = HCCHAR_DEVADDR(dev_addr) |      // Bits 22-28
             HCCHAR_EPNUM(0) |               // Bits 11-14 (EP0)
             HCCHAR_EPTYPE(EPTYPE_CTRL) |    // Bits 18-19 (Control)
             HCCHAR_EPDIR_OUT |              // Bit 15 (OUT)
             HCCHAR_MPS(64);                 // Bits 0-10 (Max Packet 64)
    
    // Note: On n'active pas encore (CHENA), on écrit la config d'abord
    usb_write(HCCHAR(chan_num), hcchar);

    // 2. Configurer HCTSIZ0 (Taille du Transfert)
    uint32_t hctsiz_setup = HCTSIZ_PID_SETUP |  // PID : Setup Packet
                            HCTSIZ_PKTCNT(1) |  // 1 paquet
                            HCTSIZ_XFRSIZ(8);   // 8 octets
    
    usb_write(HCTSIZ(chan_num), hctsiz_setup);
    usb_write(HCDMA(chan_num), dma_addr);

    // 3. Activer le Canal
    hcchar |= HCCHAR_CHENA;
    usb_write(HCCHAR(chan_num), hcchar);

    if (usb_host_wait_xfer_complete(chan_num) < 0) {
        printf("USB: Échec Phase SETUP.\r\n");
        return -1;
    }

    // --- Phase 2 : DATA (Non implémentée pour SET_ADDRESS) ---
    if (data_len > 0) {
        printf("USB: Phase DATA non implémentée.\r\n");
        return -1;
    }
    
    // --- Phase 3 : STATUS (IN pour un Setup OUT) ---
    
    printf("USB: Phase STATUS...\r\n");
    
    // 1. Configurer HCCHAR0 pour STATUS IN (réception ZLP)
    hcchar = HCCHAR_DEVADDR(dev_addr) |
             HCCHAR_EPNUM(0) |
             HCCHAR_EPTYPE(EPTYPE_CTRL) |
             HCCHAR_EPDIR_IN |               // CORRECTION : IN est au bit 15
             HCCHAR_MPS(64);
             
    usb_write(HCCHAR(chan_num), hcchar);

    // 2. Configurer HCTSIZ0 pour la phase STATUS (paquet ZLP)
    // Utiliser PID_DATA1 (toggle data)
    uint32_t hctsiz_status = HCTSIZ_PID_DATA1 |  
                             HCTSIZ_PKTCNT(1) |
                             HCTSIZ_XFRSIZ(0);   // 0 octet (ZLP)
    
    usb_write(HCTSIZ(chan_num), hctsiz_status);
    // HCDMA n'a pas d'importance pour un ZLP IN, mais on peut le laisser à 0 ou pointer sur un buffer dummy
    usb_write(HCDMA(chan_num), 0);

    // 3. Activer le Canal
    hcchar |= HCCHAR_CHENA;
    usb_write(HCCHAR(chan_num), hcchar);

    if (usb_host_wait_xfer_complete(chan_num) < 0) {
        printf("USB: Échec Phase STATUS.\r\n");
        return -1;
    }
    
    printf("USB: Transfert de contrôle terminé avec succès.\r\n");
    return 0;
}

/* Attribue une nouvelle adresse USB au périphérique (initialement à 0) */
static int usb_enumerate_set_address(uint8_t new_addr) {
    // Paquet Setup pour SET_ADDRESS
    struct usb_setup_packet setup = {
        .bmRequestType = 0x00,      // Host to Device, Standard, Device
        .bRequest      = 0x05,      // SET_ADDRESS
        .wValue        = new_addr,  // La nouvelle adresse
        .wIndex        = 0x0000,
        .wLength       = 0x0000
    };
    
    // Le transfert SET_ADDRESS doit être envoyé à l'adresse 0 (l'adresse par défaut)
    if (usb_control_transfer(0, &setup, NULL, 0) < 0) {
        return -1;
    }
    
    // Après le succès du transfert, le périphérique prend sa nouvelle adresse
    printf("USB: Adresse 0x%x attribuée avec succès.\r\n", new_addr);
    // Note: Vous devrez stocker cette adresse dans la structure net_dev pour les futurs transferts.
    
    return 0;
}


/*
 * Initialise le DWC2 en mode Hôte, allume le port et effectue le reset.
 */
static int usb_host_init(void) {
    uint32_t reg;
    uint32_t timeout = 10000;

    // --- FIFO Configuration ---
    
    // GRXFSIZ (Receive FIFO Size Register)
    usb_write(USB_GRXFSIZ, 0x200); // (2KB)
    
    // GNPTXFSIZ (Non-Periodic Transmit FIFO Size Register)
    usb_write(USB_GNPTXFSIZ, (0x100 << 16) | 0x200); // Start 0x200, Size 0x100
    
    // --- HCFG Configuration (Host Configuration Register) ---
    
    reg = usb_read(USB_HCFG);
    // Ici, vous pourriez configurer la vitesse et le bus Host si nécessaire.
    usb_write(USB_HCFG, reg);

    // --- Activation du Port et Réinitialisation (HPRT) ---
    
    // 1. Port VBUS Power On
    reg = usb_read(USB_HPRT); 
    reg |= HPRT_PRTPWR;
    usb_write(USB_HPRT, reg); 
    delay(50000); // Wait 50ms+ for power stabilization

    // CRITIQUE : Nettoyage des indicateurs de changement AVANT le reset
    reg = usb_read(USB_HPRT);
    reg |= (HPRT_PRTCONCHG | HPRT_PRTENCHG | HPRT_PRTOVRCURRCHG | HPRT_PRTRSTCHG);
    usb_write(USB_HPRT, reg);
    
    // 2. Port Reset
    reg = usb_read(USB_HPRT); 
    reg |= HPRT_PRTRST;
    usb_write(USB_HPRT, reg); 
    delay(50000); // Wait 50ms+ (USB Specs + Marge QEMU)
    
    // 3. End Reset
    reg = usb_read(USB_HPRT);
    reg &= ~HPRT_PRTRST;
    usb_write(USB_HPRT, reg);
    
    // 4. Waiting for Port Reset completion
    timeout = 10000;
    while(usb_read(USB_HPRT) & HPRT_PRTRST) {
         if (--timeout == 0) {
            printf("USB: Timeout waiting for Port Reset completion\r\n");
            return -1;
        }
        delay(10);
    }
    
    // CRITIQUE : Nettoyage des indicateurs de changement APRÈS le reset
    // Cela débloque souvent la mise à jour du bit PRTCONNS sous QEMU
    reg = usb_read(USB_HPRT);
    reg |= (HPRT_PRTCONCHG | HPRT_PRTENCHG | HPRT_PRTOVRCURRCHG | HPRT_PRTRSTCHG);
    usb_write(USB_HPRT, reg);

    // Petit délai pour laisser le statut se stabiliser
    delay(1000);

    // 5. Check Connection
    if (usb_read(USB_HPRT) & HPRT_PRTCONNS) { 
        // Attendre un tout petit peu que le port s'active (PRTENA)
        int retries = 100;
        while (!(usb_read(USB_HPRT) & HPRT_PRTENA) && retries-- > 0) delay(100);
        
        printf("USB: LAN9514 detected. Port ready.\r\n");
    } else {
        uint32_t debug_hprt = usb_read(USB_HPRT);
        printf("USB: No device detected after port reset. HPRT=0x%x\r\n", debug_hprt);
        return -1;
    }

    return 0;
}

/* Initialize USB controller */
static int usb_init(void) {
    uint32_t reg;
    
    printf("USB: Initializing USB controller...\r\n");
    
    
    if (usb_core_reset() < 0) {
        return -1;
    }
    
    /* Configure AHB */
    reg = usb_read(USB_GAHBCFG);
    reg |= USB_GAHBCFG_GLBL_INTR_EN;
    reg |= USB_GAHBCFG_HBSTLEN_INCR4;
    reg |= USB_GAHBCFG_DMA_EN;
    usb_write(USB_GAHBCFG, reg);
    
    /* Configure USB (GUSBCFG) */
    reg = usb_read(USB_GUSBCFG);
    // Ici, le mode Hôte Forcé est souvent configuré.
    reg |= USB_GUSBCFG_FHMOD;
    usb_write(USB_GUSBCFG, reg);
    
    if (usb_host_init() < 0) {
        printf("NET: Failed to initialize USB host port\r\n");
        return -1;
    }
    
    printf("USB: Controller initialized\r\n");
    
    return 0;
}

/* Initialize network driver */
int net_init(void) {
    int i;
    
    printf("NET: Initializing network driver...\r\n");
    
    /* Initialize device structure */
    for (i = 0; i < ETH_ALEN; i++) {
        net_dev.mac_addr[i] = default_mac[i];
    }
    
    net_dev.state = NET_STATE_DOWN;
    net_dev.link_speed = 0;
    net_dev.rx_packets = 0;
    net_dev.tx_packets = 0;
    net_dev.rx_errors = 0;
    net_dev.tx_errors = 0;
    
    /* Initialize USB controller */
    if (usb_init() < 0) {
        printf("NET: Failed to initialize USB controller\r\n");
        return -1;
    }
    
    // *****************************************************
    // NOUVEAU : Étape 1 d'Énumération USB : SET_ADDRESS
    // *****************************************************
    uint8_t new_usb_addr = 1; 

    printf("NET: Démarrage de l'énumération USB (SET_ADDRESS 0x%x)...\r\n", new_usb_addr);
    if (usb_enumerate_set_address(new_usb_addr) < 0) {
        printf("NET: Échec de l'énumération SET_ADDRESS.\r\n");
        return -1;
    }
    
    /* Note: Complete SMSC LAN9514 initialization would require:
     * 2. Device descriptor reading (GET_DESCRIPTOR)
     * 3. Configuration setup (SET_CONFIGURATION)
     * 4. Bulk endpoints setup
     * 5. SMSC-specific register configuration
     */
    
    net_dev.state = NET_STATE_UP;
    
    printf("NET: Network driver initialized\r\n");
    printf("NET: MAC address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           net_dev.mac_addr[0], net_dev.mac_addr[1], net_dev.mac_addr[2],
           net_dev.mac_addr[3], net_dev.mac_addr[4], net_dev.mac_addr[5]);
    
    return 0;
}

/* Get network device */
struct net_device *net_get_device(void) {
    return &net_dev;
}

/* Send packet */
int net_send_packet(const uint8_t *packet, uint32_t length) {
    if (net_dev.state != NET_STATE_UP && net_dev.state != NET_STATE_LINK_UP) {
        return -1;
    }
    
    if (length > ETH_FRAME_LEN) {
        net_dev.tx_errors++;
        return -2;
    }
    
    /* TODO: Implement actual packet transmission via USB bulk endpoint */
    
    printf("NET: Would send packet of %d bytes\r\n", length);
    net_dev.tx_packets++;
    
    return 0;
}

/* Receive packet */
int net_receive_packet(uint8_t *packet, uint32_t max_length) {
    if (net_dev.state != NET_STATE_UP && net_dev.state != NET_STATE_LINK_UP) {
        return -1;
    }
    
    /* TODO: Implement actual packet reception via USB bulk endpoint */
    
    /* No packet available for now */
    return 0;
}

/* Check link status */
int net_link_status(void) {
    /* TODO: Read PHY status register to check actual link status */
    /* For now, return based on device state */
    return (net_dev.state == NET_STATE_LINK_UP) ? 1 : 0;
}


/* Get MAC address */
void net_get_mac_address(uint8_t *mac) {
    int i;
    
    if (!mac) {
        return;
    }
    
    for (i = 0; i < ETH_ALEN; i++) {
        mac[i] = net_dev.mac_addr[i];
    }
}

/* Set network state */
int net_set_state(int enable) {
    if (enable) {
        if (net_dev.state == NET_STATE_DOWN) {
            net_dev.state = NET_STATE_UP;
            printf("NET: Interface enabled\r\n");
        }
    } else {
        net_dev.state = NET_STATE_DOWN;
        printf("NET: Interface disabled\r\n");
    }
    
    return 0;
}

/* Print network statistics */
void net_print_stats(void) {
    printf("\r\n=== Network Statistics ===\r\n");
    printf("State: ");
    
    switch (net_dev.state) {
        case NET_STATE_DOWN:
            printf("DOWN\r\n");
            break;
        case NET_STATE_UP:
            printf("UP (no link)\r\n");
            break;
        case NET_STATE_LINK_DOWN:
            printf("UP (link down)\r\n");
            break;
        case NET_STATE_LINK_UP:
            printf("UP (link up)\r\n");
            break;
        default:
            printf("UNKNOWN\r\n");
    }
    
    printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           net_dev.mac_addr[0], net_dev.mac_addr[1], net_dev.mac_addr[2],
           net_dev.mac_addr[3], net_dev.mac_addr[4], net_dev.mac_addr[5]);
    
    printf("Link Speed: %d Mbps\r\n", net_dev.link_speed);
    printf("RX Packets: %d\r\n", net_dev.rx_packets);
    printf("TX Packets: %d\r\n", net_dev.tx_packets);
    printf("RX Errors: %d\r\n", net_dev.rx_errors);
    printf("TX Errors: %d\r\n", net_dev.tx_errors);
    printf("========================\r\n\r\n");
}

/* IRQ handler */
void net_irq_handler(void) {
    /* TODO: Handle network interrupts */
}