#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#include "network.h"

#define TICK_RATE 60

typedef struct {
    struct sockaddr_in addr;
    bool connected;
    double last_seen;
    NetPlayer state;
    NetProjectile projectiles[MAX_PLAYER_PROJECTILE];
    int16_t pending_damage;
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
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Erreur creation socket");
        return 1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Erreur bind (le port 8080 est peut-etre deja utilise)");
        close(sock);
        return 1;
    }

    ClientSlot clients[MAX_CLIENTS] = { 0 };
    bool building_destroyed[MAX_BUILDINGS] = { 0 };
    unsigned int map_seed = 133742;

    Drone drones[MAX_DRONE_COUNT];
    SpawnDrones(drones, MAX_DRONE_COUNT);

    printf("=== Serveur Forbidden Alliance 5v5 demarre sur le port UDP %d ===\n", SERVER_PORT);
    printf("Taille ClientPacket : %zu octets | ServerPacket : %zu octets\n", sizeof(ClientPacket), sizeof(ServerPacket));

    while (1) {
        double now = GetCurrentTimeSec();

        // --- 1. RÉCEPTION DES PAQUETS CLIENTS ---
        ClientPacket inPacket;
        struct sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);

        while (recvfrom(sock, &inPacket, sizeof(inPacket), 0, (struct sockaddr *)&senderAddr, &senderLen) == sizeof(inPacket)) {
            int slot = -1;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].connected && IsSameClient(clients[i].addr, senderAddr)) {
                    slot = i;
                    break;
                }
            }

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

                for (int target = 0; target < MAX_CLIENTS; target++) {
                    if (inPacket.damage_dealt[target] > 0 && clients[target].connected) {
                        clients[target].pending_damage += inPacket.damage_dealt[target];
                    }
                }

                for (int b = 0; b < MAX_BUILDINGS; b++) {
                    if (inPacket.building_destroyed[b]) {
                        building_destroyed[b] = true;
                    }
                }

                // Synchronisation des drones abattus ou utilisés pour réincarnation
                for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                    if (inPacket.drone_destroyed[d]) {
                        drones[d].is_alive = false;
                    }
                }
            }
        }

        // Simulation des 60 drones sur le serveur
        UpdateDrones(drones, MAX_DRONE_COUNT, 1.0f / (float)TICK_RATE);

        // --- 2. DÉCONNEXION DES JOUEURS INACTIFS (Timeout 15s) ---
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].connected && (now - clients[i].last_seen) > 15.0) {
                printf("[-] Joueur #%d deconnecte (timeout)\n", i);
                clients[i].connected = false;
            }
        }

        // --- 3. PRÉPARATION ET ENVOI DU SERVER PACKET À CHAQUE JOUEUR ---
        ServerPacket outPacket = { 0 };
        outPacket.map_seed = map_seed;
        memcpy(outPacket.building_destroyed, building_destroyed, sizeof(building_destroyed));

        for (int d = 0; d < MAX_DRONE_COUNT; d++) {
            outPacket.drones[d].pos = drones[d].pos;
            outPacket.drones[d].is_alive = drones[d].is_alive;
        }

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

        // --- 4. PAUSE DE 1 TICK (16.6 ms = 60 Hz) ---
        struct timeval sleepTime = { 0, 1000000 / TICK_RATE };
        select(0, NULL, NULL, NULL, &sleepTime);
    }

    close(sock);
    return 0;
}