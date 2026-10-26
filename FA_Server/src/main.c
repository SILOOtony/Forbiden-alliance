#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#include "server.h"

static double GetCurrentTimeSec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static uint32_t GenerateSessionToken(void) {
    static uint32_t counter = 1000;
    counter++;
    uint32_t r = (uint32_t)rand() ^ (counter * 2654435761u);
    return (r == 0) ? 1u : r;
}

static bool IsValidFloat(float v) {
    return !isnan(v) && !isinf(v) && (v > -20000.0f) && (v < 20000.0f);
}

static uint8_t CountSquadMembers(const LobbyRoom *sq) {
    uint8_t c = 0;
    for (int i = 0; i < MAX_SQUAD_SIZE; i++) if (sq->members[i].connected) c++;
    return c;
}

static void RemoveFromAllSquads(LobbyRoom squads[], int except_sq, struct sockaddr_in addr) {
    for (int s = 0; s < MAX_LOBBY_ROOMS; s++) {
        if (s == except_sq) continue;
        for (int i = 0; i < MAX_SQUAD_SIZE; i++) {
            if (squads[s].members[i].connected && IsSameClient(squads[s].members[i].addr, addr)) {
                squads[s].members[i].connected = false;
                squads[s].in_matchmaking = false;
            }
        }
    }
}

static int FindClientSquad(LobbyRoom squads[], struct sockaddr_in addr, int *out_slot) {
    for (int s = 0; s < MAX_LOBBY_ROOMS; s++) {
        for (int i = 0; i < MAX_SQUAD_SIZE; i++) {
            if (squads[s].members[i].connected && IsSameClient(squads[s].members[i].addr, addr)) {
                if (out_slot) *out_slot = i;
                return s;
            }
        }
    }
    return -1;
}

static int JoinOrUpdateSquad(LobbyRoom *sq, struct sockaddr_in addr, double now) {
    for (int i = 0; i < MAX_SQUAD_SIZE; i++) {
        if (sq->members[i].connected && IsSameClient(sq->members[i].addr, addr)) {
            sq->members[i].last_seen = now;
            return i;
        }
    }
    for (int i = 0; i < MAX_SQUAD_SIZE; i++) {
        if (!sq->members[i].connected) {
            memset(&sq->members[i], 0, sizeof(SquadMemberSlot));
            sq->members[i].connected = true;
            sq->members[i].addr = addr;
            sq->members[i].last_seen = now;
            sq->members[i].assigned_game_room = -1;
            sq->in_matchmaking = false;
            return i;
        }
    }
    return -1;
}

static void AssignSquadToGameRoom(LobbyRoom *sq, GameRoom *gr, int game_room_id, int preferred_team, double now) {
    for (int m = 0; m < MAX_SQUAD_SIZE; m++) {
        if (!sq->members[m].connected) continue;

        int chosenSlot = -1;
        int startSlot = (preferred_team == 0) ? 0 : 5;
        int endSlot   = (preferred_team == 0) ? 5 : 10;

        for (int i = startSlot; i < endSlot; i++) {
            if (!gr->clients[i].connected) { chosenSlot = i; break; }
        }
        if (chosenSlot == -1) {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!gr->clients[i].connected) { chosenSlot = i; break; }
            }
        }

        if (chosenSlot != -1) {
            ClientSlot *c = &gr->clients[chosenSlot];
            memset(c, 0, sizeof(ClientSlot));
            c->connected = true;
            c->addr = sq->members[m].addr;
            c->last_seen = now;
            c->last_move_time = now;
            c->session_token = GenerateSessionToken();
            c->state.is_alive = true;
            c->state.respawns_left = 3;
            c->state.team = (chosenSlot < 5) ? 0 : 1;
            c->state.plane_class = sq->members[m].plane_class;
            c->state.skin_id = sq->members[m].skin_id;
            c->state.elo = (int16_t)sq->members[m].elo;
            c->state.hp = GetClassMaxHp(c->state.plane_class, false);
            strncpy(c->state.username, sq->members[m].username, sizeof(c->state.username) - 1);
            InitServerPlayerWeapons(c);
            gr->had_players = true;

            sq->members[m].assigned_game_room = (int8_t)game_room_id;
            sq->members[m].session_token = c->session_token;
            sq->members[m].is_ready = false;
        }
    }
    sq->in_matchmaking = false;
}

