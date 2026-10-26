#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>

#include "network.h"
#include "weaponSlot.h"

#define TICK_RATE 60
#define TICK_DT (1.0 / (double)TICK_RATE)

typedef struct {
    Vector3 position;
    Vector3 direction;
    float speed;
    float lifetime;
    int damage;
    uint8_t type;
    bool active;
} ServerProjectile;

typedef struct {
    struct sockaddr_in addr;
    bool connected;
    double last_seen;
    uint32_t last_seq;
    NetPlayer state;
    bool has_pos;
    Weapon weapons[MAX_WEAPON_SLOTS];
    ServerProjectile projectiles[MAX_PLAYER_PROJECTILE];
} ClientSlot;

typedef struct {
    unsigned int map_seed;
    GameMap map;
    ClientSlot clients[MAX_CLIENTS];
    Drone drones[MAX_DRONE_COUNT];
    bool had_players;
    bool rewards_distributed;
    uint32_t server_seq;

    uint8_t current_round;
    float round_timer;
    uint16_t team_buildings_destroyed[2];
    uint32_t team_city_damage[2];
} GameRoom;

static double GetCurrentTimeSec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static bool IsSameClient(struct sockaddr_in a, struct sockaddr_in b) {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
}

static bool IsValidFloat(float v) {
    return !isnan(v) && !isinf(v) && (v > -20000.0f) && (v < 20000.0f);
}

static bool Srv_CheckSpheres(Vector3 a, float ra, Vector3 b, float rb) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    float r = ra + rb;
    return (dx * dx + dy * dy + dz * dz) <= (r * r);
}

static bool Srv_CheckBoxSphere(Vector3 bmin, Vector3 bmax, Vector3 center, float radius) {
    float cx = (center.x < bmin.x) ? bmin.x : ((center.x > bmax.x) ? bmax.x : center.x);
    float cy = (center.y < bmin.y) ? bmin.y : ((center.y > bmax.y) ? bmax.y : center.y);
    float cz = (center.z < bmin.z) ? bmin.z : ((center.z > bmax.z) ? bmax.z : center.z);
    float dx = center.x - cx, dy = center.y - cy, dz = center.z - cz;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

static int16_t GetClassMaxHp(uint8_t plane_class, bool is_drone) {
    if (is_drone) return 100;
    if (plane_class == PLANE_INTERCEPTOR) return 75;
    if (plane_class == PLANE_BOMBER) return 150;
    return 100;
}

static void InitServerPlayerWeapons(ClientSlot *c) {
    if (c->state.is_drone) {
        for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
            c->weapons[i] = (Weapon){
                .type = WEAPON_DRONE_BLASTER, .name = "Drone Blaster",
                .ammo = 25, .max_ammo = 25, .reload_time = 0.25f, .reload_timer = 0.0f,
                .projectile_speed = 360.0f, .damage = 50
            };
        }
    } else if (c->state.plane_class == PLANE_INTERCEPTOR) {
        c->weapons[0] = (Weapon){ WEAPON_ROCKET, "Light Rockets", 16, 16, 1.0f, 0.0f, 360.0f, 25 };
        c->weapons[1] = (Weapon){ WEAPON_AIR_TO_AIR, "Fast A2A", 10, 10, 1.4f, 0.0f, 520.0f, 65 };
        c->weapons[2] = (Weapon){ WEAPON_AIR_TO_GROUND, "Light Bomb", 2, 2, 4.0f, 0.0f, 240.0f, 75 };
    } else if (c->state.plane_class == PLANE_BOMBER) {
        c->weapons[0] = (Weapon){ WEAPON_ROCKET, "Heavy Rockets", 12, 12, 2.0f, 0.0f, 260.0f, 50 };
        c->weapons[1] = (Weapon){ WEAPON_AIR_TO_AIR, "Defense A2A", 8, 8, 2.5f, 0.0f, 380.0f, 45 };
        c->weapons[2] = (Weapon){ WEAPON_AIR_TO_GROUND, "Mega Bomb", 8, 8, 2.5f, 0.0f, 220.0f, 160 };
    } else {
        c->weapons[0] = (Weapon){ WEAPON_ROCKET, "Rockets", 12, 12, 1.5f, 0.0f, 300.0f, 35 };
        c->weapons[1] = (Weapon){ WEAPON_AIR_TO_AIR, "Air-to-Air", 6, 6, 2.5f, 0.0f, 420.0f, 55 };
        c->weapons[2] = (Weapon){ WEAPON_AIR_TO_GROUND, "Air-to-Ground", 4, 4, 3.5f, 0.0f, 240.0f, 120 };
    }
}

