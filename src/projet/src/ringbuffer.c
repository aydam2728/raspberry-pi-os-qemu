#include "mm.h"      // Pour get_free_page
#include "ringbuffer.h"
#include "sched.h" // Pour sleep_on, wake_up, preempt_disable...

struct ringbuffer* ringbuffer_create() {
    // 1. Allocation d'une page physique brute
    unsigned long page = get_free_page();
    if (!page) return 0;

    // 2. Nettoyage complet (Sécurité + Init à 0)
    memzero(page, PAGE_SIZE);
    
    struct ringbuffer *rb = (struct ringbuffer *)page;

    // 3. Initialisation explicite des drapeaux
    // On considère le pipe ouvert des deux côtés à la création
    rb->read_open = 1;
    rb->write_open = 1;
    
    // read_pos et write_pos sont déjà à 0 grâce au memzero
    
    return rb;
}

int ringbuffer_write(struct ringbuffer *rb, char *src, int n) {
    if (!rb) return -1;

    int i = 0;
    
    // Boucle pour écrire chaque octet
    while (i < n) {
        preempt_disable(); // CRITIQUE : Début transaction atomique sur monocoeur

        // 1. Vérification : Le lecteur est-il toujours là ?
        if (rb->read_open == 0) {
            preempt_enable();
            return -1; // Broken Pipe : on écrit dans le vide, erreur.
        }

        // 2. Calcul de la position suivante
        uint32_t next_write = (rb->write_pos + 1) % RB_DATA_SIZE;

        // 3. Vérification : Le buffer est-il PLEIN ?
        // Si la prochaine écriture touche la lecture, on est plein.
        if (next_write == rb->read_pos) {
            
            // A. On réveille le lecteur (au cas où il dort car buffer vide)
            wake_up((unsigned long)rb);
            
            // B. On réactive les interruptions avant de dormir
            preempt_enable();
            
            // C. On dort en attendant que de la place se libère
            // La tâche passe en état TASK_INTERRUPTIBLE
            sleep_on((unsigned long)rb);
            
            // D. Au réveil, on reprend la boucle while depuis le début
            // pour revérifier si de la place s'est VRAIMENT libérée.
            continue; 
        }

        // 4. Écriture effective (Il y a de la place)
        rb->buffer[rb->write_pos] = src[i];
        
        // Barrière mémoire pour s'assurer que la donnée est écrite avant l'index
        asm volatile("" ::: "memory");
        
        rb->write_pos = next_write;
        i++;

        preempt_enable(); // Fin transaction atomique pour cet octet
    }

    // Tout est écrit, on réveille le lecteur qui attendait peut-être des données
    wake_up((unsigned long)rb);
    return i; // Nombre d'octets écrits
}


int ringbuffer_read(struct ringbuffer *rb, char *dst, int n) {
    if (!rb) return -1;

    int i = 0;

    while (i < n) {
        preempt_disable(); // CRITIQUE

        // 1. Vérification : Le buffer est-il VIDE ?
        if (rb->read_pos == rb->write_pos) {
            
            // A. Si vide ET écrivain parti -> Fin de fichier (EOF)
            if (rb->write_open == 0) {
                preempt_enable();
                return i; // On retourne ce qu'on a lu jusqu'ici (peut être 0)
            }

            // B. Sinon, on doit attendre des données
            wake_up((unsigned long)rb); // Réveille l'écrivain s'il dormait (buffer plein)
            
            preempt_enable();
            sleep_on((unsigned long)rb);
            
            // C. Au réveil, on revérifie la boucle
            continue;
        }

        // 2. Lecture effective (Il y a des données)
        dst[i] = rb->buffer[rb->read_pos];
        
        // Barrière
        asm volatile("" ::: "memory");

        // 3. Avancer la tête de lecture
        rb->read_pos = (rb->read_pos + 1) % RB_DATA_SIZE;
        i++;

        preempt_enable();
    }

    // On a fait de la place, on signale à l'écrivain
    wake_up((unsigned long)rb);
    return i;
}




// Appelé par sys_close ou exit_process
void ringbuffer_close(struct ringbuffer *rb, int is_writer) {
    if (!rb) return;

    preempt_disable();

    if (is_writer) {
        rb->write_open = 0;
    } else {
        rb->read_open = 0;
    }

    // TRES IMPORTANT : On réveille tout le monde sur ce canal !
    // Pourquoi ? 
    // Si un lecteur dort en attendant des données, et que l'écrivain ferme (write_open=0),
    // le lecteur DOIT se réveiller pour voir le flag 'write_open=0' et retourner EOF.
    // Sinon, il reste endormi pour toujours (Zombie).
    wake_up((unsigned long)rb);

    // Si les deux sont partis, on libère la page physique
    if (rb->read_open == 0 && rb->write_open == 0) {
        free_page((unsigned long)rb);
    }

    preempt_enable();
}
