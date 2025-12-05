#include "user_sys.h"
#include "user.h"


// Wrapper pour copier des chaines (si tu n'as pas string.h en user)
int strlen(char *s) {
    int i = 0;
    while(s[i]) i++;
    return i;
}

void user_process() {
    int fds[2]; // fds[0] = Read, fds[1] = Write
    char buffer[50];
    
    call_sys_write("User: Creation du Pipe...\n\r");
    
    // 1. Création du Pipe
    int ret = call_sys_pipe(fds);
    if (ret < 0) {
        call_sys_write("Erreur: Echec creation Pipe\n\r");
        call_sys_exit();
    }

    // 2. Fork
    int pid = call_sys_fork();
    
    if (pid < 0) {
        call_sys_write("Erreur: Echec Fork\n\r");
        call_sys_exit();
    }

    if (pid == 0) {
        // --- ENFANT (CONSOMMATEUR) ---
        call_sys_close(fds[1]); // Bonne pratique : fermer le bout d'écriture inutile
        
        call_sys_write("Enfant: J'attends des donnees (Reading...)\n\r");
        
        // Ceci doit BLOQUER tant que le père n'écrit pas
        int n = call_sys_read(fds[0], buffer, 50);
        
        if (n > 0) {
            buffer[n] = '\0'; // Null-terminate pour l'affichage
            call_sys_write("Enfant RECU: ");
            call_sys_write(buffer);
            call_sys_write("\n\r");
        } else {
            call_sys_write("Enfant: Erreur ou EOF\n\r");
        }

        call_sys_close(fds[0]);
        call_sys_exit();
        
    } else {
        // --- PARENT (PRODUCTEUR) ---
        call_sys_close(fds[0]); // Bonne pratique : fermer le bout de lecture inutile
        
        call_sys_write("Parent: Je dors 2 secondes...\n\r");
        user_delay(2000000); // Simulation de travail
        
        call_sys_write("Parent: J'ecris dans le pipe !\n\r");
        char *msg = "Secret Kernel Message";
        
        // Écriture dans le Ring Buffer
        call_sys_write1(fds[1], msg, strlen(msg));
        
        call_sys_close(fds[1]); // Envoie EOF à l'enfant
        
        // Attendre que l'enfant finisse (si tu as wait, sinon boucle inf)
        user_delay(1000000); 
        call_sys_exit();
    }
}