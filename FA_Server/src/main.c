#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>

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
    GameMap map;
    ClientSlot clients[MAX_CLIENTS];
    Drone drones[MAX_DRONE_COUNT];
    bool had_players;

    uint8_t current_round;
    float round_timer;
    uint16_t team_buildings_destroyed[2];
    uint32_t team_city_damage[2];
} GameRoom;

static double GetCurrentTimeSec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static bool IsSameClient(struct sockaddr_in a, struct sockaddr_in b) {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
}

// Nettoie l'identifiant pour éviter tout caractère interdit dans le nom du fichier JSON
static void SanitizeUsername(const char *in, char *out, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < max_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '_' || c == '-') {
            out[j++] = (char)tolower(c);
        }
    }
    out[j] = '\0';
    if (j == 0) {
        strcpy(out, "pilote");
    }
}

static void SaveAccountJSON(const char *clean_user, uint8_t plane_class, int xp, int gems) {
    mkdir("FA_Server", 0755);
    mkdir("FA_Server/accounts", 0755);

    char path[128];
    snprintf(path, sizeof(path), "FA_Server/accounts/%s.json", clean_user);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"username\": \"%s\",\n", clean_user);
    fprintf(f, "  \"plane_class\": %u,\n", (unsigned int)plane_class);
    fprintf(f, "  \"xp\": %d,\n", xp);
    fprintf(f, "  \"gems\": %d\n", gems);
    fprintf(f, "}\n");
    fclose(f);
}

static void LoadOrCreateAccountJSON(const char *raw_user, char *clean_out, uint8_t *out_class, int *out_xp, int *out_gems) {
    SanitizeUsername(raw_user, clean_out, MAX_USERNAME_LEN);
    mkdir("FA_Server", 0755);
    mkdir("FA_Server/accounts", 0755);

    char path[128];
    snprintf(path, sizeof(path), "FA_Server/accounts/%s.json", clean_out);

    *out_class = 0;
    *out_xp = 0;
    *out_gems = 0;

    FILE *f = fopen(path, "r");
    if (!f) {
        // Le compte n'existe pas encore : on le crée sur le VPS
        SaveAccountJSON(clean_out, 0, 0, 0);
        printf("[Compte JSON] Nouveau compte cree : %s\n", path);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned int cls = 0;
        int val = 0;
        if (sscanf(line, " \"plane_class\": %u", &cls) == 1) {
            if (cls <= 2) *out_class = (uint8_t)cls;
        } else if (sscanf(line, " \"xp\": %d", &val) == 1) {
            *out_xp = val;
        } else if (sscanf(line, " \"gems\": %d", &val) == 1) {
            *out_gems = val;
        }
    }
    fclose(f);
    printf("[Compte JSON] Compte charge : %s (Classe: %u)\n", path, *out_class);
}

static void StartRoomRound(GameRoom *room, uint8_t round_num) {
    room->current_round = round_num;
    room->round_timer = (round_num <= 2) ? ROUND_DURATION_SEC : 12.0f;
    InitMap(&room->map, room->map_seed);
    SpawnDrones(room->drones, MAX_DRONE_COUNT);
}

static void ResetRoom(GameRoom *room, int room_index) {
    room->map_seed = 133742u + (unsigned int)(room_index * 1000);
    memset(room->clients, 0, sizeof(room->clients));
    room->had_players = false;
    room->team_buildings_destroyed[0] = 0;
    room->team_buildings_destroyed[1] = 0;
    room->team_city_damage[0] = 0;
    room->team_city_damage[1] = 0;
    StartRoomRound(room, 1);
}

static uint8_t CountRoomPlayers(const GameRoom *room) {
    uint8_t count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (room->clients[i].connected) count++;
    }
    return count;
}

static int FindClientRoom(GameRoom rooms[], struct sockaddr_in addr) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (rooms[r].clients[i].connected && IsSameClient(rooms[r].clients[i].addr, addr)) {
                return r;
            }
        }
    }
    return -1;
}

