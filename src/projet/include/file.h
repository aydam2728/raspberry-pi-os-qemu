// file.h
#ifndef _FILE_H
#define _FILE_H

#include "ringbuffer.h"

#define NOFILE 16  // Max fichiers ouverts par processus

enum file_type { FD_NONE, FD_PIPE, FD_INODE };

struct file {
    enum file_type type;
    int ref_count;     // Combien de processus tiennent ce fichier ?
    char readable;     // 1 si on peut lire
    char writable;     // 1 si on peut écrire
    struct ringbuffer *pipe; // Pointeur vers le buffer (si type == FD_PIPE)
};

// Allocation/Gestion
struct file* filealloc();
void file_init();
void fileclose(struct file *f);
void filedup(struct file *f);

#endif