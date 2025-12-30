#include "fs.h"
#include "mm.h"
#include "sched.h"
#include "utils.h"
#include "printf.h"

struct file file_table[NFILE];

void fs_init(void) {
    // La BSS met tout à 0 par défaut
}

struct file* file_alloc(void) {
    for(int i = 0; i < NFILE; i++) {
        if(file_table[i].ref == 0) {
            file_table[i].ref = 1;
            return &file_table[i];
        }
    }
    return 0;
}

int pipe_alloc(struct file **f0, struct file **f1) {
    struct pipe *p;
    unsigned long page = get_free_page();
    if (!page) return -1;
    
    p = (struct pipe *)(page + VA_START);
    p->read_open = 1;
    p->write_open = 1;
    p->nwrite = 0;
    p->nread = 0;

    *f0 = file_alloc();
    *f1 = file_alloc();

    if(*f0 && *f1) {
        (*f0)->type = FD_PIPE;
        (*f0)->readable = 1; (*f0)->writable = 0; (*f0)->pipe = p;
        (*f1)->type = FD_PIPE;
        (*f1)->readable = 0; (*f1)->writable = 1; (*f1)->pipe = p;
        return 0;
    }
    
    if(*f0) (*f0)->ref = 0;
    if(*f1) (*f1)->ref = 0;
    free_page(page);
    return -1;
}

void file_close(struct file *f) {
    if(f->ref < 1) return;
    f->ref--;
    if(f->ref > 0) return;

    if(f->type == FD_PIPE) {
        struct pipe *p = f->pipe;
        if(f->writable) {
            p->write_open = 0;
            wake_up(&p->nread);
        } else {
            p->read_open = 0;
            wake_up(&p->nwrite);
        }
        if(p->read_open == 0 && p->write_open == 0) {
           // free_page((unsigned long)p - VA_START);
        }
    }
    f->type = FD_NONE;
}

int pipe_write(struct pipe *p, char *addr, int n) {
    int i;
    for(i = 0; i < n; i++) {
        while(p->nwrite == p->nread + PIPE_SIZE) {
            if(p->read_open == 0 || current->state == TASK_ZOMBIE) return -1;
            wake_up(&p->nread);
            sleep_on(&p->nwrite);
        }
        p->data[p->nwrite++ % PIPE_SIZE] = addr[i];
    }
    wake_up(&p->nread);
    return n;
}

int pipe_read(struct pipe *p, char *addr, int n) {
    int i;
    while(p->nread == p->nwrite) {
        if(p->write_open == 0) return 0;
        if(current->state == TASK_ZOMBIE) return -1;
        sleep_on(&p->nread);
    }
    for(i = 0; i < n; i++) {
        if(p->nread == p->nwrite) break;
        addr[i] = p->data[p->nread++ % PIPE_SIZE];
    }
    wake_up(&p->nwrite);
    return i;
}