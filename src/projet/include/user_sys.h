#ifndef _USER_SYS_H
#define _USER_SYS_H

int call_sys_write(int fd, char * buf, int count);
int call_sys_read(int fd, char * buf, int count);
int call_sys_pipe(int *fds);
void call_sys_close(int fd);
int call_sys_fork();
void call_sys_exit(); 
int call_sys_wait(int *status); 

extern void user_delay ( unsigned long);
extern unsigned long get_sp ( void );
extern unsigned long get_pc ( void );

#endif  /*_USER_SYS_H */
