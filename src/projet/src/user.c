#include "user_sys.h"
#include "user.h"
#include "printf.h"

// Petite fonction utilitaire pour la longueur de chaine
int strlen(char *s) {
    int i = 0;
    while(s[i]) i++;
    return i;
}

#include "user_sys.h"

void user_delay(unsigned long count);

void user_process() {
    int fds[2];
    int pid;
    char buf[32]; 

    // Création du pipe
    if (call_sys_pipe(fds) < 0) {
        call_sys_write(1, "Erreur Pipe\n", 12);
        while(1); // On bloque ici en cas d'erreur
    }

    pid = call_sys_fork();

    if (pid == 0) {
        // --- ENFANT ---
        call_sys_close(fds[1]); 

        // On écrit caractère par caractère pour être sûr que ça passe
        call_sys_write(1, "Enfant: Je lis...\n", 18);
        
        // --- ETAPE BLOQUANTE ---
        // Si le pipe marche, l'enfant s'endort ici.
        int n = call_sys_read(fds[0], buf, 32);
        // -----------------------

        call_sys_write(1, "Enfant: Recu -> ", 16);
        call_sys_write(1, buf, n);
        call_sys_write(1, "\n", 1);
        
        call_sys_close(fds[0]);
        
        // --- FIX : ON NE MEURT PAS ---
        // On remplace sys_exit par une boucle infinie pour éviter le crash mémoire
        while(1) { user_delay(100000); } 
    } else {
        // --- PARENT ---
        call_sys_close(fds[0]);

        call_sys_write(1, "Parent: Pause...\n", 17);
        user_delay(10000000); // Pause plus longue
        
        call_sys_write(1, "Parent: Ecrit!\n", 15);
        call_sys_write(fds[1], "SALUT DU PIPE", 13);
        
        call_sys_close(fds[1]);
        
        // --- FIX : ON NE MEURT PAS ---
        while(1) { user_delay(100000); }
    }
}