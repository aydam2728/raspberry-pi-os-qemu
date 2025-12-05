#ifndef	_SYS_H
#define	_SYS_H

#define __NR_syscalls	 10

#ifndef __ASSEMBLER__

void sys_write(char * buf);
int sys_fork();
long sys_write1(unsigned int fd, char *buf, unsigned int count);
long sys_read(unsigned int fd, char *buf, unsigned int count);
long sys_pipe(int *fds);
long sys_close(unsigned int fd);

#endif

#endif  /*_SYS_H */
