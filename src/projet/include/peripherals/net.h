#ifndef _P_NET_H
#define _P_NET_H

#include "base.h"
#include <stdint.h>

int net_init(void);
// --- I. CONSTANTES RÉSEAU & STRUCTURES (CORRECT ORDER) ---

// Longueur de l'adresse MAC
#define ETH_ALEN 6

// Taille maximale d'une trame Ethernet
#define ETH_FRAME_LEN 1536 

// États du périphérique réseau
#define NET_STATE_DOWN      0
#define NET_STATE_UP        1       // Interface active, mais pas de lien physique
#define NET_STATE_LINK_DOWN 2
#define NET_STATE_LINK_UP   3

// Déclaration de la structure du périphérique réseau (maintenant complète)
struct net_device {
    uint8_t mac_addr[ETH_ALEN];
    uint8_t usb_addr;          // Adresse USB attribuée (ex: 1)
    int state;
    uint32_t link_speed;       // en Mbps
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_errors;
    uint32_t tx_errors;
};

// Déclaration anticipée de la structure du Paquet Setup (définition complète dans net.c)
struct usb_setup_packet;


// --- II. REGISTRES DWC2 (DesignWare Core 2) ---

/* USB Controller Base Address pour BCM2837*/
#define USB_BASE        (PBASE + 0x00980000)

// --- II.A. Registres Globaux (Global Registers) ---

#define USB_GOTGCTL     0x000
#define USB_GOTGINT     0x004
#define USB_GAHBCFG     0x008
#define USB_GUSBCFG     0x00C
#define USB_GRSTCTL     0x010
#define USB_GINTSTS     0x014
#define USB_GINTMSK     0x018
#define USB_GRXSTSR     0x01C
#define USB_GRXSTSP     0x020
#define USB_GRXFSIZ     0x024
#define USB_GNPTXFSIZ   0x028

/* USB GAHBCFG Register bits */
#define USB_GAHBCFG_GLBL_INTR_EN    (1 << 0)
#define USB_GAHBCFG_HBSTLEN_INCR4   (1 << 1)
#define USB_GAHBCFG_DMA_EN          (1 << 5)
#define USB_GAHBCFG_TXFEMPTYLVL     (1 << 7)
#define USB_GAHBCFG_PTXFEMPTYLVL    (1 << 8)

/* USB GRSTCTL Register bits */
#define USB_GRSTCTL_CSFTRST         (1 << 0)
#define USB_GRSTCTL_HSFTRST         (1 << 1)
#define USB_GRSTCTL_FRMCNTRRST      (1 << 2)
#define USB_GRSTCTL_RXFFLSH         (1 << 4)
#define USB_GRSTCTL_TXFFLSH         (1 << 5)
#define USB_GRSTCTL_AHBIDLE         (1 << 31)

// --- II.B. Registres Hôtes (Host Registers) ---

#define USB_HCFG        0x400  /* Host Configuration Register */
#define USB_HPRT        0x440  /* Host Port Control and Status Register */

/* HPRT Register bits*/
#define HPRT_PRTSPD_HIGH        (0 << 17) 
#define HPRT_PRTPWR             (1 << 12) /* Port Power */
#define HPRT_PRTRST             (1 << 8)  /* Port Reset */
#define HPRT_PRTCONNS           (1 << 1)  /* Port Connect Status */
#define HPRT_PRTENA             (1 << 2)  /* Port Enable */

// --- II.C. Registres de Canaux Hôtes (Host Channel Registers) ---

/* Macros corrigées : une seule définition + parenthèses pour éviter les conflits de priorité */
#define HCCHAR(n) (0x500 + 0x20 * (n))
#define HCTSIZ(n) (0x508 + 0x20 * (n))
#define HCDMA(n)  (0x50C + 0x20 * (n))
#define HCINT(n)  (0x504 + 0x20 * (n)) /* Ajout de HCINT(n) pour la complétude */

/* HCCHAR bits definitions */
#define HCCHAR_CHENA        (1 << 31)
#define HCCHAR_EPTYPE_CONTROL   (0x0 << 18) /* Endpoint Type: Control */
#define HCCHAR_CHDIS        (1 << 30)
#define HCCHAR_ODDFRM       (1 << 29)
#define HCCHAR_DEVADDR(x)   ((x & 0x7F) << 22)  /* Device Address: Bits 22-28 */
#define HCCHAR_MC(x)        ((x & 0x3) << 20)   /* Multi Count */
#define HCCHAR_EPTYPE(x)    ((x & 0x3) << 18)   /* Endpoint Type: Bits 18-19 */
#define HCCHAR_LSPDDEV      (1 << 17)           /* Low Speed Device */
#define HCCHAR_EPDIR_IN     (1 << 15)           /* Endpoint Direction: IN (Bit 15) */
#define HCCHAR_EPDIR_OUT    (0 << 15)           /* Endpoint Direction: OUT */
#define HCCHAR_EPNUM(x)     ((x & 0xF) << 11)   /* Endpoint Number: Bits 11-14 */
#define HCCHAR_MPS(x)       ((x & 0x7FF) << 0)  /* Max Packet Size: Bits 0-10 */

