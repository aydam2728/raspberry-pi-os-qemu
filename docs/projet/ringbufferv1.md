# 1 : Implémentation de la Communication Inter-Processus (Pipes)

**Objectif** : Implémenter un mécanisme de tampon circulaire bloquant (Pipe) pour permettre la communication entre processus, faisant évoluer le système d'un simple multitâche vers un OS coopératif.

**Concept** : Un pipe agit comme un canal de données unidirectionnel. Un **"Ring Buffer"** (tampon circulaire) est utilisé pour stocker les données dans une page mémoire de taille fixe. Cette implémentation nécessite une intégration profonde avec l'ordonnanceur (mécanismes Sleep/Wakeup) et l'abstraction du système de fichiers.

---

### Concepts Fondamentaux

#### Le Ring Buffer (Tampon Circulaire)
Un ring buffer est un tableau de taille fixe qui boucle sur lui-même. Il utilise deux compteurs :
* **nwrite** : Le nombre total d'octets écrits dans le tampon depuis sa création.
* **nread** : Le nombre total d'octets lus depuis le tampon depuis sa création.
* Le tampon est considéré **VIDE** quand `nread == nwrite`.
* Le tampon est considéré **PLEIN** quand `nwrite == nread + PIPE_SIZE`.

#### E/S Bloquantes (Sleep & Wake)
Dans un OS baremetal, nous ne pouvons pas nous permettre de faire de l'attente active ("spin") en attendant des données.
* Si un processus tente de **lire** dans un pipe vide, il doit dormir (état `TASK_INTERRUPTIBLE`) jusqu'à ce que des données arrivent.
* Si un processus tente d'**écrire** dans un pipe plein, il doit dormir jusqu'à ce que de l'espace se libère.
* La synchronisation est réalisée via des appels `wake_up` sur des adresses mémoire spécifiques (canaux).

---

## 1. Définition des Structures de Données

Nous devons d'abord définir ce qu'est un "Fichier" et un "Pipe" dans les en-têtes du noyau.

**Fichier : `include/fs.h`**
Nous introduisons une couche d'abstraction de fichier. Un descripteur de fichier (entier) pointera vers une `struct file`, qui elle-même pointera vers la `struct pipe` réelle.

```c
#ifndef _FS_H
#define _FS_H

#include "sched.h"

#define PIPE_SIZE 512
#define NOFILE    16    // Max fichiers ouverts par processus
#define NFILE     100   // Max fichiers ouverts dans tout l'OS

enum file_type { FD_NONE, FD_PIPE, FD_INODE };

// Le Tampon Circulaire
struct pipe {
    char data[PIPE_SIZE];
    unsigned int nread;     // Curseur de lecture (absolu)
    unsigned int nwrite;    // Curseur d'écriture (absolu)
    int read_open;          // Le côté lecture est-il ouvert ?
    int write_open;         // Le côté écriture est-il ouvert ?
};

// L'Abstraction de Fichier
struct file {
    enum file_type type;
    int ref;                // Compteur de références (pour l'héritage fork)
    char readable;
    char writable;
    struct pipe *pipe;      // Pointeur vers l'objet pipe spécifique
};

// Prototypes de Fonctions
void fs_init(void);
struct file* file_alloc(void);
void file_close(struct file *f);
int pipe_alloc(struct file **f0, struct file **f1);
int pipe_write(struct pipe *p, char *addr, int n);
int pipe_read(struct pipe *p, char *addr, int n);

#endif
```

FD_NONE indique un emplacement libre (aucun fichier), FD_PIPE désigne un canal de communication temporaire en mémoire RAM, et FD_INODE représente un fichier réel stocké physiquement sur le disque.

---

## 2. Améliorations de l'Ordonnanceur (Scheduler)

L'ordonnanceur doit gérer les tâches qui ne sont pas prêtes à s'exécuter (tâches endormies).

### États des Tâches
Mise à jour de `include/sched.h` pour inclure l'état `TASK_INTERRUPTIBLE`. Cet état signifie que le processus attend un signal ou un événement.

