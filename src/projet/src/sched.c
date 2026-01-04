#include "sched.h"
#include "irq.h"
#include "printf.h"
#include "utils.h"
#include "mm.h"



static struct task_struct init_task = INIT_TASK;
struct task_struct *current = &(init_task);
struct task_struct * task[NR_TASKS] = {&(init_task), };
int nr_tasks = 1;

void preempt_disable(void)
{
	current->preempt_count++;
}

void preempt_enable(void)
{
	current->preempt_count--;
}


void sleep_on(void *chan) {
    current->state = TASK_INTERRUPTIBLE;
    current->chan = chan;
    schedule();
    current->chan = 0;
}

void wake_up(void *chan) {
    for(int i = 0; i < NR_TASKS; i++) {
        struct task_struct *p = task[i];
        if(p && p->state == TASK_INTERRUPTIBLE && p->chan == chan) {
            p->state = TASK_RUNNING;
        }
    }
}

void _schedule(void)
{
	preempt_disable();
	int next,c;
	struct task_struct * p;
	while (1) {
		c = -1;
		next = 0;
		for (int i = 0; i < NR_TASKS; i++){
			p = task[i];
			if (p && p->state == TASK_RUNNING && p->counter > c) {
				c = p->counter;
				next = i;
			}
		}
		if (c) {
			break;
		}
		for (int i = 0; i < NR_TASKS; i++) {
			p = task[i];
			if (p) {
				p->counter = (p->counter >> 1) + p->priority;
			}
		}
	}
	switch_to(task[next]);
	preempt_enable();
}

void schedule(void)
{
	current->counter = 0;
	_schedule();
}


void switch_to(struct task_struct * next)
{
	if (current == next)
		return;
	struct task_struct * prev = current;
	current = next;
	set_pgd(next->mm.pgd);
	cpu_switch_to(prev, next);
}

void schedule_tail(void) {
	preempt_enable();
}


void timer_tick()
{
	--current->counter;
	if (current->counter>0 || current->preempt_count >0) {
		return;
	}
	current->counter=0;
	enable_irq();
	_schedule();
	disable_irq();
}

void exit_process(){
    
    
    preempt_disable(); 
    
    for (int i = 0; i < NR_TASKS; i++){
        if (task[i] == current) {
             task[i] = 0; // NE JAMAIS FAIRE CA ICI ! (Laisse le zombie pour sys_wait)
        }
    }
    
    exit_files(current);
    
    // DEBUG MIDDLE
    //printf("[KERNEL] Files closed. Setting Zombie state...\n");

    current->state = TASK_ZOMBIE;
    current->exit_code = 0;
    
    // On réveille le parent (Méthode brute pour être sûr)
    for(int i=0; i<NR_TASKS; i++) {
        struct task_struct *t = task[i];
        if(t && t->state == TASK_INTERRUPTIBLE) {
             t->state = TASK_RUNNING;
        }
    }
    
    // DEBUG END
    //printf("[KERNEL] Bye bye. Scheduling...\n");
    
    preempt_enable();
    schedule();
}



int sys_wait(int *status) {
    int have_kids, pid;
    struct task_struct *p;

    while(1) {
        have_kids = 0;
        for (int i = 0; i < NR_TASKS; i++) {
            p = task[i];
            // On ne vérifie pas current (soi-même) et on cherche les processus non nuls
            if (!p || p == current) continue;
            
            // Si c'est un processus orphelin ou autre, on simplifie ici :
            // Dans un vrai OS, on vérifierait p->parent_id == current->pid
            // Pour ton OS simple, on suppose qu'on attend n'importe quel enfant.
            
            have_kids = 1;
            
            if (p->state == TASK_ZOMBIE) {
                // On a trouvé un enfant mort !
                pid = i; // ou p->pid si tu as un champ pid
                
                // Nettoyage final (Libération de la task_struct)
                free_page((unsigned long)p - VA_START);
                task[i] = 0; // On libère le slot dans le tableau
                
                return pid;
            }
        }

        // Si pas d'enfants du tout, on retourne erreur
        if (!have_kids || current->state == TASK_ZOMBIE) return -1;

        // Si des enfants vivent encore, on dort en attendant qu'ils meurent
        // Astuce: On utilise l'adresse de 'current' comme canal d'attente
        // exit_process() devra faire un wake_up sur le parent.
        sleep_on(current); 
    }
}