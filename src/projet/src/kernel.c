#include <stddef.h>
#include <stdint.h>

#include "printf.h"
#include "utils.h"
#include "timer.h"
#include "irq.h"
#include "fork.h"
#include "sched.h"
#include "mini_uart.h"
#include "sys.h"
#include "user.h"
#include "peripherals/net.h"
#include "net/icmp.h"

// --- AJOUTS OBLIGATOIRES POUR LA COMPILATION ---
// Ces fonctions sont dans net.c mais pas dans le header
extern int net_send_packet(const uint8_t *packet, uint32_t length);
extern int net_receive_packet(uint8_t *packet, uint32_t max_length);
// Si vous voulez traiter le paquet (ARP/ICMP), il faudra aussi cette fonction :
//extern void ethernet_input(uint8_t *packet, uint32_t len); 
// -----------------------------------------------

void kernel_process(){
    printf("Kernel process started. EL %d\r\n", get_el());
    unsigned long begin = (unsigned long)&user_begin;
    unsigned long end = (unsigned long)&user_end;
    unsigned long process = (unsigned long)&user_process;
    int err = move_to_user_mode(begin, end - begin, process - begin);
    if (err < 0){
        printf("Error while moving process to user mode\n\r");
    }
}

// Buffer statique global et aligné pour le DMA
//////// modi
static uint8_t rx_buffer[1536] __attribute__((aligned(16)));

void kernel_main()
{
    uart_init();
    init_printf(NULL, putc);

    printf("kernel boots ...\n\r");

    irq_vector_init();
    timer_init();
    enable_interrupt_controller();
    enable_irq();

    // --- INITIALISATION DU PILOTE ETHERNET ---
    // (J'ai supprimé la deuxième initialisation qui était en double plus bas)
    printf("NET: Initialisation du driver réseau...\n\r");
    if (net_init() < 0) {
        printf("NET: L'initialisation du réseau a échoué ! Arrêt.\n\r");
    } else {
        printf("NET: Init OK.\n\r");

        // --- TEST ENVOI PAQUET ---
        static uint8_t dummy_packet[64] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Dest
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Src
            0x08, 0x00,                         // IPv4
            0x45, 0x00, 0x00, 0x00              // Payload
        };
        for(int i=14; i<64; i++) dummy_packet[i] = 0xAA;

        printf("KERNEL: Sending dummy packet...\n\r");
        net_send_packet(dummy_packet, 64);

        // --- TEST ARP REQUEST ---
        // Demande : Qui est 10.0.2.2 ? (La gateway QEMU)
        static uint8_t arp_packet[42] = {
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Broadcast
             0xB8, 0x27, 0xEB, 0x00, 0x00, 0x01, // Src MAC (Fictif)
             0x08, 0x06,                         // ARP
             0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, // Request
             0xB8, 0x27, 0xEB, 0x00, 0x00, 0x01, // Sender MAC
             0x0A, 0x00, 0x02, 0x0F,             // Sender IP (10.0.2.15)
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Target MAC (???)
             0x0A, 0x00, 0x02, 0x02              // Target IP (10.0.2.2)
        };

        printf("KERNEL: Sending ARP Request...\n\r");
        net_send_packet(arp_packet, 42);
        
        printf("KERNEL: Waiting for ARP Reply...\n\r");
        
        // Boucle d'attente courte (juste pour voir si ça marche au boot)
        for (int i = 0; i < 200; i++) { // J'ai augmenté un peu le délai
            int len = net_receive_packet(rx_buffer, 1536);
            if (len > 0) {
                printf("KERNEL: PACKET RECEIVED! (%d bytes)\n\r", len);
                
                // Important : Passer le paquet à la couche réseau pour traitement (ARP/ICMP)
                //ethernet_input(rx_buffer, len); 
                break;
            }
            delay(10000);
        }
    }

    int res = copy_process(PF_KTHREAD, (unsigned long)&kernel_process, 0);
    if (res < 0) {
        printf("error while starting kernel process");
        return;
    }

    // Le scheduler prend la main ici.
    // ATTENTION : Une fois ici, le réseau ne sera plus écouté
    // sauf si vous modifiez irq.c (voir étape suivante).
    while (1){
        schedule();
    }
}