#include "utils.h"
#include "printf.h"
#include "sched.h"
#include "peripherals/timer.h"
#include <stdint.h>

const unsigned int interval = 200000;
unsigned int curVal = 0;

extern int net_receive_packet(uint8_t *packet, uint32_t max_length);
//extern void ethernet_input(uint8_t *packet, uint32_t len);

// Alignement pour DMA
static volatile uint8_t irq_net_buffer[4096] __attribute__((aligned(4096)));

void timer_init ( void )
{
	curVal = get32(TIMER_CLO);
	curVal += interval;
	put32(TIMER_C1, curVal);
}

// Ajoutez ceci en haut si ce n'est pas déjà fait


void handle_timer_irq( void )
{
    // 1. Reset du timer (Code standard)
    curVal += interval;
    put32(TIMER_C1, curVal);
    put32(TIMER_CS, TIMER_CS_M1);

    // 2. DEBUG HEARTBEAT : Vérifier que l'IRQ est vivante
    // On utilise une variable statique qui garde sa valeur entre les appels
    static int heartbeat = 0;
    if (heartbeat++ % 200 == 0) {
        // Affiche un point pour dire "Je suis vivant" sans spammer
        // printf("."); 
    }

    // 3. Test Réseau
    // (Avec le nouveau net.c propre, on n'a plus besoin de buffer volatile ici,
    // car net.c gère ses propres buffers internes static).
    static uint8_t packet_buffer[1536]; 
    
    int len = net_receive_packet(packet_buffer, 1536);
    
    if (len > 0) {
        // VICTOIRE : Un paquet est arrivé et a passé le filtre RNDIS
        printf("\n[RX] Packet len=%d | MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
               len, 
               packet_buffer[0], packet_buffer[1], packet_buffer[2], 
               packet_buffer[3], packet_buffer[4], packet_buffer[5]);
               
        //ethernet_input(packet_buffer, len);
    } 
    else if (len == 0) {
        // Rien reçu (normal la plupart du temps)
    }
    else {
        // Erreur ou NAK (-1 ou -2)
        // On n'affiche rien pour ne pas polluer, sauf si vous voulez debugger l'USB
        // printf("E"); 
    }

    // 4. Appel au scheduler
    timer_tick();
}