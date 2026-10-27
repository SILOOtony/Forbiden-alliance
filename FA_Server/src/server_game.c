#include "server.h"
#include <string.h>
#include <math.h>
#include <sys/socket.h>

bool IsSameClient(struct sockaddr_in a, struct sockaddr_in b) {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
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

int16_t GetClassMaxHp(uint8_t plane_class, bool is_drone) {
    if (is_drone) return 100;
    if (plane_class == PLANE_INTERCEPTOR) return 75;
    if (plane_class == PLANE_BOMBER) return 150;
    return 100;
}

float GetClassMaxAllowedSpeed(uint8_t plane_class, bool is_drone) {
    if (is_drone) return 55.0f;
    if (plane_class == PLANE_INTERCEPTOR) return 135.0f;
    if (plane_class == PLANE_BOMBER) return 55.0f;
    return 75.0f;
}

void InitServerPlayerWeapons(ClientSlot *c) {
    InitWeaponSlotsForClass(c->weapons, (PlaneClass)c->state.plane_class);
    if (c->state.is_drone) {
        for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
            c->weapons[i] = (Weapon){
                .type = WEAPON_DRONE_BLASTER, .name = "Drone Blaster",
                .ammo = 25, .max_ammo = 25, .reload_time = 0.25f, .reload_timer = 0.0f,
                .projectile_speed = 360.0f, .damage = 50
            };
        }
    }
}

void StartRoomRound(GameRoom *room, uint8_t round_num) {
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
            room->clients[i].flare_timer = 0.0f;
            memset(room->clients[i].projectiles, 0, sizeof(room->clients[i].projectiles));
            InitServerPlayerWeapons(&room->clients[i]);
        }
    } else if (round_num == 3) {
        RewardMatchPlayersJSON(room);
    }
}

void ResetGameRoom(GameRoom *room, int room_index) {
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

uint8_t CountGameRoomPlayers(const GameRoom *room) {
    uint8_t count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) if (room->clients[i].connected) count++;
    return count;
}

void FindHomingTarget(
    const GameRoom *room,
    int shooterSlot,
    Vector3 startPos,
    Vector3 forward,
    bool shooterIsAttacker,
    int8_t *outKind,
    int8_t *outIdx
) {
    *outKind = 0;
    *outIdx = -1;
    float bestDot = 0.92f;
    uint8_t myTeam = (shooterSlot < 5) ? 0 : 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i == shooterSlot || !room->clients[i].connected || !room->clients[i].state.is_alive) continue;
        uint8_t tTeam = (i < 5) ? 0 : 1;
        if (tTeam == myTeam) continue;
        // Si l'ennemi a activé ses leurres, il est impossible à verrouiller !
        if (room->clients[i].flare_timer > 0.0f) continue;

        float dx = room->clients[i].state.position.x - startPos.x;
        float dy = room->clients[i].state.position.y - startPos.y;
        float dz = room->clients[i].state.position.z - startPos.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < 1.0f || dist > 800.0f) continue;

        float dot = (dx * forward.x + dy * forward.y + dz * forward.z) / dist;
        if (dot > bestDot) {
            bestDot = dot;
            *outKind = 1;
            *outIdx = (int8_t)i;
        }
    }

    for (int d = 0; d < MAX_DRONE_COUNT; d++) {
        if (!room->drones[d].is_alive || room->drones[d].is_attacker == shooterIsAttacker) continue;

        float dx = room->drones[d].pos.x - startPos.x;
        float dy = room->drones[d].pos.y - startPos.y;
        float dz = room->drones[d].pos.z - startPos.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < 1.0f || dist > 800.0f) continue;

        float dot = (dx * forward.x + dy * forward.y + dz * forward.z) / dist;
        if (dot > bestDot) {
            bestDot = dot;
            *outKind = 2;
            *outIdx = (int8_t)d;
        }
    }
}

