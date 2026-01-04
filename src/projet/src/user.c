#include "user_sys.h"
#include "user.h"
//#include "printf.h"

// ==========================================
// 1. OUTILS D'AFFICHAGE (Mini-Lib C User)
// ==========================================

void user_delay(unsigned long count); // Défini en ASM

int strlen(char *s) {
    int i = 0;
    while(s[i]) i++;
    return i;
}

// Affiche une chaine brute
void print(char *s) {
    call_sys_write(1, s, strlen(s));
}

// Affiche un entier
void print_int(int n) {
    char buffer[16];
    int i = 0;
    if (n == 0) {
        print("0");
        return;
    }
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    // Inverse
    for (int j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = tmp;
    }
    call_sys_write(1, buffer, i);
}

void print_ln() {
    print("\n");
}

void assert_equals(char *ctx, char expected, char actual) {
    if (expected != actual) {
        print("\n[FAIL] "); print(ctx);
        print(" Attendu: '"); char t[2] = {expected, 0}; print(t);
        print("' Recu: '"); char u[2] = {actual, 0}; print(u);
        print("'\n");
        call_sys_exit(); // Arrêt d'urgence
    }
}

// ==========================================
// TEST 1 : INTEGRITE CIRCULAIRE (WRAP-AROUND)
// ==========================================
void test_circular_integrity() {
    int fds[2];
    int TOTAL_BYTES = 5000; // > 4096 pour forcer le wrap
    
    print("\n========================================\n");
    print("[TEST 1] Buffer Circulaire & Integrite\n");
    print("OBJECTIF: Ecrire 5000 octets (taille buffer 4096)\n");
    print("          Verifier qu'aucune donnee n'est perdue.\n");
    print("========================================\n");

    call_sys_pipe(fds);

    if (call_sys_fork() == 0) {
        // --- ENFANT (Consommateur) ---
        call_sys_close(fds[1]);
        char c;
        
        for(int i = 0; i < TOTAL_BYTES; i++) {
            call_sys_read(fds[0], &c, 1);
            
            // Vérification stricte
            char expected = 'A' + (i % 26);
            assert_equals("Data Integrity", expected, c);

            // Barre de progression tous les 500 octets
            if (i % 500 == 0) {
                print("[INFO] Lecture octet "); 
                print_int(i); print("/"); print_int(TOTAL_BYTES);
                print(" -> OK\n");
            }
        }
        print("[SUCCESS] Integrity Check: 100% Passed.\n");
        call_sys_close(fds[0]);
        call_sys_exit();
    } else {
        // --- PARENT (Producteur) ---
        call_sys_close(fds[0]);
        char c = 'A';
        
        for(int i = 0; i < TOTAL_BYTES; i++) {
            call_sys_write(fds[1], &c, 1);
            c++;
            if (c > 'Z') c = 'A';
        }
        
        call_sys_wait(0);
        print("[TEST 1] RESULTAT: SUCCES COMPLET\n");
        call_sys_close(fds[1]);
        // Pas de exit ici pour enchainer les tests
    }
}

// ==========================================
// TEST 2 : SYNCHRONISATION (PING-PONG)
// ==========================================
void test_ping_pong() {
    int p1[2], p2[2];
    char buf[10];
    int ROUNDS = 3;

    print("\n========================================\n");
    print("[TEST 2] Synchronisation Ping-Pong\n");
    print("OBJECTIF: Echanges bidirectionnels sans Deadlock.\n");
    print("========================================\n");
    
    call_sys_pipe(p1); // P -> E
    call_sys_pipe(p2); // E -> P

    if (call_sys_fork() == 0) {
        // --- ENFANT ---
        call_sys_close(p1[1]); call_sys_close(p2[0]);
        
        for(int i=1; i<=ROUNDS; i++) {
            call_sys_read(p1[0], buf, 4); // Reçoit PING
            
            print("   [Enfant] Recu: "); print(buf);
            print(" -> Renvoi PONG\n");
            
            call_sys_write(p2[1], "PONG", 4);
        }
        call_sys_exit();
    } else {
        // --- PARENT ---
        call_sys_close(p1[0]); call_sys_close(p2[1]);
        
        for(int i=1; i<=ROUNDS; i++) {
            print("[Round "); print_int(i); print("] Envoi PING...\n");
            
            call_sys_write(p1[1], "PING", 4);
            call_sys_read(p2[0], buf, 4); // Reçoit PONG
            
            print("[Round "); print_int(i); print("] Parent Recu: "); 
            print(buf); print_ln();
        }
        
        call_sys_wait(0);
        print("[TEST 2] RESULTAT: SUCCES COMPLET\n");
        // Nettoyage
        call_sys_close(p1[1]); call_sys_close(p2[0]);
    }
}

// ==========================================
// TEST 3 : PRESSION HYDRAULIQUE (BLOCKING)
// ==========================================
void test_pressure() {
    int fds[2];
    char chunk[128]; 
    int CHUNK_COUNT = 40; // 40 * 128 = 5120 octets (> 4096)
    
    print("\n========================================\n");
    print("[TEST 3] Stress Test (Blocking I/O)\n");
    print("OBJECTIF: Saturer le pipe et verifier le Sleep/Wakeup.\n");
    print("          Le parent doit BLOQUER quand le buffer est plein.\n");
    print("========================================\n");

    call_sys_pipe(fds);

    if (call_sys_fork() == 0) {
        // --- ENFANT (LENT) ---
        call_sys_close(fds[1]);
        
        print("[Enfant] Je dors 2 secondes pour laisser le parent saturer...\n");
        user_delay(4000000); // Grosse pause
        
        print("[Enfant] Je me reveille. Je commence a vider.\n");
        
        for(int i=0; i<CHUNK_COUNT; i++) {
            // Lecture lente
            call_sys_read(fds[0], chunk, 128);
            
            if(i % 10 == 0) {
                print("   [Enfant] Vianne ouverte: "); 
                print_int((i+1)*128); print(" octets lus.\n");
            }
            user_delay(200000); // Petite pause entre les lectures
        }
        
        print("[Enfant] Tout lu. Termine.\n");
        call_sys_exit();
    } else {
        // --- PARENT (RAPIDE) ---
        call_sys_close(fds[0]);
        
        print("[Parent] J'ecris massivement (5120 octets)...\n");
        
        for(int i=0; i<CHUNK_COUNT; i++) {
            // Remplissage rapide
            call_sys_write(fds[1], chunk, 128);
            
            if(i == 31) { // 32 * 128 = 4096
                 print("[Parent] !!! BUFFER PLEIN (4096) !!! Je devrais bloquer ici...\n");
            }
        }
        
        print("[Parent] Ecriture terminee (Debloque) !\n");
        call_sys_wait(0);
        print("[TEST 3] RESULTAT: SUCCES COMPLET\n");
        call_sys_close(fds[1]);
        call_sys_exit();
    }
}


// ==========================================
// MAIN LOOP
// ==========================================
void user_process() {
    print("\n[INIT] Demarrage de la suite de tests PIPE...\n");
    
    test_circular_integrity();
    test_ping_pong();
    test_pressure();
    
    print("\n[FIN] TOUS LES TESTS SONT PASSES AVEC SUCCES !\n");
    print("      Arret du systeme (Boucle infinie).\n");
    while(1);
}