static void SanitizeUsername(const char *in, char *out, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < max_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '_' || c == '-') {
            out[j++] = (char)tolower(c);
        }
    }
    out[j] = '\0';
    if (j == 0) strcpy(out, "pilote");
}

static void SaveAccountJSON(const char *clean_user, uint8_t plane_class, uint8_t skin_id, int xp, int gems) {
    mkdir("FA_Server", 0755);
    mkdir("FA_Server/accounts", 0755);
    char path[128];
    snprintf(path, sizeof(path), "FA_Server/accounts/%s.json", clean_user);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n  \"username\": \"%s\",\n  \"plane_class\": %u,\n  \"skin_id\": %u,\n  \"xp\": %d,\n  \"gems\": %d\n}\n",
            clean_user, (unsigned int)plane_class, (unsigned int)skin_id, xp, gems);
    fclose(f);
}

static void LoadOrCreateAccountJSON(const char *raw_user, char *clean_out, uint8_t *out_class, uint8_t *out_skin, int *out_xp, int *out_gems) {
    SanitizeUsername(raw_user, clean_out, MAX_USERNAME_LEN);
    mkdir("FA_Server", 0755);
    mkdir("FA_Server/accounts", 0755);
    char path[128];
    snprintf(path, sizeof(path), "FA_Server/accounts/%s.json", clean_out);
    *out_class = 0; *out_skin = 0; *out_xp = 0; *out_gems = 0;
    FILE *f = fopen(path, "r");
    if (!f) {
        SaveAccountJSON(clean_out, 0, 0, 0, 0);
        printf("[Compte JSON] Nouveau compte cree : %s\n", path);
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned int uval = 0; int val = 0;
        if (sscanf(line, " \"plane_class\": %u", &uval) == 1) { if (uval <= 2) *out_class = (uint8_t)uval; }
        else if (sscanf(line, " \"skin_id\": %u", &uval) == 1) { if (uval <= 4) *out_skin = (uint8_t)uval; }
        else if (sscanf(line, " \"xp\": %d", &val) == 1) *out_xp = val;
        else if (sscanf(line, " \"gems\": %d", &val) == 1) *out_gems = val;
    }
    fclose(f);
}

static void RewardMatchPlayersJSON(GameRoom *room) {
    if (room->rewards_distributed) return;
    room->rewards_distributed = true;

    int winner = -1;
    if (room->team_buildings_destroyed[0] > room->team_buildings_destroyed[1]) winner = 0;
    else if (room->team_buildings_destroyed[1] > room->team_buildings_destroyed[0]) winner = 1;
    else if (room->team_city_damage[0] > room->team_city_damage[1]) winner = 0;
    else if (room->team_city_damage[1] > room->team_city_damage[0]) winner = 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!room->clients[i].connected) continue;
        char cleanUser[MAX_USERNAME_LEN];
        uint8_t cls = 0, skin = 0; int xp = 0, gems = 0;
        LoadOrCreateAccountJSON(room->clients[i].state.username, cleanUser, &cls, &skin, &xp, &gems);
        uint8_t team = (i < 5) ? 0 : 1;
        int gainedXp = 50 + (int)room->team_buildings_destroyed[team] * 15 + ((winner == (int)team) ? 100 : 0);
        int gainedGems = (winner == (int)team) ? 25 : 10;
        SaveAccountJSON(cleanUser, cls, skin, xp + gainedXp, gems + gainedGems);
        printf("[JSON XP] %s gagne +%d XP et +%d Gemmes !\n", cleanUser, gainedXp, gainedGems);
    }
}

