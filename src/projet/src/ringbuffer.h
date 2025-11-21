#ifndef _RINGBUFFER_H
#define _RINGBUFFER_H

#include <stdint.h>

// Calcul dynamique de la taille du tableau de données pour remplir la page
#define RB_DATA_SIZE (PAGE_SIZE - (4 * sizeof(uint32_t)))

struct ringbuffer {
    // Les données sont au début de la page (choix de ta structure)
    uint8_t buffer[RB_DATA_SIZE];
    
    // Les variables de contrôle sont à la fin
    volatile uint32_t read_pos;   // Tête de lecture
    volatile uint32_t write_pos;  // Tête d'écriture
    volatile uint32_t read_open;  // 1 = Lecteur actif, 0 = Lecteur parti
    volatile uint32_t write_open; // 1 = Écrivain actif, 0 = Écrivain parti
};

// Prototypes
struct ringbuffer* ringbuffer_create();
void ringbuffer_free(struct ringbuffer *rb);
int ringbuffer_read(struct ringbuffer *rb, char *dst, int n);
int ringbuffer_write(struct ringbuffer *rb, char *src, int n);

#endif