```c
#define TASK_RUNNING         0
#define TASK_INTERRUPTIBLE   1  // Nouvel état : Endormi
#define TASK_ZOMBIE          2
```

Nous ajoutons également la table `ofile` (Fichiers Ouverts) à la `task_struct`.

```c
struct task_struct {
    // ... contexte CPU existant ...
    struct file *ofile[NOFILE]; // Table des descripteurs de fichiers
    void *chan;                 // Le canal sur lequel nous dormons
};
```

### Logique de Sleep et Wakeup
Dans `src/sched.c`, nous implémentons la logique pour mettre un processus en sommeil et le réveiller.

* **Sleep On** : Met le processus courant à `TASK_INTERRUPTIBLE`, enregistre ce qu'il attend (`chan`), et cède le CPU.
* **Wake Up** : Scanne toutes les tâches. Si une tâche dort sur le `chan` cible, elle est remise à `TASK_RUNNING`.

```c
void sleep_on(void *chan) {
    current->state = TASK_INTERRUPTIBLE;
    current->chan = chan;
    schedule(); // Changement de contexte immédiat
    current->chan = 0; // Réinitialisation au réveil
}

void wake_up(void *chan) {
    for(int i = 0; i < NR_TASKS; i++) {
        struct task_struct *p = task[i];
        if(p && p->state == TASK_INTERRUPTIBLE && p->chan == chan) {
            p->state = TASK_RUNNING; // Rend éligible à l'ordonnancement
        }
    }
}
```

---

## 3. Implémentation du Système de Fichiers

C'est la logique centrale située dans `src/fs.c`. Elle gère la table globale des fichiers et le comportement du pipe.

### Allocation de Pipe
Lorsque `pipe_alloc` est appelé, nous :
1.  Allouons une page mémoire physique pour la `struct pipe`.
2.  Allouons deux slots `struct file` dans la table globale.
3.  Lions un fichier en **Lecture Seule** et l'autre en **Écriture Seule**.

### Lecture (Bloquante)
L'opération de lecture doit gérer le cas du tampon vide.

```c
int pipe_read(struct pipe *p, char *addr, int n) {
    int i;
    // BOUCLE BLOQUANTE
    while(p->nread == p->nwrite) { // Le tampon est vide
        if(p->write_open == 0) return 0; // L'écrivain a fermé ? Retourne EOF (0)
        if(current->state == TASK_ZOMBIE) return -1;
        
        sleep_on(&p->nread); // Dort jusqu'à l'événement 'nread'
    }

    // Lecture des octets
    for(i = 0; i < n; i++) {
        if(p->nread == p->nwrite) break;
        addr[i] = p->data[p->nread++ % PIPE_SIZE]; // Accès circulaire
    }
    
    wake_up(&p->nwrite); // Réveille tout écrivain attendant de la place
    return i;
}
```

### Écriture (Bloquante)
L'opération d'écriture doit gérer le cas du tampon plein.

```c
int pipe_write(struct pipe *p, char *addr, int n) {
    int i;
    for(i = 0; i < n; i++) {
        // BOUCLE BLOQUANTE
        while(p->nwrite == p->nread + PIPE_SIZE) { // Le tampon est plein
            if(p->read_open == 0) return -1; // Le lecteur est parti ? Pipe brisé.
            
            wake_up(&p->nread);   // S'assure que le lecteur est réveillé
            sleep_on(&p->nwrite); // Dort jusqu'à l'événement 'nwrite' (espace libéré)
        }
        p->data[p->nwrite++ % PIPE_SIZE] = addr[i];
    }
    wake_up(&p->nread); // Données disponibles, réveille le lecteur
    return n;
}
```

---

## 4. Intégration des Appels Système

Nous exposons cette fonctionnalité à l'espace utilisateur via `src/sys.c`.

