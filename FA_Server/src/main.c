#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include "network.h"

typedef struct {
    struct sockaddr_in addr;
    bool connected;
    double last_seen;
    NetPlayer state;
    NetProjectile projectiles[MAX_PLAYER_PROJECTILE];
    int16_t pending_damage;
} ClientSlot;

static double GetCurrentTimeSec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    ClientSlot clients[MAX_CLIENTS] = { 0 };
    bool building_destroyed[MAX_BUILDINGS] = { 0 };
    unsigned int map_seed = 133742;

    printf("=== Serveur Forbidden Alliance 5v5 demarre sur le port UDP %d ===\n", SERVER_PORT);
    printf("Taille ClientPacket : %zu octets | ServerPacket : %zu octets\n", sizeof(ClientPacket), sizeof(ServerPacket));

    const double tick_dt = 1.0 / 60.0;

    while (1) {
        double now = GetCurrentTimeSec();

        // --- 1. RÉCEPTION DES PAQUETS CLIENTS ---
        ClientPacket inPacket;
        struct sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);

        while (recvfrom(sock, &inPacket, sizeof(inPacket), 0, (struct sockaddr *)&senderAddr, &senderLen) == sizeof(inPacket)) {
            int slot = -1;

            // Cherche si le client est déjà connecté
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].connected &&
                    clients[i].addr.sin_addr.s_addr == senderAddr.sin_addr.s_addr &&
                    clients[i].addr.sin_port == senderAddr.sin_port) {
                    slot = i;
                    break;
                }
            }

            // Sinon, lui attribue une place libre (0 à 4 = Équipe A, 5 à 9 = Équipe B)
            if (slot == -1) {
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (!clients[i].connected) {
                        slot = i;
                        clients[i].connected = true;
                        clients[i].addr = senderAddr;
                        clients[i].pending_damage = 0;
                        printf("[+] Joueur #%d connecte (%s:%d)\n", slot, inet_ntoa(senderAddr.sin_addr), ntohs(senderAddr.sin_port));
                        break;
                    }
                }
            }

            if (slot != -1) {
                clients[slot].last_seen = now;
                clients[slot].state = inPacket.player;
                clients[slot].state.team = (slot < 5) ? 0 : 1;
                memcpy(clients[slot].projectiles, inPacket.projectiles, sizeof(inPacket.projectiles));

                // Cumul des dégâts infligés aux autres joueurs
                for (int target = 0; target < MAX_CLIENTS; target++) {
                    if (inPacket.damage_dealt[target] > 0 && clients[target].connected) {
                        clients[target].pending_damage += inPacket.damage_dealt[target];
                    }
                }

                // Synchronisation des bâtiments détruits
                for (int b = 0; b < MAX_BUILDINGS; b++) {
                    if (inPacket.building_destroyed[b]) {
                        building_destroyed[b] = true;
                    }
                }
            }
        }

        // --- 2. DÉCONNEXION DES JOUEURS INACTIFS (Timeout 5s) ---
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].connected && (now - clients[i].last_seen) > 5.0) {
                printf("[-] Joueur #%d deconnecte (timeout)\n", i);
                clients[i].connected = false;
            }
        }

        // --- 3. PRÉPARATION ET ENVOI DU SERVER PACKET À CHAQUE JOUEUR ---
        ServerPacket outPacket = { 0 };
        outPacket.map_seed = map_seed;
        memcpy(outPacket.building_destroyed, building_destroyed, sizeof(building_destroyed));

        for (int i = 0; i < MAX_CLIENTS; i++) {
            outPacket.connected[i] = clients[i].connected;
            outPacket.players[i] = clients[i].state;

            for (int p = 0; p < MAX_PLAYER_PROJECTILE; p++) {
                outPacket.projectiles[i * MAX_PLAYER_PROJECTILE + p] = clients[i].projectiles[p];
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].connected) continue;
            outPacket.your_id = (int8_t)i;
            outPacket.damage_received = clients[i].pending_damage;
            clients[i].pending_damage = 0;

            sendto(sock, &outPacket, sizeof(outPacket), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
        }

        usleep((useconds_t)(tick_dt * 1000000.0));
    }

    close(sock);
    return 0;
}