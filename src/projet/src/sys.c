#include "fork.h"
#include "printf.h"
#include "utils.h"
#include "sched.h"
#include "mm.h"
#include "ringbuffer.h"


void sys_write(char * buf){
	printf(buf);
}

int sys_fork(){
	return copy_process(0, 0, 0);
}

void sys_exit(){
	exit_process();
}

void sys_initbuffer(){
	struct ringbuffer *rb = ringbuffer_create();
		
	if (!rb) {
		return 0; // Erreur (NULL)
	}
} 

void * const sys_call_table[] = {sys_write, sys_fork, sys_exit, sys_initbuffer};