void UpdateGameRoomTick(
    GameRoom *room,
    int room_id,
    LobbyRoom lobbyRooms[],
    int sock,
    double now,
    float dt
) {
    uint8_t activePlayers = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (room->clients[i].connected) {
            if ((now - room->clients[i].last_seen) > 10.0) {
                room->clients[i].connected = false;
            } else {
                activePlayers++;
            }
        }
    }

    if (activePlayers == 0) {
        if (room->had_players) {
            ResetGameRoom(room, room_id);
        }
        return;
    }

    room->round_timer -= dt;
    int remainingBuildings = GetCityRemainingBuildings(&room->map);

    if (room->current_round == 1 && (room->round_timer <= 0.0f || remainingBuildings == 0)) {
        StartRoomRound(room, 2);
    } else if (room->current_round == 2 && (room->round_timer <= 0.0f || remainingBuildings == 0)) {
        StartRoomRound(room, 3);
    } else if (room->current_round == 3 && room->round_timer <= 0.0f) {
        for (int s = 0; s < MAX_LOBBY_ROOMS; s++) {
            for (int m = 0; m < MAX_SQUAD_SIZE; m++) {
                if (lobbyRooms[s].members[m].assigned_game_room == room_id) {
                    lobbyRooms[s].members[m].assigned_game_room = -1;
                    lobbyRooms[s].members[m].is_ready = false;
                }
            }
        }
        ResetGameRoom(room, room_id);
        return;
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

            if (c->flare_timer > 0.0f) {
                c->flare_timer -= dt;
                if (c->flare_timer < 0.0f) c->flare_timer = 0.0f;
            }

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

                if (proj->type == WEAPON_AIR_TO_AIR && proj->target_kind != 0) {
                    Vector3 targetPos = { 0 };
                    bool validTarget = false;
                    if (proj->target_kind == 1 && proj->target_idx >= 0 && proj->target_idx < MAX_CLIENTS) {
                        ClientSlot *tc = &room->clients[proj->target_idx];
                        if (tc->connected && tc->state.is_alive && tc->flare_timer <= 0.0f) {
                            targetPos = tc->state.position;
                            validTarget = true;
                        } else {
                            proj->target_kind = 0;
                            proj->target_idx = -1;
                        }
                    } else if (proj->target_kind == 2 && proj->target_idx >= 0 && proj->target_idx < MAX_DRONE_COUNT) {
                        if (room->drones[proj->target_idx].is_alive) {
                            targetPos = room->drones[proj->target_idx].pos;
                            validTarget = true;
                        } else {
                            proj->target_kind = 0;
                            proj->target_idx = -1;
                        }
                    }

                    if (validTarget) {
                        float tx = targetPos.x - proj->position.x;
                        float ty = targetPos.y - proj->position.y;
                        float tz = targetPos.z - proj->position.z;
                        float tlen = sqrtf(tx * tx + ty * ty + tz * tz);
                        if (tlen > 0.01f) {
                            tx /= tlen; ty /= tlen; tz /= tlen;
                            float turnRate = 3.2f * dt;
                            proj->direction.x += (tx - proj->direction.x) * turnRate;
                            proj->direction.y += (ty - proj->direction.y) * turnRate;
                            proj->direction.z += (tz - proj->direction.z) * turnRate;
                            float nlen = sqrtf(proj->direction.x * proj->direction.x +
                                               proj->direction.y * proj->direction.y +
                                               proj->direction.z * proj->direction.z);
                            if (nlen > 0.01f) {
                                proj->direction.x /= nlen;
                                proj->direction.y /= nlen;
                                proj->direction.z /= nlen;
                            }
                        }
                    }
                }

                proj->lifetime -= dt;
                proj->position.x += proj->direction.x * proj->speed * dt;
                proj->position.y += proj->direction.y * proj->speed * dt;
                proj->position.z += proj->direction.z * proj->speed * dt;

                if (proj->lifetime <= 0.0f || proj->position.y <= 0.0f) {
                    proj->active = false;
                    continue;
                }

                // Les leurres ne font pas de dégâts aux bâtiments/avions
                if (proj->type == WEAPON_FLARES) continue;

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
    outPacket.room_id = (int8_t)room_id;
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
                // Encodage de la cible verrouillée (1..10) sur les bits 3..6 de type_and_active
                uint8_t encodedTarget = 0;
                if (sp->type == WEAPON_AIR_TO_AIR && sp->target_kind == 1 && sp->target_idx >= 0 && sp->target_idx < MAX_CLIENTS) {
                    encodedTarget = (uint8_t)(sp->target_idx + 1);
                }
                outPacket.projectiles[activeProjCount++] = (NetProjectile){
                    .x = QuantizeCoord(sp->position.x),
                    .y = QuantizeCoord(sp->position.y),
                    .z = QuantizeCoord(sp->position.z),
                    .type_and_active = (uint8_t)((sp->type & 0x07) | ((encodedTarget & 0x0F) << 3) | 0x80)
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