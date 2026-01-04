#ifndef _FS_H
#define _FS_H

#include "sched.h"

#define PIPE_SIZE 512

#ifndef __ASSEMBLER__

#define NOFILE    16
#define NFILE     100

enum file_type { FD_NONE, FD_PIPE, FD_INODE };

struct pipe {
    
    char *data;      
    unsigned int nread;
    unsigned int nwrite;
    int read_open;
    int write_open;
};

struct file {
    enum file_type type;
    int ref;
    char readable;
    char writable;
    struct pipe *pipe;
};

void fs_init(void);
struct file* file_alloc(void);
void file_close(struct file *f);
int pipe_alloc(struct file **f0, struct file **f1);
int pipe_write(struct pipe *p, char *addr, int n);
int pipe_read(struct pipe *p, char *addr, int n);

#endif // Fin de __ASSEMBLER__

#endif