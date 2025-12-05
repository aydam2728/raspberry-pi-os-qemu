// ringbuffer.c
#include "ringbuffer.h"
#include "sched.h" 
#include "utils.h" // memcpy, copy_from_user, copy_to_user

// Création simple
struct ringbuffer* ringbuffer_create() {
    unsigned long page = get_free_page();
    if (!page) return 0;
    memzero(page, PAGE_SIZE);
    
    struct ringbuffer *rb = (struct ringbuffer *)page;
    rb->read_open = 1;
    rb->write_open = 1;
    return rb;
}

// Libération
void ringbuffer_free(struct ringbuffer *rb) {
    if(rb) free_page((unsigned long)rb);
}

// Écriture (User -> Kernel)
int ringbuffer_write(struct ringbuffer *rb, char *user_src, int n) {
    int i = 0;
    while (i < n) {
        preempt_disable(); 
        
        if (rb->read_open == 0) { // Broken Pipe
            preempt_enable(); return -1; 
        }

        uint32_t next_write = (rb->write_pos + 1) % RB_DATA_SIZE;

        if (next_write == rb->read_pos) { // PLEIN
            wake_up((unsigned long)rb);
            preempt_enable();
            sleep_on((unsigned long)rb);
            continue; // On revérifie au réveil
        }

        // COPIE SECURISEE : On récupère l'octet depuis l'espace USER
        char byte;
        // copy_from_user(&byte, user_src + i, 1); // Version idéale sécurisée
        byte = user_src[i]; // Version simplifiée (si Kernel peut lire User)

        rb->buffer[rb->write_pos] = byte;
        asm volatile("" ::: "memory");
        rb->write_pos = next_write;
        i++;
        
        preempt_enable();
    }
    wake_up((unsigned long)rb);
    return i;
}

// Lecture (Kernel -> User)
int ringbuffer_read(struct ringbuffer *rb, char *user_dst, int n) {
    int i = 0;
    while (i < n) {
        preempt_disable();

        if (rb->read_pos == rb->write_pos) { // VIDE
            if (rb->write_open == 0) { // EOF
                preempt_enable(); return i;
            }
            wake_up((unsigned long)rb);
            preempt_enable();
            sleep_on((unsigned long)rb);
            continue;
        }

        char byte = rb->buffer[rb->read_pos];
        
        // COPIE SECURISEE : On écrit vers l'espace USER
        // copy_to_user(user_dst + i, &byte, 1);
        user_dst[i] = byte; // Version simplifiée

        asm volatile("" ::: "memory");
        rb->read_pos = (rb->read_pos + 1) % RB_DATA_SIZE;
        i++;
        preempt_enable();
    }
    wake_up((unsigned long)rb);
    return i;
}