### Éviter les Flux Standards
Un détail critique est la gestion des Descripteurs de Fichiers (FDs).
* **FD 0** : Stdin (Entrée standard)
* **FD 1** : Stdout (UART / Sortie standard)
* **FD 2** : Stderr (Erreur standard)

L'implémentation de `sys_pipe` doit sauter ces FDs pour éviter les conflits. Si `sys_pipe` retournait 1 pour le côté écriture, les appels `printf` suivants (qui écrivent sur 1) iraient dans le pipe au lieu de l'écran.

```c
int sys_pipe(int *fds) {
    // ... allocations ...
    
    // Commence la recherche à l'index 3 pour protéger 0, 1, 2
    for(int i = 3; i < NOFILE; i++) { 
        // ... trouver des slots libres ...
    }
    // ...
}
```

### Fork et Héritage
Dans `src/fork.c`, lorsqu'un processus fork, il doit dupliquer sa table de fichiers. Cela permet à l'enfant d'hériter des extrémités ouvertes du pipe.

```c
// Dans copy_process()
for(int i = 0; i < NOFILE; i++) {
    if(current->ofile[i]) {
        p->ofile[i] = current->ofile[i];
        p->ofile[i]->ref++; // Incrémente le compteur de références
    }
}
```

---

## 5. Espace Utilisateur & Tests

Pour vérifier l'implémentation, nous utilisons un modèle **Producteur/Consommateur** dans `src/user.c`.

### Le Cas de Test
1.  Le Parent crée un pipe.
2.  Le Parent fork.
3.  **Enfant (Lecteur)** : Ferme l'écriture. Appelle `read()`. Comme le pipe est initialement vide, l'Enfant dort.
4.  **Parent (Écrivain)** : Ferme la lecture. Fait une pause (simule du travail). Puis écrit des données.
5.  **Résultat** : L'écriture réveille l'enfant, qui affiche le message reçu.

```c
void user_process() {
    int fds[2];
    char buf[32];
    
    call_sys_pipe(fds);
    
    if (call_sys_fork() == 0) {
        // Enfant
        call_sys_close(fds[1]); 
        call_sys_read(fds[0], buf, 32); // BLOQUE ICI
        call_sys_write(1, "Enfant a recu: ", 16);
        call_sys_write(1, buf, 13);
        call_sys_exit(); // Boucle infinie pour éviter le crash
    } else {
        // Parent
        call_sys_close(fds[0]);
        user_delay(5000000); // Attend que l'enfant s'endorme
        call_sys_write(fds[1], "Hello World", 12); // RÉVEILLE L'ENFANT
        call_sys_exit(); // Boucle infinie
    }
}
```

---

## 6. Compilation et Exécution

### Compilation
Assurez-vous que votre Makefile inclut le nouvel objet `fs.c` et que `entry.S` autorise jusqu'au syscall #6.

```bash
make
```

### Exécution
Lancez le noyau dans QEMU.

```bash
qemu-system-aarch64 -machine raspi3b -serial null -serial mon:stdio -nographic -kernel ./kernel8.img
```

### Sortie Attendue
La sortie doit démontrer la synchronisation :
```plaintext
Kernel process started. EL 1
Parent: Pause...
(Court délai...)
Parent: Ecrit!
Enfant: Je lis...
Enfant: Recu -> SALUT DU PIPE
```

---

## 7. Problèmes Connus & Optimisations

* **Nettoyage Mémoire** : Actuellement, l'appel `free_page` dans `fs.c` est commenté. Dans un OS de production, des vérifications appropriées de mappage mémoire virtuel doivent être effectuées avant de libérer des pages noyau pour éviter les exceptions `SYNC_INVALID_EL1h`.
* **Gestion des FDs** : Un allocateur par bitmap pour les descripteurs de fichiers serait plus efficace qu'une boucle de recherche linéaire.
* **Concurrence** : Sur un système multi-cœurs, des verrous (`spinlocks`) seraient nécessaires autour des compteurs de la structure pipe (`nread`, `nwrite`) pour éviter les conditions de course.