#include "fork.h"
#include "printf.h"
#include "utils.h"
#include "sched.h"
#include "mm.h"
#include "ringbuffer.h"
#include "file.h"


void sys_write(char * buf){
	printf(buf);
}

int sys_fork(){
	return copy_process(0, 0, 0);
}

void sys_exit(){
	exit_process();
}

// Helper pour récupérer le fichier depuis le FD
struct file* get_file_from_fd(int fd) {
    if (fd < 0 || fd >= NOFILE) return 0;
    return current->ofile[fd];
}

// === SYS_PIPE ===
long sys_pipe(int *u_fds) {
    // 1. Créer le buffer physique
    struct ringbuffer *rb = ringbuffer_create();
    if (!rb) return -1;

    // 2. Allouer 2 objets 'file'
    struct file *f_read = filealloc();
    struct file *f_write = filealloc();
    if (!f_read || !f_write) return -1; // (Il faudrait nettoyer rb ici en vrai)

    // 3. Trouver 2 FD libres dans le processus
    int fd0 = -1, fd1 = -1;
    for(int i=0; i<NOFILE; i++) {
        if (current->ofile[i] == 0) {
            if (fd0 == -1) fd0 = i;
            else { fd1 = i; break; }
        }
    }
    if (fd0 == -1 || fd1 == -1) return -1;

    // 4. Configurer le côté Lecture
    f_read->type = FD_PIPE;
    f_read->readable = 1;
    f_read->writable = 0;
    f_read->pipe = rb;
    f_read->ref_count = 1;

    // 5. Configurer le côté Écriture
    f_write->type = FD_PIPE;
    f_write->readable = 0;
    f_write->writable = 1;
    f_write->pipe = rb;
    f_write->ref_count = 1;

    // 6. Lier au processus
    current->ofile[fd0] = f_read;
    current->ofile[fd1] = f_write;

    // 7. Renvoyer les FD à l'utilisateur
    // u_fds est une adresse USER. On écrit directement dedans.
    u_fds[0] = fd0;
    u_fds[1] = fd1;

    return 0; // Succès
}

// === SYS_WRITE ===
long sys_write1(unsigned int fd, char *buf, unsigned int count) {
    struct file *f = get_file_from_fd(fd);
    
    // Vérifications : Fichier existe ? Est-ce un Pipe ? A-t-on le droit d'écrire ?
    if (!f || f->type != FD_PIPE || f->writable == 0) {
        return -1;
    }

    // Appel au moteur RingBuffer (voir implémentation précédente)
    // 'buf' est une adresse User, gérée dans ringbuffer_write
    return ringbuffer_write(f->pipe, buf, count);
}

// === SYS_READ ===
long sys_read(unsigned int fd, char *buf, unsigned int count) {
    struct file *f = get_file_from_fd(fd);
    
    if (!f || f->type != FD_PIPE || f->readable == 0) {
        return -1;
    }

    return ringbuffer_read(f->pipe, buf, count);
}

// === SYS_CLOSE ===
long sys_close(unsigned int fd) {
    struct file *f = get_file_from_fd(fd);
    if (!f) return -1;

    fileclose(f);          // Décrémente ref_count et libère si 0
    current->ofile[fd] = 0; // Libère le slot dans le processus
    return 0;
}



void * const sys_call_table[] = {sys_write, sys_fork, sys_exit, sys_pipe, sys_write1, sys_read, sys_close};
