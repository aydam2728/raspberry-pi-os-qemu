#include "fs.h"
#include "sched.h"
#include "printf.h"
#include "utils.h"
#include "mm.h"
#include "mini_uart.h" 

int copy_process(unsigned long clone_flags, unsigned long fn, unsigned long arg, unsigned long stack);

// fd 1 = stdout (UART)
int sys_write(int fd, char * buf, int count) {
    if (fd == 1) { 
        for(int i=0; i<count; i++) {
            uart_send(buf[i]);
        }
        return count;
    }
    
    if(fd < 0 || fd >= NOFILE || !current->ofile[fd]) return -1;
    struct file *f = current->ofile[fd];
    if(!f->writable || f->type != FD_PIPE) return -1;
    
    return pipe_write(f->pipe, buf, count);
}

int sys_read(int fd, char * buf, int count) {
    if(fd < 0 || fd >= NOFILE || !current->ofile[fd]) return -1;
    struct file *f = current->ofile[fd];
    if(!f->readable || f->type != FD_PIPE) return -1;
    
    return pipe_read(f->pipe, buf, count);
}

int sys_pipe(int *fds) {
    struct file *f0, *f1;
    int fd0 = -1, fd1 = -1;

    if(pipe_alloc(&f0, &f1) < 0) return -1;

    // On commence à i = 3 pour protéger stdin(0), stdout(1), stderr(2)
    for(int i = 3; i < NOFILE; i++) {
        if(current->ofile[i] == 0) {
            if(fd0 == -1) fd0 = i;
            else { fd1 = i; break; }
        }
    }

    if(fd0 == -1 || fd1 == -1) {
        file_close(f0);
        file_close(f1);
        return -1;
    }

    current->ofile[fd0] = f0;
    current->ofile[fd1] = f1;
    fds[0] = fd0;
    fds[1] = fd1;
    return 0;
}

int sys_close(int fd) {
    if(fd < 0 || fd >= NOFILE || !current->ofile[fd]) return -1;
    file_close(current->ofile[fd]);
    current->ofile[fd] = 0;
    return 0;
}

// Fork doit être déclaré ici ou dans fork.c, mais sys_fork appelle copy_process
int sys_fork() {
    return copy_process(0, 0, 0, 0);
}


void sys_exit(struct task_struct *p) {
    for(int i = 0; i < NOFILE; i++) {
        if(p->ofile[i]) {
            file_close(p->ofile[i]);
            p->ofile[i] = 0;
        }
    }
}

int sys_wait(int *status);


void * const sys_call_table[] = {
    sys_write, // 0
    sys_fork,  // 1
    sys_exit,  // 2
    sys_read,  // 3
    sys_pipe,  // 4
    sys_close,  // 5
    sys_wait  // 6
};