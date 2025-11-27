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

// DÉPLACEMENT ICI : Buffer statique global et aligné pour le DMA
// aligned(16) est une sécurité pour le cache line et les contraintes DMA
static uint8_t rx_buffer[1536] __attribute__((aligned(16)));

void kernel_main()
{
	uart_init();
	init_printf(NULL, putc);

	printf("kernel boots ...\n\r");

	irq_vector_init();
	timer_init();
//	generic_timer_init();
	enable_interrupt_controller();
	enable_irq();

	// --- INITIALISATION DU PILOTE ETHERNET ---
    printf("NET: Initialisation du driver réseau...\n\r");
    if (net_init() < 0) {
        printf("NET: L'initialisation du réseau a échoué ! Arrêt.\n\r");
        // Vous pouvez décider d'arrêter le boot ou de continuer sans réseau
        // return; 
    }
    printf("NET: Initialisation du driver réseau terminée.\n\r");
    // ------------------------------------------

	// Init Réseau
    if (net_init() < 0) {
        printf("NET: Error.\n\r");
    } else {
        printf("NET: Init OK.\n\r");
        
        // --- TEST D'ENVOI DE PAQUET ---
        // Création d'un buffer Ethernet simple (Destination FF:FF... broadcast)
        // 6 octets Dest, 6 octets Src, 2 octets Type, + Data
        static uint8_t dummy_packet[64] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Dest MAC (Broadcast)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Src MAC (Fake)
            0x08, 0x00,                         // EtherType (IPv4)
            0x45, 0x00, 0x00, 0x00              // Début Payload IP bidon...
        };
        
        // Remplissage du reste avec des A
        for(int i=14; i<64; i++) dummy_packet[i] = 0xAA;

        printf("KERNEL: Sending dummy packet...\n\r");
        net_send_packet(dummy_packet, 64);
        // ------------------------------
    
	// --- TEST ARP ---
        static uint8_t arp_packet[42] = {
             // ... (votre paquet ARP inchangé) ...
             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Dest MAC (Broadcast)
             0xB8, 0x27, 0xEB, 0x00, 0x00, 0x01, // Src MAC
             0x08, 0x06,                         // EtherType ARP
             0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, // ARP Request
             0xB8, 0x27, 0xEB, 0x00, 0x00, 0x01, // Sender MAC
             0x0A, 0x00, 0x02, 0x0F,             // Sender IP (10.0.2.15)
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Target MAC (0)
             0x0A, 0x00, 0x02, 0x02              // Target IP (10.0.2.2)
        };

        printf("KERNEL: Sending ARP Request...\n\r");
        net_send_packet(arp_packet, 42);
        
        printf("KERNEL: Waiting for ARP Reply...\n\r");
        
        // Remplir le buffer avec un motif connu pour vérifier l'écrasement
        for(int k=0; k<1536; k++) rx_buffer[k] = 0xCC;

        for (int i = 0; i < 20; i++) {
            if (net_receive_packet(rx_buffer, 1536) > 0) {
                printf("KERNEL: PACKET RECEIVED!\n\r");
                
                // Petit Dump Hexadécimal pour voir tout le paquet
                printf("Dump: ");
                for(int j=0; j<30; j++) printf("%x ", rx_buffer[j]);
                printf("\n\r");

                // Vérification spécifique ARP Reply
                // L'en-tête Ethernet fait 14 octets.
                // Sender MAC dans ARP est à l'offset 22 (14 + 8)
                printf("Source MAC: %x:%x:%x:%x:%x:%x\n\r", 
                    rx_buffer[22], rx_buffer[23], rx_buffer[24],
                    rx_buffer[25], rx_buffer[26], rx_buffer[27]);
                break;
            }
            delay(100000);
        }
	}
	int res = copy_process(PF_KTHREAD, (unsigned long)&kernel_process, 0);
	if (res < 0) {
		printf("error while starting kernel process");
		return;
	}

	while (1){
		schedule();
	}
}
