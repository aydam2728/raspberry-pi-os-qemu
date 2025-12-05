#ifndef	_USER_SYS_H
#define	_USER_SYS_H

void call_sys_write(char * buf);
int call_sys_fork();
void call_sys_exit();
long call_sys_write1(unsigned int fd, char *buf, unsigned int count);
long call_sys_read(unsigned int fd, char *buf, unsigned int count);
long call_sys_pipe(int *fds);
long call_sys_close(unsigned int fd);

extern void user_delay ( unsigned long);
extern unsigned long get_sp ( void );
extern unsigned long get_pc ( void );

#endif  /*_USER_SYS_H */
