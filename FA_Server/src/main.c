#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#include "network.h"
#include "map.h"
#include "weapons.h"
#include "weaponSlot.h"

#define TICK_RATE 60

typedef struct {
    bool active;
    struct sockaddr_in addr;
    double last_seen;
    PlayerState state;
    Projectile projectile[MAX_PLAYER_PROJECTILE];
} ClientSlot;

static double GetCurrentTimeSec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static bool IsSameClient(struct sockaddr_in a, struct sockaddr_in b) {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
}

int main(void) {
    // Initialisation de la map sur le serveur avec une graine (seed)
    unsigned int serverSeed = 133742;
    GameMap cityMap;
    InitMap(&cityMap, serverSeed);

    //Création des projectiles
    Projectile projectile[MAX_PROJECTILES] = { 0 };


    // 1. Création du socket UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Erreur creation socket");
        return 1;
    }

    // 2. Passage du socket en mode NON-BLOQUANT
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // 3. Configuration de l'adresse du serveur (Port 8080)
    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // 4. Réservation du port avec bind()
    if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Erreur bind (le port 8080 est peut-etre deja utilise)");
        close(sock);
        return 1;
    }

    printf("=== SERVEUR FORBIDDEN ALLIANCE DEMARRE SUR LE PORT %d (Seed: %u) ===\n", SERVER_PORT, serverSeed);
    printf("Taille de ServerPacket : %zu octets (Limite UDP macOS : 9216 octets)\n", sizeof(ServerPacket));

    ClientSlot clients[MAX_CLIENTS] = { 0 };

    while (1) {
        double now = GetCurrentTimeSec();

        // --- A. RÉCEPTION DES PAQUETS DES CLIENTS ---
        ClientPacket inPacket;
        struct sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);

        while (recvfrom(sock, &inPacket, sizeof(inPacket), 0, (struct sockaddr *)&senderAddr, &senderLen) == sizeof(inPacket)) {
            int slotIndex = -1;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active && IsSameClient(clients[i].addr, senderAddr)) {
                    slotIndex = i;
                    break;
                }
            }

            if (slotIndex == -1) {
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (!clients[i].active) {
                        slotIndex = i;
                        clients[i].active = true;
                        clients[i].addr = senderAddr;
                        printf("[+] Joueur #%d connecte (%s:%d)\n", i, inet_ntoa(senderAddr.sin_addr), ntohs(senderAddr.sin_port));
                        break;
                    }
                }
            }

            if (slotIndex != -1) {
                clients[slotIndex].last_seen = now;
                clients[slotIndex].state.position = inPacket.position;
                clients[slotIndex].state.pitch = inPacket.pitch;
                clients[slotIndex].state.yaw = inPacket.yaw;
                clients[slotIndex].state.roll = inPacket.roll;
                clients[slotIndex].state.hp = inPacket.hp;
                clients[slotIndex].state.is_alive = inPacket.is_alive;
                memcpy(clients[slotIndex].projectile, inPacket.projectiles, sizeof(clients[slotIndex].projectile));

                // Synchronise les immeubles détruits par ce joueur sur la carte du serveur
                for (int b = 0; b < cityMap.building_count; b++) {
                    if (inPacket.building_destroyed[b]) {
                        cityMap.buildings[b].is_destroyed = true;
                    }
                }
            }
        }

        // --- B. DÉCONNEXION DES JOUEURS INACTIFS (Timeout 3 secondes) ---
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && (now - clients[i].last_seen > 15.0)) {
                clients[i].active = false;
                printf("[-] Joueur #%d deconnecte (timeout)\n", i);
            }
        }

        // 1. On rassemble tous les projectiles UNE SEULE FOIS par Tick
        int projIndex = 0;
        for (int j = 0; j < MAX_CLIENTS; j++) {
            if (!clients[j].active) continue;
            for (int p = 0; p < MAX_PLAYER_PROJECTILE; p++) {
                projectile[projIndex++] = clients[j].projectile[p];
            }
        }

        // --- C. DIFFUSION DE L'ÉTAT DU MONDE À TOUS LES CLIENTS ---
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) continue;

            ServerPacket outPacket = { 0 };
            outPacket.your_id = i;
            outPacket.map_seed = cityMap.seed;

            for (int j = 0; j < MAX_CLIENTS; j++) {
                outPacket.connected[j] = clients[j].active;
                outPacket.players[j] = clients[j].state;
            }

            for (int b = 0; b < cityMap.building_count; b++) {
                outPacket.building_destroyed[b] = cityMap.buildings[b].is_destroyed;
            }         

            memcpy(outPacket.projectiles, projectile, sizeof(outPacket.projectiles));
        
            sendto(sock, &outPacket, sizeof(outPacket), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
        }

        // --- D. PAUSE DE 1 TICK (16.6 millisecondes = 60 Hz) ---
        struct timeval sleepTime = { 0, 1000000 / TICK_RATE };
        select(0, NULL, NULL, NULL, &sleepTime);
    }

    close(sock);
    return 0;
}