/* Endpoint Types */
#define EPTYPE_CTRL         0
#define EPTYPE_ISO          1
#define EPTYPE_BULK         2
#define EPTYPE_INTR         3

/* HCTSIZ bits */
#define HCTSIZ_PID_SETUP        (0x3 << 29) /* PID: Setup Packet */
#define HCTSIZ_PID_DATA0        (0x0 << 29) /* PID: Data0 */
#define HCTSIZ_PID_DATA1        (0x2 << 29) /* PID: Data1 */
#define HCTSIZ_XFRSIZ(s)        ((s) & 0x7FFFF) /* Transfer Size */
#define HCTSIZ_PKTCNT(c)        (((c) & 0x3FF) << 19) /* Packet Count */

/* HCINT bits (Interrupts) */
#define HCINT_XFRC              (1 << 0)    /* Transfer Complete */
#define HCINT_CHH               (1 << 1)    /* Channel Halted */
#define HCINT_AHBERR            (1 << 2)    /* AHB Error */


// --- III. COMMANDES USB STANDARD (pour l'énumération) ---

#define USB_DT_DEVICE           0x01    /* Descripteur de Périphérique */
#define USB_DT_CONFIGURATION    0x02    /* Descripteur de Configuration */

// --- IV. DÉCLARATIONS DE FONCTIONS (MAC/PHY) ---

/* Déclarations des fonctions dans net.c */
int net_init(void);
struct net_device *net_get_device(void);
int net_send_packet(const uint8_t *packet, uint32_t length);
int net_receive_packet(uint8_t *packet, uint32_t max_length);
int net_link_status(void);
void net_get_mac_address(uint8_t *mac);
int net_set_state(int enable);
void net_print_stats(void);
void net_irq_handler(void);

// --- V. REGISTRES SMSC LAN9514 (pour référence) ---
// Note: L'accès se fera via des transferts de contrôle USB.

#define MAC_CR          0x100
#define ADDRH           0x104
#define ADDRL           0x108

#define PHY_BCR         0x00
#define PHY_BSR         0x01
#define PHY_ID1         0x02
#define PHY_ID2         0x03

/* Control flags */
#define MAC_CR_TXEN     (1 << 3)
#define MAC_CR_RXEN     (1 << 2)

/* PHY Control flags */
#define PHY_BCR_RESET           (1 << 15)
#define PHY_BCR_LOOPBACK        (1 << 14)
#define PHY_BCR_SPEED_SEL       (1 << 13)
#define PHY_BCR_AN_ENABLE       (1 << 12)
#define PHY_BCR_POWER_DOWN      (1 << 11)
#define PHY_BCR_ISOLATE         (1 << 10)
#define PHY_BCR_RESTART_AN      (1 << 9)
#define PHY_BCR_DUPLEX_MODE     (1 << 8)

/* PHY Status flags */
#define PHY_BSR_100BASE_T4      (1 << 15)
#define PHY_BSR_100BASE_TX_FD   (1 << 14)
#define PHY_BSR_100BASE_TX_HD   (1 << 13)
#define PHY_BSR_10BASE_T_FD     (1 << 12)
#define PHY_BSR_10BASE_T_HD     (1 << 11)
#define PHY_BSR_AN_COMPLETE     (1 << 5)
#define PHY_BSR_REMOTE_FAULT    (1 << 4)
#define PHY_BSR_AN_ABILITY      (1 << 3)
#define PHY_BSR_LINK_STATUS     (1 << 2)
#define PHY_BSR_JABBER_DETECT   (1 << 1)
#define PHY_BSR_EXTENDED_CAP    (1 << 0)

/* HPRT Register bits - VERSION CORRIGÉE */
#define HPRT_PRTSPD_HIGH        (0 << 17) 
#define HPRT_PRTPWR             (1 << 12) /* Port Power */
#define HPRT_PRTRST             (1 << 8)  /* Port Reset */
#define HPRT_PRTENA             (1 << 2)  /* Port Enable */
#define HPRT_PRTCONNS           (1 << 0)  /* Port Connect Status (Bit 0) */

/* Indicateurs de changement (utilisés pour le nettoyage) */
/* Sous QEMU raspi3, ces bits semblent être en haut (17+), gardons votre définition précédente pour le nettoyage qui semble avoir fonctionné pour nettoyer le registre */
#define HPRT_PRTCONCHG      (1 << 17)
#define HPRT_PRTENCHG       (1 << 18)
#define HPRT_PRTOVRCURRCHG  (1 << 19) 
#define HPRT_PRTRSTCHG      (1 << 20)

#define USB_GUSBCFG_FHMOD           (1 << 8)

#endif /* _P_NET_H */