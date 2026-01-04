#ifndef _SCHED_H
#define _SCHED_H

#include "fs.h" 

#define TASK_RUNNING         0
#define TASK_INTERRUPTIBLE   1
#define TASK_ZOMBIE          2

// --- AJOUTS POUR CORRIGER FORK.C ---
#define PF_KTHREAD           2      // Flag pour thread noyau
#define THREAD_SIZE          4096   // Taille de la stack (4KB)

#define NR_TASKS             64

#define THREAD_CPU_CONTEXT   0

#ifndef __ASSEMBLER__

struct cpu_context {
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;
    unsigned long sp;
    unsigned long pc;
};

#ifndef NOFILE
#define NOFILE 16
#endif

// Déclaration anticipée pour casser la dépendance circulaire
struct mm_struct; 

// ATTENTION : Pour que cela compile, il faut que mm.h soit corrigé (voir Étape 2)
// Si mm.h inclut sched.h AVANT de définir struct mm_struct, ça plantera toujours.
// Nous supposons ici que task_struct inclut mm_struct par VALEUR, donc la définition complète est requise.
// C'est pourquoi l'étape 2 est CRUCIALE.

#include "mm.h" // On inclut mm.h pour avoir la définition de mm_struct

struct task_struct {
    struct cpu_context cpu_context;
    long state;
    long counter;
    long priority;
    long preempt_count;
    unsigned long flags;
    struct mm_struct mm;        // <--- C'est ici que ça bloquait
    struct file *ofile[NOFILE];
    void *chan;
    long exit_code;
};

#define INIT_TASK \
/*cpu_context*/ { {0,0,0,0,0,0,0,0,0,0,0,0,0}, \
/* state */ 0, \
/* counter */ 0, \
/* priority */ 1, \
/* preempt_count */ 0, \
/* flags */ 0, \
/* mm */ { 0, 0, {{0}}, 0, {0} }, \
/* ofile */ { 0 }, \
/* chan */ 0, \
/* exit_code */ 0 \
}

extern struct task_struct *current;
extern struct task_struct *task[NR_TASKS];
extern int nr_tasks;

void schedule(void);
void sleep_on(void *chan);
void wake_up(void *chan);
void exit_files(struct task_struct *p);
void exit_process(void);
int sys_wait(int *status);

// --- AJOUT PROTOTYPES ---
void preempt_disable(void);
void preempt_enable(void);

extern void cpu_switch_to(struct task_struct* prev, struct task_struct* next);
void switch_to(struct task_struct * next);

#endif
#endif