.section ".text.user"

.set SYS_WRITE_NUMBER, 0 		// syscal numbers
.set SYS_FORK_NUMBER, 1
.set SYS_EXIT_NUMBER, 2
.set SYS_WRITE1_NUMBER, 3
.set SYS_READ_NUMBER,  4
.set SYS_PIPE_NUMBER,  5
.set SYS_CLOSE_NUMBER, 6

.globl user_delay
user_delay:
	subs x0, x0, #1
	bne user_delay
	ret

.globl call_sys_write
call_sys_write:
	mov w8, #SYS_WRITE_NUMBER
	svc #0
	ret

.globl call_sys_exit
call_sys_exit:
	mov w8, #SYS_EXIT_NUMBER
	svc #0
	ret

.globl call_sys_fork
call_sys_fork:
	mov w8, #SYS_FORK_NUMBER
	svc #0
	ret


/* * int call_sys_pipe(int *fds); 
 * x0 = pointeur vers tableau de 2 ints (User Space)
 */
.globl call_sys_pipe
call_sys_pipe:
    mov w8, #SYS_PIPE_NUMBER   /* x8 contient le numéro du syscall */
    svc #0                     /* Interruption logicielle -> Saut vers le Kernel */
    ret                        /* Retour à l'appelant, x0 contient le code retour */

/* * int call_sys_write(int fd, char *buf, int n);
 * x0=fd, x1=buf, x2=n
 */
.globl call_sys_write1
call_sys_write1:
    mov w8, #SYS_WRITE1_NUMBER
    svc #0
    ret

/* * int call_sys_read(int fd, char *buf, int n);
 * x0=fd, x1=buf, x2=n
 */
.globl call_sys_read
call_sys_read:
    mov w8, #SYS_READ_NUMBER
    svc #0
    ret

/* * int call_sys_close(int fd);
 * x0=fd
 */
.globl call_sys_close
call_sys_close:
    mov w8, #SYS_CLOSE_NUMBER
    svc #0
    ret