static void StartRoomRound(GameRoom *room, uint8_t round_num) {
    room->current_round = round_num;
    room->round_timer = (round_num <= 2) ? ROUND_DURATION_SEC : 12.0f;
    InitMap(&room->map, room->map_seed);
    SpawnDrones(room->drones, MAX_DRONE_COUNT);

    if (round_num <= 2) {
        room->rewards_distributed = false;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            room->clients[i].state.is_drone = false;
            room->clients[i].state.is_alive = true;
            room->clients[i].state.respawns_left = 3;
            room->clients[i].state.hp = GetClassMaxHp(room->clients[i].state.plane_class, false);
            room->clients[i].has_pos = false;
            memset(room->clients[i].projectiles, 0, sizeof(room->clients[i].projectiles));
            InitServerPlayerWeapons(&room->clients[i]);
        }
    } else if (round_num == 3) {
        RewardMatchPlayersJSON(room);
    }
}

static void ResetRoom(GameRoom *room, int room_index) {
    // Chaque room a une ville par défaut (Room 1=Paris, 2=New York, 3=Tokyo, 4=Dubaï) modifiable par le créateur
    room->map_seed = 133740u + (unsigned int)(room_index % 4);
    memset(room->clients, 0, sizeof(room->clients));
    room->had_players = false;
    room->server_seq = 0;
    room->team_buildings_destroyed[0] = 0;
    room->team_buildings_destroyed[1] = 0;
    room->team_city_damage[0] = 0;
    room->team_city_damage[1] = 0;
    StartRoomRound(room, 1);
}

static uint8_t CountRoomPlayers(const GameRoom *room) {
    uint8_t count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) if (room->clients[i].connected) count++;
    return count;
}

static int FindClientRoom(GameRoom rooms[], struct sockaddr_in addr) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (rooms[r].clients[i].connected && IsSameClient(rooms[r].clients[i].addr, addr)) return r;
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
            ClientSlot *c = &room->clients[i];
            memset(c, 0, sizeof(ClientSlot));
            c->connected = true;
            c->addr = addr;
            c->last_seen = now;
            c->state.position = (Vector3){ 0.0f, 90.0f, 250.0f };
            c->state.is_alive = true;
            c->state.respawns_left = 3;
            c->state.team = (i < 5) ? 0 : 1;
            c->state.hp = 100;
            strcpy(c->state.username, "pilote");
            InitServerPlayerWeapons(c);
            room->had_players = true;
            printf("[+] Room #%d (%s) : Joueur #%d rejoint (%s:%d)\n",
                   room_id + 1, GetCityName(room->map.city_type), i, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            return i;
        }
    }
    return -1;
}