static void RemoveClientFromOtherRooms(GameRoom rooms[], int except_room, struct sockaddr_in addr) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        if (r == except_room) continue;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (rooms[r].clients[i].connected && IsSameClient(rooms[r].clients[i].addr, addr)) {
                rooms[r].clients[i].connected = false;
                printf("[-] Joueur #%d a quitte la Room #%d\n", i, r + 1);
            }
        }
    }
}

static int RegisterOrUpdateClientInRoom(GameRoom *room, int room_id, struct sockaddr_in addr, double now) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (room->clients[i].connected && IsSameClient(room->clients[i].addr, addr)) {
            room->clients[i].last_seen = now;
            return i;
        }
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!room->clients[i].connected) {
            room->clients[i].connected = true;
            room->clients[i].addr = addr;
            room->clients[i].last_seen = now;
            room->clients[i].pending_damage = 0;
            memset(&room->clients[i].state, 0, sizeof(NetPlayer));
            memset(room->clients[i].projectiles, 0, sizeof(room->clients[i].projectiles));
            room->had_players = true;
            printf("[+] Room #%d : Joueur #%d rejoint (%s:%d)\n",
                   room_id + 1, i, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            return i;
        }
    }
    return -1;
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

    printf("=== Serveur Multi-Rooms (%d Rooms 5v5) demarre sur le port UDP %d ===\n", MAX_ROOMS, SERVER_PORT);
    printf("Tailles -> LobbyClient: %zu o | LobbyServer: %zu o | GameClient: %zu o | GameServer: %zu o\n",
           sizeof(LobbyClientPacket), sizeof(LobbyServerPacket), sizeof(ClientPacket), sizeof(ServerPacket));

    const float tick_dt = 1.0f / (float)TICK_RATE;

    while (1) {
        double now = GetCurrentTimeSec();

        // --- 1. RÉCEPTION DES PAQUETS ---
        uint8_t recvBuffer[sizeof(ServerPacket)];
        struct sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);
        ssize_t received;

        while ((received = recvfrom(sock, recvBuffer, sizeof(recvBuffer), 0, (struct sockaddr *)&senderAddr, &senderLen)) > 0) {
            uint8_t packetType = recvBuffer[0];

            if (packetType == PACKET_LOBBY && received == sizeof(LobbyClientPacket)) {
                LobbyClientPacket lobbyIn;
                memcpy(&lobbyIn, recvBuffer, sizeof(LobbyClientPacket));
                lobbyIn.username[MAX_USERNAME_LEN - 1] = '\0';

                LobbyServerPacket lobbyOut = {
                    .type = PACKET_LOBBY,
                    .room_id = lobbyIn.room_id,
                    .connected_count = 0,
                    .plane_class = lobbyIn.plane_class,
                    .account_loaded = false
                };
                strncpy(lobbyOut.username, lobbyIn.username, MAX_USERNAME_LEN - 1);

                if (lobbyIn.action == LOBBY_LOGIN) {
                    char cleanUser[MAX_USERNAME_LEN];
                    uint8_t savedClass = 0;
                    int xp = 0, gems = 0;
                    LoadOrCreateAccountJSON(lobbyIn.username, cleanUser, &savedClass, &xp, &gems);
                    lobbyOut.plane_class = savedClass;
                    lobbyOut.account_loaded = true;
                    strncpy(lobbyOut.username, cleanUser, MAX_USERNAME_LEN - 1);
                }
                else if (lobbyIn.action == LOBBY_SET_CLASS) {
                    char cleanUser[MAX_USERNAME_LEN];
                    uint8_t oldClass = 0;
                    int xp = 0, gems = 0;
                    LoadOrCreateAccountJSON(lobbyIn.username, cleanUser, &oldClass, &xp, &gems);
                    uint8_t newClass = (lobbyIn.plane_class <= 2) ? lobbyIn.plane_class : 0;
                    SaveAccountJSON(cleanUser, newClass, xp, gems);
                    lobbyOut.plane_class = newClass;
                    lobbyOut.account_loaded = true;
                    strncpy(lobbyOut.username, cleanUser, MAX_USERNAME_LEN - 1);
                }
                else if (lobbyIn.action == LOBBY_LEAVE) {
                    RemoveClientFromOtherRooms(rooms, -1, senderAddr);
                    lobbyOut.room_id = -1;
                }
                else if (lobbyIn.action == LOBBY_CREATE) {
                    RemoveClientFromOtherRooms(rooms, -1, senderAddr);
                    int chosen = -1;
                    for (int r = 0; r < MAX_ROOMS; r++) {
                        if (CountRoomPlayers(&rooms[r]) == 0) { chosen = r; break; }
                    }
                    if (chosen == -1) {
                        for (int r = 0; r < MAX_ROOMS; r++) {
                            if (CountRoomPlayers(&rooms[r]) < MAX_CLIENTS) { chosen = r; break; }
                        }
                    }
                    if (chosen != -1 && RegisterOrUpdateClientInRoom(&rooms[chosen], chosen, senderAddr, now) != -1) {
                        lobbyOut.room_id = (int8_t)chosen;
                        lobbyOut.connected_count = CountRoomPlayers(&rooms[chosen]);
                    }
                }
                else if (lobbyIn.action == LOBBY_JOIN || lobbyIn.action == LOBBY_HEARTBEAT) {
                    int r = lobbyIn.room_id;
                    if (r >= 0 && r < MAX_ROOMS) {
                        RemoveClientFromOtherRooms(rooms, r, senderAddr);
                        if (RegisterOrUpdateClientInRoom(&rooms[r], r, senderAddr, now) != -1) {
                            lobbyOut.room_id = (int8_t)r;
                            lobbyOut.connected_count = CountRoomPlayers(&rooms[r]);
                        }
                    }
                }

                // Si le joueur était déjà dans une room lors d'un LOGIN/SET_CLASS, on maintient son compteur à jour
                if (lobbyOut.room_id >= 0 && lobbyOut.room_id < MAX_ROOMS) {
                    lobbyOut.connected_count = CountRoomPlayers(&rooms[lobbyOut.room_id]);
                }

                sendto(sock, &lobbyOut, sizeof(lobbyOut), 0, (struct sockaddr *)&senderAddr, senderLen);
            }
            else if (packetType == PACKET_GAME && received == sizeof(ClientPacket)) {
                ClientPacket inPacket;
                memcpy(&inPacket, recvBuffer, sizeof(ClientPacket));

                int r = inPacket.room_id;
                if (r < 0 || r >= MAX_ROOMS) {
                    r = FindClientRoom(rooms, senderAddr);
                    if (r == -1) {
                        for (int i = 0; i < MAX_ROOMS; i++) {
                            uint8_t c = CountRoomPlayers(&rooms[i]);
                            if (c > 0 && c < MAX_CLIENTS) { r = i; break; }
                        }
                        if (r == -1) {
                            for (int i = 0; i < MAX_ROOMS; i++) {
                                if (CountRoomPlayers(&rooms[i]) < MAX_CLIENTS) { r = i; break; }
                            }
                        }
                    }
                }

                if (r < 0 || r >= MAX_ROOMS) continue;

                RemoveClientFromOtherRooms(rooms, r, senderAddr);
                GameRoom *room = &rooms[r];
                int slot = RegisterOrUpdateClientInRoom(room, r, senderAddr, now);

                if (slot != -1) {
                    uint8_t team = (slot < 5) ? 0 : 1;
                    bool is_attacker = (room->current_round == 1) ? (team == 1) : (team == 0);

                    room->clients[slot].state = inPacket.player;
                    room->clients[slot].state.team = team;
                    memcpy(room->clients[slot].projectiles, inPacket.projectiles, sizeof(inPacket.projectiles));

                    if (room->current_round <= 2) {
                        for (int target = 0; target < MAX_CLIENTS; target++) {
                            uint8_t targetTeam = (target < 5) ? 0 : 1;
                            if (inPacket.damage_dealt[target] > 0 && room->clients[target].connected && targetTeam != team) {
                                room->clients[target].pending_damage += inPacket.damage_dealt[target];
                            }
                        }

                        if (is_attacker) {
                            for (int b = 0; b < room->map.building_count; b++) {
                                int16_t dmg = inPacket.building_damage_dealt[b];
                                if (dmg > 0 && !room->map.buildings[b].is_destroyed) {
                                    if (dmg > room->map.buildings[b].hp) {
                                        dmg = (int16_t)room->map.buildings[b].hp;
                                    }
                                    room->map.buildings[b].hp -= dmg;
                                    room->team_city_damage[team] += (uint32_t)dmg;

                                    if (room->map.buildings[b].hp <= 0) {
                                        room->map.buildings[b].hp = 0;
                                        room->map.buildings[b].is_destroyed = true;
                                        room->team_buildings_destroyed[team]++;
                                    }
                                }
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
        }

        // --- 2. MISE À JOUR DES MANCHES ET DIFFUSION ---
        for (int r = 0; r < MAX_ROOMS; r++) {
            GameRoom *room = &rooms[r];
            uint8_t activePlayers = 0;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (room->clients[i].connected) {
                    if ((now - room->clients[i].last_seen) > 10.0) {
                        printf("[-] Room #%d : Joueur #%d deconnecte (timeout)\n", r + 1, i);
                        room->clients[i].connected = false;
                    } else {
                        activePlayers++;
                    }
                }
            }

            if (activePlayers == 0) {
                if (room->had_players) {
                    printf("[*] Room #%d vide : reinitialisation.\n", r + 1);
                    ResetRoom(room, r);
                }
                continue;
            }

            room->round_timer -= tick_dt;
            int remainingBuildings = GetCityRemainingBuildings(&room->map);

            if (room->current_round == 1 && (room->round_timer <= 0.0f || remainingBuildings == 0)) {
                printf("[Room #%d] Fin Manche 1 -> Passage a la Manche 2 !\n", r + 1);
                StartRoomRound(room, 2);
            }
            else if (room->current_round == 2 && (room->round_timer <= 0.0f || remainingBuildings == 0)) {
                printf("[Room #%d] Fin Manche 2 -> Affichage des resultats !\n", r + 1);
                StartRoomRound(room, 3);
            }
            else if (room->current_round == 3 && room->round_timer <= 0.0f) {
                printf("[Room #%d] Nouveau match !\n", r + 1);
                room->team_buildings_destroyed[0] = 0;
                room->team_buildings_destroyed[1] = 0;
                room->team_city_damage[0] = 0;
                room->team_city_damage[1] = 0;
                StartRoomRound(room, 1);
            }

            UpdateDrones(room->drones, MAX_DRONE_COUNT, tick_dt);

            ServerPacket outPacket = { 0 };
            outPacket.type = PACKET_GAME;
            outPacket.room_id = (int8_t)r;
            outPacket.connected_count = activePlayers;
            outPacket.map_seed = room->map_seed;
            outPacket.current_round = room->current_round;
            outPacket.round_time_left = (room->round_timer > 0.0f) ? (uint16_t)room->round_timer : 0;
            outPacket.team_buildings_destroyed[0] = room->team_buildings_destroyed[0];
            outPacket.team_buildings_destroyed[1] = room->team_buildings_destroyed[1];
            outPacket.team_city_damage[0] = room->team_city_damage[0];
            outPacket.team_city_damage[1] = room->team_city_damage[1];

            for (int b = 0; b < MAX_BUILDINGS; b++) {
                outPacket.building_hp[b] = (int16_t)room->map.buildings[b].hp;
            }

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