#ifndef	_SYS_H
#define	_SYS_H

#define __NR_syscalls	 6
#ifndef __ASSEMBLER__

int sys_write(int fd, char * buf, int count);
int sys_read(int fd, char * buf, int count);
int sys_pipe(int *fds);
int sys_close(int fd);
int sys_fork();
void sys_exit(struct task_struct *p);
int copy_process(unsigned long clone_flags, unsigned long fn, unsigned long arg, unsigned long stack);

#endif

#endif  /*_SYS_H */