int main(void) {
    srand((unsigned int)time(NULL));
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

    LobbyRoom lobbyRooms[MAX_LOBBY_ROOMS];
    memset(lobbyRooms, 0, sizeof(lobbyRooms));

    GameRoom gameRooms[MAX_GAME_ROOMS];
    for (int r = 0; r < MAX_GAME_ROOMS; r++) ResetGameRoom(&gameRooms[r], r);

    printf("=== Serveur ETAPE 10 Final (%d Escouades + %d GameRooms Modulaires) sur UDP %d ===\n",
           MAX_LOBBY_ROOMS, MAX_GAME_ROOMS, SERVER_PORT);

    double previousTime = GetCurrentTimeSec();
    double accumulator = 0.0;

    while (1) {
        double now = GetCurrentTimeSec();
        double frameTime = now - previousTime;
        previousTime = now;
        if (frameTime > 0.25) frameTime = 0.25;
        accumulator += frameTime;

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
                lobbyIn.password[MAX_PASSWORD_LEN - 1] = '\0';

                char cleanUser[MAX_USERNAME_LEN];
                char cleanPass[MAX_PASSWORD_LEN];
                uint8_t savedClass = lobbyIn.plane_class;
                uint8_t savedSkin  = lobbyIn.skin_id;
                uint8_t unlockedSkins = 1;
                int xp = 0, gems = 0, elo = 1000;

                bool allowCreate = (lobbyIn.action == LOBBY_LOGIN);
                bool isExplicitAccountAction = (lobbyIn.action == LOBBY_LOGIN || lobbyIn.action == LOBBY_SET_LOADOUT);
                bool authOk = AuthenticateAccountJSON(
                    lobbyIn.username, lobbyIn.password, allowCreate,
                    cleanUser, cleanPass,
                    &savedClass, &savedSkin, &unlockedSkins, &xp, &gems, &elo
                );

                int level = 1 + (xp / 200);

                if (authOk && lobbyIn.action == LOBBY_SET_LOADOUT) {
                    const int reqLevels[3] = { 1, 2, 4 };
                    if (lobbyIn.plane_class <= 2 && level >= reqLevels[lobbyIn.plane_class]) {
                        savedClass = lobbyIn.plane_class;
                    }

                    uint8_t reqSkin = (lobbyIn.skin_id <= 4) ? lobbyIn.skin_id : 0;
                    const int skinCosts[5] = { 0, 50, 100, 150, 200 };
                    if ((unlockedSkins & (1u << reqSkin)) != 0) {
                        savedSkin = reqSkin;
                    } else if (gems >= skinCosts[reqSkin]) {
                        gems -= skinCosts[reqSkin];
                        unlockedSkins |= (uint8_t)(1u << reqSkin);
                        savedSkin = reqSkin;
                        printf("[Boutique] %s achete le Skin #%u pour %d Gemmes !\n", cleanUser, reqSkin, skinCosts[reqSkin]);
                    }
                    SaveAccountJSON(cleanUser, cleanPass, savedClass, savedSkin, unlockedSkins, xp, gems, elo);
                }

                int memberSlot = -1;
                int sqId = FindClientSquad(lobbyRooms, senderAddr, &memberSlot);

                if (lobbyIn.action == LOBBY_LEAVE_SQUAD) {
                    RemoveFromAllSquads(lobbyRooms, -1, senderAddr);
                    for (int r = 0; r < MAX_GAME_ROOMS; r++) {
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (gameRooms[r].clients[i].connected && IsSameClient(gameRooms[r].clients[i].addr, senderAddr)) {
                                gameRooms[r].clients[i].connected = false;
                            }
                        }
                    }
                    sqId = -1;
                }
                else if (lobbyIn.action == LOBBY_CREATE_SQUAD) {
                    RemoveFromAllSquads(lobbyRooms, -1, senderAddr);
                    for (int s = 0; s < MAX_LOBBY_ROOMS; s++) {
                        if (CountSquadMembers(&lobbyRooms[s]) == 0) {
                            sqId = s;
                            memberSlot = JoinOrUpdateSquad(&lobbyRooms[s], senderAddr, now);
                            lobbyRooms[s].preferred_city = lobbyIn.city_type % 4;
                            break;
                        }
                    }
                }
                else if (lobbyIn.action == LOBBY_JOIN_SQUAD) {
                    int targetSq = lobbyIn.squad_id;
                    if (targetSq >= 0 && targetSq < MAX_LOBBY_ROOMS && CountSquadMembers(&lobbyRooms[targetSq]) < MAX_SQUAD_SIZE) {
                        RemoveFromAllSquads(lobbyRooms, targetSq, senderAddr);
                        memberSlot = JoinOrUpdateSquad(&lobbyRooms[targetSq], senderAddr, now);
                        sqId = targetSq;
                    }
                }
                else if (lobbyIn.action == LOBBY_SET_CITY && sqId != -1) {
                    lobbyRooms[sqId].preferred_city = lobbyIn.city_type % 4;
                }
                else if (lobbyIn.action == LOBBY_TOGGLE_READY) {
                    if (sqId == -1) {
                        for (int s = 0; s < MAX_LOBBY_ROOMS; s++) {
                            if (CountSquadMembers(&lobbyRooms[s]) == 0) {
                                sqId = s;
                                memberSlot = JoinOrUpdateSquad(&lobbyRooms[s], senderAddr, now);
                                lobbyRooms[s].preferred_city = lobbyIn.city_type % 4;
                                break;
                            }
                        }
                    }
                    if (sqId != -1 && memberSlot != -1) {
                        lobbyRooms[sqId].members[memberSlot].is_ready = !lobbyRooms[sqId].members[memberSlot].is_ready;
                        lobbyRooms[sqId].members[memberSlot].assigned_game_room = -1;
                        lobbyRooms[sqId].preferred_city = lobbyIn.city_type % 4;
                    }
                }

                if (sqId >= 0 && sqId < MAX_LOBBY_ROOMS && memberSlot >= 0 && memberSlot < MAX_SQUAD_SIZE) {
                    SquadMemberSlot *ms = &lobbyRooms[sqId].members[memberSlot];
                    ms->last_seen = now;
                    strncpy(ms->username, (cleanUser[0] != '\0') ? cleanUser : "pilote", MAX_USERNAME_LEN - 1);
                    ms->plane_class = savedClass;
                    ms->skin_id = savedSkin;
                    ms->unlocked_skins = unlockedSkins;
                    ms->xp = xp;
                    ms->gems = gems;
                    ms->elo = elo;

                    bool allReady = true;
                    uint8_t count = 0;
                    for (int i = 0; i < MAX_SQUAD_SIZE; i++) {
                        if (lobbyRooms[sqId].members[i].connected) {
                            count++;
                            if (!lobbyRooms[sqId].members[i].is_ready) allReady = false;
                        }
                    }
                    if (count > 0 && allReady) {
                        if (!lobbyRooms[sqId].in_matchmaking) {
                            lobbyRooms[sqId].in_matchmaking = true;
                            lobbyRooms[sqId].matchmaking_start = now;
                            printf("[Matchmaking] Escouade #%d (%u joueurs) entre dans la file !\n", sqId + 1, count);
                        }
                    } else {
                        lobbyRooms[sqId].in_matchmaking = false;
                    }
                }

                LobbyServerPacket lobbyOut = {
                    .type = PACKET_LOBBY,
                    .squad_id = (int8_t)sqId,
                    .squad_count = 0,
                    .plane_class = savedClass,
                    .skin_id = savedSkin,
                    .unlocked_skins = unlockedSkins,
                    .city_type = (sqId != -1) ? lobbyRooms[sqId].preferred_city : (lobbyIn.city_type % 4),
                    .account_loaded = (isExplicitAccountAction && authOk),
                    .auth_failed = (isExplicitAccountAction && !authOk),
                    .is_ready = (sqId != -1 && memberSlot != -1) ? lobbyRooms[sqId].members[memberSlot].is_ready : false,
                    .in_matchmaking = (sqId != -1) ? lobbyRooms[sqId].in_matchmaking : false,
                    .matchmaking_secs = (sqId != -1 && lobbyRooms[sqId].in_matchmaking) ? (uint8_t)(now - lobbyRooms[sqId].matchmaking_start) : 0,
                    .assigned_game_room = (sqId != -1 && memberSlot != -1) ? lobbyRooms[sqId].members[memberSlot].assigned_game_room : -1,
                    .session_token = (sqId != -1 && memberSlot != -1) ? lobbyRooms[sqId].members[memberSlot].session_token : 0,
                    .xp = xp,
                    .gems = gems,
                    .elo = elo
                };
                strncpy(lobbyOut.username, cleanUser, MAX_USERNAME_LEN - 1);

                if (sqId != -1) {
                    uint8_t idx = 0;
                    for (int i = 0; i < MAX_SQUAD_SIZE; i++) {
                        if (lobbyRooms[sqId].members[i].connected) {
                            SquadMemberSlot *m = &lobbyRooms[sqId].members[i];
                            strncpy(lobbyOut.squad_members[idx].username, m->username, 15);
                            lobbyOut.squad_members[idx].plane_class = m->plane_class;
                            lobbyOut.squad_members[idx].skin_id = m->skin_id;
                            lobbyOut.squad_members[idx].is_ready = m->is_ready;
                            lobbyOut.squad_members[idx].elo = (int16_t)m->elo;
                            lobbyOut.squad_members[idx].level = (uint8_t)(1 + m->xp / 200);
                            idx++;
                        }
                    }
                    lobbyOut.squad_count = idx;
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

                int r = inPacket.room_id;
                if (r < 0 || r >= MAX_GAME_ROOMS || inPacket.session_token == 0) continue;

                GameRoom *room = &gameRooms[r];
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (room->clients[i].connected &&
                        IsSameClient(room->clients[i].addr, senderAddr) &&
                        room->clients[i].session_token == inPacket.session_token) {
                        slot = i;
                        break;
                    }
                }
                if (slot == -1) continue;

                ClientSlot *client = &room->clients[slot];
                client->last_seen = now;

                if (inPacket.seq <= client->last_seq && (client->last_seq - inPacket.seq) < 100000u) continue;
                client->last_seq = inPacket.seq;

                uint8_t team = (slot < 5) ? 0 : 1;
                bool is_attacker = (room->current_round == 1) ? (team == 1) : (team == 0);
                client->state.team = team;
                SanitizeUsername(inPacket.username, client->state.username, sizeof(client->state.username));
                if (client->state.username[0] == '\0') strcpy(client->state.username, "pilote");

                // Spawns placés hors de la grille d'immeubles (z = ±520, y = 110)
                Vector3 spawnPos = is_attacker ? (Vector3){ 0.0f, 110.0f, 520.0f } : (Vector3){ 0.0f, 110.0f, -520.0f };

                if (!client->has_pos) {
                    client->state.hp = GetClassMaxHp(client->state.plane_class, false);
                    client->state.is_alive = true;
                    client->state.position = spawnPos;
                    client->last_move_time = now;
                    client->has_pos = true;
                    InitServerPlayerWeapons(client);
                }

                if (!client->state.is_alive && room->current_round <= 2) {
                    if (inPacket.respawn_cmd == RESPAWN_PLANE && client->state.respawns_left > 0) {
                        client->state.respawns_left--;
                        client->state.is_drone = false;
                        client->state.is_alive = true;
                        client->state.hp = GetClassMaxHp(client->state.plane_class, false);
                        client->state.position = spawnPos;
                        client->last_move_time = now;
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
                                client->last_move_time = now;
                                InitServerPlayerWeapons(client);
                                break;
                            }
                        }
                    }
                }

                if (client->state.is_alive && room->current_round <= 2) {
                    double dt_move = now - client->last_move_time;
                    client->last_move_time = now;
                    if (dt_move < 0.004) dt_move = 0.004;
                    if (dt_move > 0.35)  dt_move = 0.35;

                    float maxSpeed = GetClassMaxAllowedSpeed(client->state.plane_class, client->state.is_drone);
                    float maxAllowedDist = maxSpeed * 1.35f * (float)dt_move + 3.0f;

                    float dx = inPacket.position.x - client->state.position.x;
                    float dy = inPacket.position.y - client->state.position.y;
                    float dz = inPacket.position.z - client->state.position.z;
                    float dist = sqrtf(dx * dx + dy * dy + dz * dz);

                    if (dist <= maxAllowedDist) {
                        client->state.position = inPacket.position;
                    } else if (dist > 0.001f) {
                        float ratio = maxAllowedDist / dist;
                        client->state.position.x += dx * ratio;
                        client->state.position.y += dy * ratio;
                        client->state.position.z += dz * ratio;
                    }

                    if (client->state.position.x >  1000.0f) client->state.position.x =  1000.0f;
                    if (client->state.position.x < -1000.0f) client->state.position.x = -1000.0f;
                    if (client->state.position.z >  1000.0f) client->state.position.z =  1000.0f;
                    if (client->state.position.z < -1000.0f) client->state.position.z = -1000.0f;
                    if (client->state.position.y >   500.0f) client->state.position.y =   500.0f;

                    client->state.pitch = inPacket.pitch;
                    client->state.yaw   = inPacket.yaw;
                    client->state.roll  = inPacket.roll;

                    if (inPacket.fire_slot >= 1 && inPacket.fire_slot <= MAX_WEAPON_SLOTS) {
                        Weapon *w = &client->weapons[inPacket.fire_slot - 1];
                        if (w->ammo > 0) {
                            int chosenSlot = -1;
                            for (int p = 0; p < MAX_PLAYER_PROJECTILE; p++) {
                                if (!client->projectiles[p].active) { chosenSlot = p; break; }
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

                            Vector3 dir = {
                                -cosf(client->state.pitch) * sinf(client->state.yaw),
                                 sinf(client->state.pitch),
                                -cosf(client->state.pitch) * cosf(client->state.yaw)
                            };

                            int8_t tKind = 0;
                            int8_t tIdx = -1;
                            if (w->type == WEAPON_AIR_TO_AIR) {
                                FindHomingTarget(room, slot, client->state.position, dir, is_attacker, &tKind, &tIdx);
                            }

                            w->ammo--;
                            client->projectiles[chosenSlot] = (ServerProjectile){
                                .position = client->state.position,
                                .direction = dir,
                                .speed = w->projectile_speed,
                                .lifetime = 3.0f,
                                .damage = w->damage,
                                .type = (uint8_t)w->type,
                                .active = true,
                                .target_kind = tKind,
                                .target_idx = tIdx
                            };
                        }
                    }
                }
            }
        }

        // --- 2. MATCHMAKING & SIMULATION 60 Hz ---
        while (accumulator >= TICK_DT) {
            float dt = (float)TICK_DT;

            for (int s = 0; s < MAX_LOBBY_ROOMS; s++) {
                LobbyRoom *sq = &lobbyRooms[s];
                for (int m = 0; m < MAX_SQUAD_SIZE; m++) {
                    if (sq->members[m].connected && (now - sq->members[m].last_seen) > 15.0) {
                        sq->members[m].connected = false;
                        sq->in_matchmaking = false;
                    }
                }
                if (sq->in_matchmaking && (now - sq->matchmaking_start) >= 2.5) {
                    int opponentSq = -1;
                    for (int other = 0; other < MAX_LOBBY_ROOMS; other++) {
                        if (other != s && lobbyRooms[other].in_matchmaking && CountSquadMembers(&lobbyRooms[other]) > 0) {
                            opponentSq = other;
                            break;
                        }
                    }

                    int chosenGameRoom = -1;
                    uint8_t sqSize = CountSquadMembers(sq);

                    if (opponentSq == -1) {
                        for (int gr = 0; gr < MAX_GAME_ROOMS; gr++) {
                            uint8_t active = CountGameRoomPlayers(&gameRooms[gr]);
                            if (active > 0 && (active + sqSize) <= MAX_CLIENTS && gameRooms[gr].current_round == 1) {
                                int team0Count = 0, team1Count = 0;
                                for (int c = 0; c < 5; c++) if (gameRooms[gr].clients[c].connected) team0Count++;
                                for (int c = 5; c < 10; c++) if (gameRooms[gr].clients[c].connected) team1Count++;
                                int prefTeam = (team1Count <= team0Count) ? 1 : 0;
                                AssignSquadToGameRoom(sq, &gameRooms[gr], gr, prefTeam, now);
                                chosenGameRoom = gr;
                                printf("[Matchmaking] Escouade #%d rejoint la GameRoom #%d en cours !\n", s + 1, gr + 1);
                                break;
                            }
                        }
                    }

                    if (chosenGameRoom == -1) {
                        for (int gr = 0; gr < MAX_GAME_ROOMS; gr++) {
                            if (CountGameRoomPlayers(&gameRooms[gr]) == 0) {
                                chosenGameRoom = gr;
                                ResetGameRoom(&gameRooms[gr], gr);
                                gameRooms[gr].map_seed = 133740u + (unsigned int)(sq->preferred_city % 4);
                                StartRoomRound(&gameRooms[gr], 1);

                                AssignSquadToGameRoom(sq, &gameRooms[gr], gr, 1, now);
                                if (opponentSq != -1) {
                                    AssignSquadToGameRoom(&lobbyRooms[opponentSq], &gameRooms[gr], gr, 0, now);
                                }
                                printf("[Matchmaking] Match lance dans GameRoom #%d (%s) !\n",
                                       gr + 1, GetCityName(gameRooms[gr].map.city_type));
                                break;
                            }
                        }
                    }
                }
            }

            for (int r = 0; r < MAX_GAME_ROOMS; r++) {
                UpdateGameRoomTick(&gameRooms[r], r, lobbyRooms, sock, now, dt);
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