int main(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("Erreur socket"); return 1; }
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Erreur bind"); close(sock); return 1;
    }

    GameRoom rooms[MAX_ROOMS];
    for (int r = 0; r < MAX_ROOMS; r++) ResetRoom(&rooms[r], r);

    printf("=== Serveur AUTORITAIRE (%d Rooms 5v5 - Villes & Skins) demarre sur UDP %d ===\n", MAX_ROOMS, SERVER_PORT);
    printf("Tailles -> LobbyClient: %zu o | GameClient: %zu o | GameServer Base: %zu o\n",
           sizeof(LobbyClientPacket), sizeof(ClientPacket), SERVER_PACKET_BASE_SIZE);

    double previousTime = GetCurrentTimeSec();
    double accumulator = 0.0;

    while (1) {
        double now = GetCurrentTimeSec();
        double frameTime = now - previousTime;
        previousTime = now;
        if (frameTime > 0.25) frameTime = 0.25;
        accumulator += frameTime;

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

                char cleanUser[MAX_USERNAME_LEN];
                uint8_t savedClass = 0, savedSkin = 0; int xp = 0, gems = 0;
                LoadOrCreateAccountJSON(lobbyIn.username, cleanUser, &savedClass, &savedSkin, &xp, &gems);

                LobbyServerPacket lobbyOut = {
                    .type = PACKET_LOBBY,
                    .room_id = lobbyIn.room_id,
                    .connected_count = 0,
                    .plane_class = savedClass,
                    .skin_id = savedSkin,
                    .city_type = lobbyIn.city_type % 4,
                    .account_loaded = true,
                    .xp = xp,
                    .gems = gems
                };
                strncpy(lobbyOut.username, cleanUser, MAX_USERNAME_LEN - 1);

                if (lobbyIn.action == LOBBY_SET_CLASS) {
                    uint8_t newClass = (lobbyIn.plane_class <= 2) ? lobbyIn.plane_class : 0;
                    uint8_t newSkin  = (lobbyIn.skin_id <= 4) ? lobbyIn.skin_id : 0;
                    SaveAccountJSON(cleanUser, newClass, newSkin, xp, gems);
                    lobbyOut.plane_class = newClass;
                    lobbyOut.skin_id = newSkin;
                }
                else if (lobbyIn.action == LOBBY_LEAVE) {
                    RemoveClientFromOtherRooms(rooms, -1, senderAddr);
                    lobbyOut.room_id = -1;
                }
                else if (lobbyIn.action == LOBBY_CREATE) {
                    RemoveClientFromOtherRooms(rooms, -1, senderAddr);
                    int chosen = -1;
                    for (int r = 0; r < MAX_ROOMS; r++) if (CountRoomPlayers(&rooms[r]) == 0) { chosen = r; break; }
                    if (chosen == -1) for (int r = 0; r < MAX_ROOMS; r++) if (CountRoomPlayers(&rooms[r]) < MAX_CLIENTS) { chosen = r; break; }
                    if (chosen != -1 && RegisterOrUpdateClientInRoom(&rooms[chosen], chosen, senderAddr, now) != -1) {
                        rooms[chosen].map_seed = 133740u + (unsigned int)(lobbyIn.city_type % 4);
                        StartRoomRound(&rooms[chosen], 1);
                        lobbyOut.room_id = (int8_t)chosen;
                        lobbyOut.city_type = rooms[chosen].map.city_type;
                        lobbyOut.connected_count = CountRoomPlayers(&rooms[chosen]);
                    }
                }
                else if (lobbyIn.action == LOBBY_JOIN || lobbyIn.action == LOBBY_HEARTBEAT) {
                    int r = lobbyIn.room_id;
                    if (r >= 0 && r < MAX_ROOMS) {
                        RemoveClientFromOtherRooms(rooms, r, senderAddr);
                        if (RegisterOrUpdateClientInRoom(&rooms[r], r, senderAddr, now) != -1) {
                            lobbyOut.room_id = (int8_t)r;
                            lobbyOut.city_type = rooms[r].map.city_type;
                            lobbyOut.connected_count = CountRoomPlayers(&rooms[r]);
                        }
                    }
                }

                if (lobbyOut.room_id >= 0 && lobbyOut.room_id < MAX_ROOMS) {
                    lobbyOut.connected_count = CountRoomPlayers(&rooms[lobbyOut.room_id]);
                    lobbyOut.city_type = rooms[lobbyOut.room_id].map.city_type;
                }
                sendto(sock, &lobbyOut, sizeof(lobbyOut), 0, (struct sockaddr *)&senderAddr, senderLen);
            }
            else if (packetType == PACKET_GAME && received == sizeof(ClientPacket)) {
                ClientPacket inPacket;
                memcpy(&inPacket, recvBuffer, sizeof(ClientPacket));

                if (!IsValidFloat(inPacket.position.x) || !IsValidFloat(inPacket.position.y) ||
                    !IsValidFloat(inPacket.position.z) || !IsValidFloat(inPacket.pitch) ||
                    !IsValidFloat(inPacket.yaw) || !IsValidFloat(inPacket.roll)) {
                    continue;
                }

                int r = FindClientRoom(rooms, senderAddr);
                if (r == -1) {
                    if (inPacket.room_id >= 0 && inPacket.room_id < MAX_ROOMS &&
                        CountRoomPlayers(&rooms[inPacket.room_id]) < MAX_CLIENTS) {
                        r = inPacket.room_id;
                    } else {
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
                    if (r >= 0 && r < MAX_ROOMS && CountRoomPlayers(&rooms[r]) == 0) {
                        rooms[r].map_seed = 133740u + (unsigned int)(inPacket.city_type % 4);
                        StartRoomRound(&rooms[r], 1);
                    }
                }
                if (r < 0 || r >= MAX_ROOMS) continue;

                GameRoom *room = &rooms[r];
                int slot = RegisterOrUpdateClientInRoom(room, r, senderAddr, now);
                if (slot == -1) continue;

                ClientSlot *client = &room->clients[slot];
                if (inPacket.seq <= client->last_seq && (client->last_seq - inPacket.seq) < 100000u) continue;
                client->last_seq = inPacket.seq;

                uint8_t team = (slot < 5) ? 0 : 1;
                bool is_attacker = (room->current_round == 1) ? (team == 1) : (team == 0);
                client->state.team = team;
                client->state.skin_id = (inPacket.skin_id <= 4) ? inPacket.skin_id : 0;
                SanitizeUsername(inPacket.username, client->state.username, sizeof(client->state.username));

                uint8_t reqClass = (inPacket.plane_class <= 2) ? inPacket.plane_class : 0;
                if (!client->has_pos || client->state.plane_class != reqClass) {
                    client->state.plane_class = reqClass;
                    if (!client->has_pos) {
                        client->state.hp = GetClassMaxHp(reqClass, false);
                        client->state.is_alive = true;
                        client->has_pos = true;
                    }
                    InitServerPlayerWeapons(client);
                }

                if (!client->state.is_alive && room->current_round <= 2) {
                    if (inPacket.respawn_cmd == RESPAWN_PLANE && client->state.respawns_left > 0) {
                        client->state.respawns_left--;
                        client->state.plane_class = reqClass;
                        client->state.is_drone = false;
                        client->state.is_alive = true;
                        client->state.hp = GetClassMaxHp(reqClass, false);
                        client->state.position = is_attacker ? (Vector3){ 0.0f, 95.0f, 450.0f } : (Vector3){ 0.0f, 80.0f, -250.0f };
                        InitServerPlayerWeapons(client);
                    }
                    else if (inPacket.respawn_cmd == RESPAWN_DRONE && client->state.respawns_left == 0) {
                        int startIdx = is_attacker ? DEFENDER_DRONE_COUNT : 0;
                        int endIdx   = is_attacker ? MAX_DRONE_COUNT : DEFENDER_DRONE_COUNT;
                        for (int d = startIdx; d < endIdx; d++) {
                            if (room->drones[d].is_alive) {
                                room->drones[d].is_alive = false;
                                room->drones[d].bullet_active = false;
                                client->state.is_drone = true;
                                client->state.is_alive = true;
                                client->state.hp = 100;
                                client->state.position = room->drones[d].pos;
                                InitServerPlayerWeapons(client);
                                break;
                            }
                        }
                    }
                }

                if (client->state.is_alive && room->current_round <= 2) {
                    client->state.position = inPacket.position;
                    client->state.pitch    = inPacket.pitch;
                    client->state.yaw      = inPacket.yaw;
                    client->state.roll     = inPacket.roll;

                    if (inPacket.fire_slot >= 1 && inPacket.fire_slot <= MAX_WEAPON_SLOTS) {
                        Weapon *w = &client->weapons[inPacket.fire_slot - 1];
                        if (w->ammo > 0) {
                            int chosenSlot = -1;
                            for (int p = 0; p < MAX_PLAYER_PROJECTILE; p++) {
                                if (!client->projectiles[p].active) {
                                    chosenSlot = p;
                                    break;
                                }
                            }
                            if (chosenSlot == -1) {
                                chosenSlot = 0;
                                float minLife = client->projectiles[0].lifetime;
                                for (int p = 1; p < MAX_PLAYER_PROJECTILE; p++) {
                                    if (client->projectiles[p].lifetime < minLife) {
                                        minLife = client->projectiles[p].lifetime;
                                        chosenSlot = p;
                                    }
                                }
                            }

                            w->ammo--;
                            client->projectiles[chosenSlot] = (ServerProjectile){
                                .position = client->state.position,
                                .direction = (Vector3){
                                    -cosf(client->state.pitch) * sinf(client->state.yaw),
                                     sinf(client->state.pitch),
                                    -cosf(client->state.pitch) * cosf(client->state.yaw)
                                },
                                .speed = w->projectile_speed,
                                .lifetime = 3.0f,
                                .damage = w->damage,
                                .type = (uint8_t)w->type,
                                .active = true
                            };
                        }
                    }
                }
            }
        }

        // --- 2. BOUCLE DE SIMULATION AUTORITAIRE À 60 Hz ---
        while (accumulator >= TICK_DT) {
            float dt = (float)TICK_DT;

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

                room->round_timer -= dt;
                int remainingBuildings = GetCityRemainingBuildings(&room->map);

                if (room->current_round == 1 && (room->round_timer <= 0.0f || remainingBuildings == 0)) {
                    StartRoomRound(room, 2);
                } else if (room->current_round == 2 && (room->round_timer <= 0.0f || remainingBuildings == 0)) {
                    StartRoomRound(room, 3);
                } else if (room->current_round == 3 && room->round_timer <= 0.0f) {
                    room->team_buildings_destroyed[0] = 0;
                    room->team_buildings_destroyed[1] = 0;
                    room->team_city_damage[0] = 0;
                    room->team_city_damage[1] = 0;
                    StartRoomRound(room, 1);
                }

                if (room->current_round <= 2) {
                    uint8_t attackerTeam = (room->current_round == 1) ? 1 : 0;
                    UpdateDrones(
                        room->drones, MAX_DRONE_COUNT, &room->map,
                        &room->team_buildings_destroyed[attackerTeam],
                        &room->team_city_damage[attackerTeam], dt
                    );

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        ClientSlot *c = &room->clients[i];
                        if (!c->connected || !c->has_pos) continue;

                        uint8_t team = (i < 5) ? 0 : 1;
                        bool is_attacker = (room->current_round == 1) ? (team == 1) : (team == 0);

                        if (c->state.is_alive) {
                            UpdateWeaponsReload(c->weapons, dt);

                            if (c->state.position.y <= 0.5f) {
                                c->state.hp = 0;
                                c->state.is_alive = false;
                            } else {
                                float selfRadius = c->state.is_drone ? 3.0f : 1.5f;
                                for (int b = 0; b < room->map.building_count; b++) {
                                    Building *bld = &room->map.buildings[b];
                                    if (bld->is_destroyed) continue;
                                    if (Srv_CheckBoxSphere(bld->min, bld->max, c->state.position, selfRadius)) {
                                        c->state.hp = 0;
                                        c->state.is_alive = false;
                                        if (is_attacker) {
                                            int dmg = (100 > bld->hp) ? bld->hp : 100;
                                            bld->hp -= dmg;
                                            room->team_city_damage[team] += (uint32_t)dmg;
                                            if (bld->hp <= 0) {
                                                bld->hp = 0;
                                                bld->is_destroyed = true;
                                                room->team_buildings_destroyed[team]++;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }

                        for (int p = 0; p < MAX_PLAYER_PROJECTILE; p++) {
                            ServerProjectile *proj = &c->projectiles[p];
                            if (!proj->active) continue;

                            proj->lifetime -= dt;
                            proj->position.x += proj->direction.x * proj->speed * dt;
                            proj->position.y += proj->direction.y * proj->speed * dt;
                            proj->position.z += proj->direction.z * proj->speed * dt;

                            if (proj->lifetime <= 0.0f || proj->position.y <= 0.0f) {
                                proj->active = false;
                                continue;
                            }

                            bool hit = false;
                            for (int b = 0; b < room->map.building_count; b++) {
                                Building *bld = &room->map.buildings[b];
                                if (bld->is_destroyed) continue;
                                if (Srv_CheckBoxSphere(bld->min, bld->max, proj->position, 0.8f)) {
                                    proj->active = false;
                                    hit = true;
                                    if (is_attacker) {
                                        int dmg = proj->damage;
                                        if (proj->type == WEAPON_DRONE_BLASTER) {
                                            dmg = (bld->max_hp + 14) / 15;
                                        }
                                        if (dmg > bld->hp) dmg = bld->hp;
                                        bld->hp -= dmg;
                                        room->team_city_damage[team] += (uint32_t)dmg;
                                        if (bld->hp <= 0) {
                                            bld->hp = 0;
                                            bld->is_destroyed = true;
                                            room->team_buildings_destroyed[team]++;
                                        }
                                    }
                                    break;
                                }
                            }
                            if (hit) continue;

                            for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                                if (!room->drones[d].is_alive) continue;
                                if (room->drones[d].is_attacker == is_attacker) continue;
                                if (Srv_CheckSpheres(proj->position, 1.0f, room->drones[d].pos, 6.0f)) {
                                    proj->active = false;
                                    hit = true;
                                    room->drones[d].hp -= proj->damage;
                                    if (room->drones[d].hp <= 0) {
                                        room->drones[d].is_alive = false;
                                        room->drones[d].bullet_active = false;
                                    }
                                    break;
                                }
                            }
                            if (hit) continue;

                            for (int target = 0; target < MAX_CLIENTS; target++) {
                                if (target == i || !room->clients[target].connected || !room->clients[target].state.is_alive) continue;
                                uint8_t targetTeam = (target < 5) ? 0 : 1;
                                if (targetTeam == team) continue;

                                float rad = room->clients[target].state.is_drone ? 6.0f : 3.5f;
                                if (Srv_CheckSpheres(proj->position, 1.0f, room->clients[target].state.position, rad)) {
                                    proj->active = false;
                                    room->clients[target].state.hp -= (int16_t)proj->damage;
                                    if (room->clients[target].state.hp <= 0) {
                                        room->clients[target].state.hp = 0;
                                        room->clients[target].state.is_alive = false;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }

                room->server_seq++;
                ServerPacket outPacket = { 0 };
                outPacket.type = PACKET_GAME;
                outPacket.room_id = (int8_t)r;
                outPacket.connected_count = activePlayers;
                outPacket.seq = room->server_seq;
                outPacket.map_seed = room->map_seed;
                outPacket.current_round = room->current_round;
                outPacket.round_time_left = (room->round_timer > 0.0f) ? (uint16_t)room->round_timer : 0;
                outPacket.team_buildings_destroyed[0] = room->team_buildings_destroyed[0];
                outPacket.team_buildings_destroyed[1] = room->team_buildings_destroyed[1];
                outPacket.team_city_damage[0] = room->team_city_damage[0];
                outPacket.team_city_damage[1] = room->team_city_damage[1];

                for (int b = 0; b < MAX_BUILDINGS; b++) {
                    int hp = room->map.buildings[b].hp;
                    outPacket.building_hp[b] = (uint8_t)((hp < 0) ? 0 : ((hp > 255) ? 255 : hp));
                }

                for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                    outPacket.drones[d].x  = QuantizeCoord(room->drones[d].pos.x);
                    outPacket.drones[d].y  = QuantizeCoord(room->drones[d].pos.y);
                    outPacket.drones[d].z  = QuantizeCoord(room->drones[d].pos.z);
                    outPacket.drones[d].bx = QuantizeCoord(room->drones[d].bullet_pos.x);
                    outPacket.drones[d].by = QuantizeCoord(room->drones[d].bullet_pos.y);
                    outPacket.drones[d].bz = QuantizeCoord(room->drones[d].bullet_pos.z);
                    outPacket.drones[d].flags = (room->drones[d].is_alive ? 0x01 : 0x00) |
                                                (room->drones[d].bullet_active ? 0x02 : 0x00);
                }

                uint8_t activeProjCount = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    outPacket.connected[i] = room->clients[i].connected;
                    outPacket.players[i] = room->clients[i].state;

                    if (!room->clients[i].connected) continue;
                    for (int p = 0; p < MAX_PLAYER_PROJECTILE; p++) {
                        ServerProjectile *sp = &room->clients[i].projectiles[p];
                        if (sp->active && activeProjCount < MAX_PROJECTILES) {
                            outPacket.projectiles[activeProjCount++] = (NetProjectile){
                                .x = QuantizeCoord(sp->position.x),
                                .y = QuantizeCoord(sp->position.y),
                                .z = QuantizeCoord(sp->position.z),
                                .type_and_active = (uint8_t)((sp->type & 0x7F) | 0x80)
                            };
                        }
                    }
                }
                outPacket.active_projectile_count = activeProjCount;

                size_t packetWireSize = SERVER_PACKET_BASE_SIZE + (size_t)activeProjCount * sizeof(NetProjectile);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (!room->clients[i].connected) continue;
                    outPacket.your_id = (int8_t)i;
                    sendto(sock, &outPacket, packetWireSize, 0, (struct sockaddr *)&room->clients[i].addr, sizeof(room->clients[i].addr));
                }
            }

            accumulator -= TICK_DT;
        }

        double elapsedInLoop = GetCurrentTimeSec() - now;
        double remainingForTick = (TICK_DT - accumulator) - elapsedInLoop;
        if (remainingForTick > 0.0005) {
            struct timeval sleepTime = { 0, (suseconds_t)(remainingForTick * 1000000.0) };
            select(0, NULL, NULL, NULL, &sleepTime);
        }
    }

    close(sock);
    return 0;
}