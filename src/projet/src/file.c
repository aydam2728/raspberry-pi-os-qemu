// file.c
#include "file.h"


struct file file_pool[100];

void file_init() {
    // On force la remise à zéro brutale de toute la table
    memzero((unsigned long)file_pool, sizeof(struct file) * 100);
}

struct file* filealloc() {
    for(int i=0; i<100; i++) {
        if(file_pool[i].ref_count == 0) {
            file_pool[i].ref_count = 1;
            return &file_pool[i];
        }
    }
    return 0;
}

void fileclose(struct file *f) {
    
    f->ref_count--;
    
    if(f->ref_count > 0) return;

    // Si c'est le dernier référence, on nettoie l'objet sous-jacent
    if(f->type == FD_PIPE) {
        struct ringbuffer *rb = f->pipe;
        
        // On signale au buffer qu'un coté se ferme
        if(f->writable) {
            rb->write_open = 0;
            // Reveille les lecteurs pour qu'ils voient EOF
            wake_up((unsigned long)rb);
        }
        if(f->readable) {
            rb->read_open = 0;
            // Reveille les écrivains pour qu'ils voient Broken Pipe
            wake_up((unsigned long)rb); 
        }

        // Si plus personne n'est connecté au pipe, on libère la page
        if(rb->read_open == 0 && rb->write_open == 0) {
            ringbuffer_free(rb);
        }
    }
    
    f->type = FD_NONE;
}