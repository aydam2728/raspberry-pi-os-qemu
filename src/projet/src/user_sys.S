.section ".text.user"

.set SYS_WRITE, 0
.set SYS_FORK,  1
.set SYS_EXIT,  2
.set SYS_READ,  3
.set SYS_PIPE,  4
.set SYS_CLOSE, 5

.globl user_delay
user_delay:
    subs x0, x0, #1
    bne user_delay
    ret
    
.globl call_sys_write
call_sys_write:
    mov w8, #SYS_WRITE
    svc #0
    ret

.globl call_sys_read
call_sys_read:
    mov w8, #SYS_READ
    svc #0
    ret

.globl call_sys_pipe
call_sys_pipe:
    mov w8, #SYS_PIPE
    svc #0
    ret

.globl call_sys_close
call_sys_close:
    mov w8, #SYS_CLOSE
    svc #0
    ret

.globl call_sys_fork
call_sys_fork:
    mov w8, #SYS_FORK
    svc #0
    ret

.globl call_sys_exit
call_sys_exit:
    mov w8, #SYS_EXIT
    svc #0
    ret