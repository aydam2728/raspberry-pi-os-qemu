#include "net.h"
#include "peripherals/net.h"
#include "peripherals/base.h"
#include "utils.h"
#include "printf.h"


static struct net_device net_dev;

static volatile uint32_t *usb_regs = (uint32_t *)USB_BASE;

/* Default MAC address */
static uint8_t default_mac[ETH_ALEN] = {0xB8, 0x27, 0xEB, 0x00, 0x00, 0x01};

/* Helper functions for USB register access */
static inline uint32_t usb_read(uint32_t reg) {
    return *(volatile uint32_t *)(USB_BASE + reg);
}

static inline void usb_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(USB_BASE + reg) = value;
}

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
    usb_write(USB_GAHBCFG, reg);
    
    /* Configure USB */
    reg = usb_read(USB_GUSBCFG);
    usb_write(USB_GUSBCFG, reg);
    
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
    
    /* Note: Complete SMSC LAN9514 initialization would require:
     * 1. USB enumeration
     * 2. Device descriptor reading
     * 3. Configuration setup
     * 4. Bulk endpoints setup
     * 5. SMSC-specific register configuration
     * This is a simplified base implementation.
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
    /* This would require:
     * 1. Setting up USB bulk OUT endpoint
     * 2. Formatting data according to SMSC LAN9514 protocol
     * 3. Initiating USB transfer
     * 4. Waiting for completion
     */
    
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
    /* This would require:
     * 1. Setting up USB bulk IN endpoint
     * 2. Checking for available data
     * 3. Reading data from USB
     * 4. Parsing SMSC LAN9514 protocol header
     * 5. Extracting Ethernet frame
     */
    
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
    /* This would include:
     * 1. Reading interrupt status
     * 2. Handling received packets
     * 3. Handling transmission completion
     * 4. Handling errors
     */
}