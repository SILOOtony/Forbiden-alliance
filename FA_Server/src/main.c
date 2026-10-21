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

typedef struct {
    unsigned int map_seed;
    ClientSlot clients[MAX_CLIENTS];
    bool building_destroyed[MAX_BUILDINGS];
    Drone drones[MAX_DRONE_COUNT];
    bool had_players;
} GameRoom;

static double GetCurrentTimeSec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static bool IsSameClient(struct sockaddr_in a, struct sockaddr_in b) {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
}

static void ResetRoom(GameRoom *room, int room_index) {
    room->map_seed = 133742u + (unsigned int)(room_index * 1000);
    memset(room->clients, 0, sizeof(room->clients));
    memset(room->building_destroyed, 0, sizeof(room->building_destroyed));
    SpawnDrones(room->drones, MAX_DRONE_COUNT);
    room->had_players = false;
}

static void RemoveClientFromOtherRooms(GameRoom rooms[], int except_room, struct sockaddr_in addr) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        if (r == except_room) continue;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (rooms[r].clients[i].connected && IsSameClient(rooms[r].clients[i].addr, addr)) {
                rooms[r].clients[i].connected = false;
                printf("[-] Joueur #%d a quitte le Salon #%d\n", i, r + 1);
            }
        }
    }
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

    GameRoom rooms[MAX_ROOMS];
    for (int r = 0; r < MAX_ROOMS; r++) {
        ResetRoom(&rooms[r], r);
    }

    printf("=== Serveur Multi-Rooms (%d Salons 5v5) demarre sur le port UDP %d ===\n", MAX_ROOMS, SERVER_PORT);
    printf("LobbyPacket : %zu octets | ClientPacket : %zu octets | ServerPacket : %zu octets\n",
           sizeof(LobbyClientPacket), sizeof(ClientPacket), sizeof(ServerPacket));

    while (1) {
        double now = GetCurrentTimeSec();

        // --- 1. RÉCEPTION ET AIGUILLAGE DES PAQUETS (LOBBY vs GAME) ---
        uint8_t recvBuffer[sizeof(ServerPacket)];
        struct sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);
        ssize_t received;

        while ((received = recvfrom(sock, recvBuffer, sizeof(recvBuffer), 0, (struct sockaddr *)&senderAddr, &senderLen)) > 0) {
            uint8_t packetType = recvBuffer[0];

            // A. Paquet de Lobby (Menu des salons)
            if (packetType == PACKET_LOBBY && received == sizeof(LobbyClientPacket)) {
                LobbyClientPacket lobbyIn;
                memcpy(&lobbyIn, recvBuffer, sizeof(LobbyClientPacket));

                if (lobbyIn.action == 1) {
                    RemoveClientFromOtherRooms(rooms, -1, senderAddr);
                }

                LobbyServerPacket lobbyOut = { 0 };
                lobbyOut.type = PACKET_LOBBY;
                for (int r = 0; r < MAX_ROOMS; r++) {
                    uint8_t count = 0;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (rooms[r].clients[i].connected) count++;
                    }
                    lobbyOut.room_players[r] = count;
                }

                sendto(sock, &lobbyOut, sizeof(lobbyOut), 0, (struct sockaddr *)&senderAddr, senderLen);
            }
            // B. Paquet de Jeu (Partie en cours dans un salon)
            else if (packetType == PACKET_GAME && received == sizeof(ClientPacket)) {
                ClientPacket inPacket;
                memcpy(&inPacket, recvBuffer, sizeof(ClientPacket));

                int r = inPacket.room_id;
                if (r < 0 || r >= MAX_ROOMS) continue;

                // Nettoie la présence éventuelle du joueur dans un autre salon
                RemoveClientFromOtherRooms(rooms, r, senderAddr);

                GameRoom *room = &rooms[r];
                int slot = -1;

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (room->clients[i].connected && IsSameClient(room->clients[i].addr, senderAddr)) {
                        slot = i;
                        break;
                    }
                }

                if (slot == -1) {
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (!room->clients[i].connected) {
                            slot = i;
                            room->clients[i].connected = true;
                            room->clients[i].addr = senderAddr;
                            room->clients[i].pending_damage = 0;
                            room->had_players = true;
                            printf("[+] Salon #%d : Joueur #%d connecte (%s:%d)\n",
                                   r + 1, slot, inet_ntoa(senderAddr.sin_addr), ntohs(senderAddr.sin_port));
                            break;
                        }
                    }
                }

                if (slot != -1) {
                    room->clients[slot].last_seen = now;
                    room->clients[slot].state = inPacket.player;
                    room->clients[slot].state.team = (slot < 5) ? 0 : 1;
                    memcpy(room->clients[slot].projectiles, inPacket.projectiles, sizeof(inPacket.projectiles));

                    for (int target = 0; target < MAX_CLIENTS; target++) {
                        if (inPacket.damage_dealt[target] > 0 && room->clients[target].connected) {
                            room->clients[target].pending_damage += inPacket.damage_dealt[target];
                        }
                    }

                    for (int b = 0; b < MAX_BUILDINGS; b++) {
                        if (inPacket.building_destroyed[b]) {
                            room->building_destroyed[b] = true;
                        }
                    }

                    for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                        if (inPacket.drone_destroyed[d]) {
                            room->drones[d].is_alive = false;
                        }
                    }
                }
            }
        }

        // --- 2. MISE À JOUR ET DIFFUSION POUR CHAQUE SALON ACTIF ---
        for (int r = 0; r < MAX_ROOMS; r++) {
            GameRoom *room = &rooms[r];
            int activePlayers = 0;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (room->clients[i].connected) {
                    if ((now - room->clients[i].last_seen) > 15.0) {
                        printf("[-] Salon #%d : Joueur #%d deconnecte (timeout)\n", r + 1, i);
                        room->clients[i].connected = false;
                    } else {
                        activePlayers++;
                    }
                }
            }

            // Si tous les joueurs ont quitté le salon, on le remet à neuf pour la prochaine partie
            if (activePlayers == 0) {
                if (room->had_players) {
                    printf("[*] Salon #%d vide : reinitialisation de la carte et des drones.\n", r + 1);
                    ResetRoom(room, r);
                }
                continue;
            }

            UpdateDrones(room->drones, MAX_DRONE_COUNT, 1.0f / (float)TICK_RATE);

            ServerPacket outPacket = { 0 };
            outPacket.type = PACKET_GAME;
            outPacket.room_id = (int8_t)r;
            outPacket.map_seed = room->map_seed;
            memcpy(outPacket.building_destroyed, room->building_destroyed, sizeof(room->building_destroyed));

            for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                outPacket.drones[d].pos = room->drones[d].pos;
                outPacket.drones[d].is_alive = room->drones[d].is_alive;
            }

            for (int i = 0; i < MAX_CLIENTS; i++) {
                outPacket.connected[i] = room->clients[i].connected;
                outPacket.players[i] = room->clients[i].state;

                for (int p = 0; p < MAX_PLAYER_PROJECTILE; p++) {
                    outPacket.projectiles[i * MAX_PLAYER_PROJECTILE + p] = room->clients[i].projectiles[p];
                }
            }

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!room->clients[i].connected) continue;
                outPacket.your_id = (int8_t)i;
                outPacket.damage_received = room->clients[i].pending_damage;
                room->clients[i].pending_damage = 0;

                sendto(sock, &outPacket, sizeof(outPacket), 0, (struct sockaddr *)&room->clients[i].addr, sizeof(room->clients[i].addr));
            }
        }

        // --- 3. PAUSE DE 1 TICK (16.6 ms = 60 Hz) ---
        struct timeval sleepTime = { 0, 1000000 / TICK_RATE };
        select(0, NULL, NULL, NULL, &sleepTime);
    }

    close(sock);
    return 0;
}