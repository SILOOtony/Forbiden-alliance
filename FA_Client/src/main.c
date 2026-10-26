#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include "input.h"
#include "playerMovment.h"
#include "weapons.h"
#include "colision.h"
#include "hud.h"
#include "menu.h"
#include "drone.h"
#include "network.h"

#define TICK_RATE 60.0f
#define TICK_DT (1.0f / TICK_RATE)
#define MAX_EXPLOSIONS 24

typedef struct {
    Vector3 pos;
    float timer;
    float max_time;
    bool active;
} ExplosionFX;

static Sound CreateSynthSound(int type) {
    int sampleRate = 22050;
    int sampleCount = (type == 0) ? (sampleRate / 6) : (sampleRate / 2);
    short *samples = (short *)malloc((size_t)sampleCount * sizeof(short));

    for (int i = 0; i < sampleCount; i++) {
        float t = (float)i / (float)sampleRate;
        float env = 1.0f - ((float)i / (float)sampleCount);
        if (type == 0) {
            float freq = 580.0f - 420.0f * ((float)i / (float)sampleCount);
            samples[i] = (short)(sinf(2.0f * PI * freq * t) * 14000.0f * env);
        } else {
            float noise = ((float)(rand() % 2000) / 1000.0f) - 1.0f;
            float boom  = sinf(2.0f * PI * 65.0f * t);
            samples[i] = (short)((noise * 0.65f + boom * 0.35f) * 20000.0f * env * env);
        }
    }

    Wave wave = {
        .frameCount = (unsigned int)sampleCount,
        .sampleRate = (unsigned int)sampleRate,
        .sampleSize = 16,
        .channels = 1,
        .data = samples
    };
    Sound s = LoadSoundFromWave(wave);
    free(samples);
    return s;
}

static void SpawnExplosion(ExplosionFX fx[], Vector3 pos) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!fx[i].active) {
            fx[i] = (ExplosionFX){ .pos = pos, .timer = 0.0f, .max_time = 0.65f, .active = true };
            return;
        }
    }
}

int main(void) {
    InitWindow(1280, 720, "Forbidden Alliance - Vol 3D (5v5)");
    InitAudioDevice();
    SetTargetFPS(60);
    ToggleFullscreen();

    Sound sfxShoot = CreateSynthSound(0);
    Sound sfxBoom  = CreateSynthSound(1);
    ExplosionFX explosions[MAX_EXPLOSIONS] = { 0 };

    int netSocket = socket(AF_INET, SOCK_DGRAM, 0);
    int flags = fcntl(netSocket, F_GETFL, 0);
    fcntl(netSocket, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    ServerPacket latestServerData = { 0 };
    latestServerData.your_id = -1;
    latestServerData.current_round = 1;
    latestServerData.round_time_left = (uint16_t)ROUND_DURATION_SEC;

    uint32_t clientSeq = 0;
    uint32_t lastServerSeq = 0;

    int currentRoom = -1;
    uint8_t connectedInRoom = 0;
    float roomHeartbeatTimer = 0.0f;
    uint8_t localRound = 1;

    char username[MAX_USERNAME_LEN] = "tony";
    bool accountLoaded = false;
    uint8_t selectedClass = PLANE_FIGHTER;
    uint8_t selectedSkin  = 0;
    uint8_t selectedCity  = CITY_PARIS;
    int accountXp = 0;
    int accountGems = 0;

    uint8_t pendingFireSlot = 0;
    uint8_t pendingRespawnCmd = RESPAWN_NONE;

    PlayerState remotePlayers[MAX_CLIENTS] = { 0 };
    Vector3 remoteTargetPos[MAX_CLIENTS] = { 0 };
    bool remoteHasTarget[MAX_CLIENTS] = { 0 };

    Projectile remoteProjectiles[MAX_PROJECTILES] = { 0 };

    KeyBindings controls = InitKeyBinds();
    GameMap cityMap = { 0 };
    InitMap(&cityMap, 133740u + selectedCity);

    PlayerState plane = { 0 };
    plane.team = 1;
    plane.is_attacker = true;
    plane.plane_class = selectedClass;
    plane.skin_id = selectedSkin;
    plane.plane_respawns_left = 3;
    InitPlayerState(&plane);

    LobbyClientPacket initLogin = {
        .type = PACKET_LOBBY,
        .action = LOBBY_LOGIN,
        .room_id = -1,
        .plane_class = selectedClass,
        .skin_id = selectedSkin,
        .city_type = selectedCity
    };
    strncpy(initLogin.username, username, MAX_USERNAME_LEN - 1);
    sendto(netSocket, &initLogin, sizeof(initLogin), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Projectile projectile[MAX_PLAYER_PROJECTILE] = { 0 };
    Drone drones[MAX_DRONE_COUNT];
    SpawnDrones(drones, MAX_DRONE_COUNT);

    GameStatus status = STATUS_MENU;
    float menuCameraAngle = 0.0f;
    float accumulator = 0.0f;

    while (!WindowShouldClose() && status != STATUS_QUIT) {
        float frameTime = GetFrameTime();
        if (frameTime > 0.25f) frameTime = 0.25f;

        if (status == STATUS_PLAYING && plane.is_alive && latestServerData.current_round <= 2) {
            if (!IsCursorHidden()) DisableCursor();
        } else {
            if (IsCursorHidden()) EnableCursor();
        }

        // --- 1. ENTRÉES INSTANTANÉES ---
        if (status == STATUS_PLAYING) {
            if (IsKeyPressed(KEY_P)) status = STATUS_PAUSED;

            if (plane.is_alive && latestServerData.current_round <= 2) {
                if (IsKeyPressed(controls.bindings[ACTION_SLOT_1])) plane.current_slot = 0;
                if (IsKeyPressed(controls.bindings[ACTION_SLOT_2])) plane.current_slot = 1;
                if (IsKeyPressed(controls.bindings[ACTION_SLOT_3])) plane.current_slot = 2;

                float wheel = GetMouseWheelMove();
                if (wheel > 0) {
                    plane.current_slot = (plane.current_slot - 1 + MAX_WEAPON_SLOTS) % MAX_WEAPON_SLOTS;
                } else if (wheel < 0) {
                    plane.current_slot = (plane.current_slot + 1) % MAX_WEAPON_SLOTS;
                }

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(controls.bindings[ACTION_ATTACK]) || IsKeyPressed(KEY_SPACE)) {
                    if (SpawnProjectile(&plane, projectile, MAX_PLAYER_PROJECTILE)) {
                        pendingFireSlot = (uint8_t)(plane.current_slot + 1);
                        PlaySound(sfxShoot);
                    }
                }
            } else if (!plane.is_alive && latestServerData.current_round <= 2) {
                if (plane.plane_respawns_left > 0) {
                    if (IsKeyPressed(KEY_R)) {
                        pendingRespawnCmd = RESPAWN_PLANE;
                        plane.plane_respawns_left--;
                        plane.plane_class = selectedClass;
                        plane.skin_id = selectedSkin;
                        memset(projectile, 0, sizeof(projectile));
                        InitPlayerState(&plane);
                    }
                } else {
                    if (IsKeyPressed(KEY_E)) {
                        pendingRespawnCmd = RESPAWN_DRONE;
                        int startIdx = plane.is_attacker ? DEFENDER_DRONE_COUNT : 0;
                        int endIdx   = plane.is_attacker ? MAX_DRONE_COUNT : DEFENDER_DRONE_COUNT;
                        for (int d = startIdx; d < endIdx; d++) {
                            if (drones[d].is_alive) {
                                drones[d].is_alive = false;
                                memset(projectile, 0, sizeof(projectile));
                                ReincarnateAsDrone(&plane, drones[d].pos);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (status == STATUS_PAUSED) {
            if (IsKeyPressed(KEY_P)) status = STATUS_PLAYING;
        }

        // --- 2. BOUCLE DE SIMULATION PAR TICKS (60 Hz) ---
        accumulator += frameTime;
        while (accumulator >= TICK_DT) {
            for (int e = 0; e < MAX_EXPLOSIONS; e++) {
                if (explosions[e].active) {
                    explosions[e].timer += TICK_DT;
                    if (explosions[e].timer >= explosions[e].max_time) explosions[e].active = false;
                }
            }

            if (status == STATUS_MENU) {
                menuCameraAngle += 0.15f * TICK_DT;
                UpdateMenuCamera(&camera, menuCameraAngle);

                if (currentRoom != -1) {
                    roomHeartbeatTimer -= TICK_DT;
                    if (roomHeartbeatTimer <= 0.0f) {
                        LobbyClientPacket hb = {
                            .type = PACKET_LOBBY,
                            .action = LOBBY_HEARTBEAT,
                            .room_id = (int8_t)currentRoom,
                            .plane_class = selectedClass,
                            .skin_id = selectedSkin,
                            .city_type = selectedCity
                        };
                        strncpy(hb.username, username, MAX_USERNAME_LEN - 1);
                        sendto(netSocket, &hb, sizeof(hb), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                        roomHeartbeatTimer = 0.3f;
                    }
                }
            } 
            else if (status == STATUS_PLAYING || status == STATUS_PAUSED) {
                int16_t dummy_bldg_dmg[MAX_BUILDINGS] = { 0 };
                bool wasAlive = plane.is_alive;

                if (status == STATUS_PLAYING && plane.is_alive && latestServerData.current_round <= 2) {
                    UpdatePlayerMovement(&plane, &controls, TICK_DT);
                    UpdatePlaneCamera(&camera, &plane);
                    UpdateWeaponsReload(plane.weapons, TICK_DT);
                    if (CheckPlaneMapCollision(&plane, &cityMap, dummy_bldg_dmg) && wasAlive) {
                        SpawnExplosion(explosions, plane.position);
                        PlaySound(sfxBoom);
                    }
                }

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (i != latestServerData.your_id && latestServerData.connected[i] && remotePlayers[i].is_alive && remoteHasTarget[i]) {
                        remotePlayers[i].position = Vector3Lerp(remotePlayers[i].position, remoteTargetPos[i], 0.35f);
                    }
                }

                UpdateProjectiles(projectile, MAX_PLAYER_PROJECTILE, TICK_DT);
                CheckProjectileMapCollisions(projectile, MAX_PLAYER_PROJECTILE, &cityMap, plane.is_attacker, dummy_bldg_dmg);
                CheckProjectileDroneCollisions(projectile, MAX_PLAYER_PROJECTILE, drones, MAX_DRONE_COUNT, plane.is_attacker);

                if (latestServerData.your_id == -1) {
                    UpdateDrones(drones, MAX_DRONE_COUNT, &cityMap, NULL, NULL, TICK_DT);
                }

                clientSeq++;
                ClientPacket outPacket = {
                    .type = PACKET_GAME,
                    .room_id = (int8_t)currentRoom,
                    .seq = clientSeq,
                    .position = plane.position,
                    .pitch = plane.pitch,
                    .yaw = plane.yaw,
                    .roll = plane.roll,
                    .plane_class = selectedClass,
                    .skin_id = selectedSkin,
                    .city_type = selectedCity,
                    .fire_slot = pendingFireSlot,
                    .respawn_cmd = pendingRespawnCmd
                };
                strncpy(outPacket.username, username, sizeof(outPacket.username) - 1);
                pendingFireSlot = 0;
                pendingRespawnCmd = RESPAWN_NONE;

                sendto(netSocket, &outPacket, sizeof(outPacket), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
            }

            // --- RÉCEPTION DES PAQUETS SERVEUR ---
            uint8_t recvBuf[sizeof(ServerPacket)];
            ssize_t received;
            while ((received = recvfrom(netSocket, recvBuf, sizeof(recvBuf), 0, NULL, NULL)) > 0) {
                uint8_t packetType = recvBuf[0];

                if (packetType == PACKET_LOBBY && received == sizeof(LobbyServerPacket)) {
                    LobbyServerPacket lobbyIn;
                    memcpy(&lobbyIn, recvBuf, sizeof(LobbyServerPacket));
                    currentRoom = lobbyIn.room_id;
                    connectedInRoom = lobbyIn.connected_count;

                    if (lobbyIn.account_loaded) {
                        accountLoaded = true;
                        selectedClass = lobbyIn.plane_class;
                        selectedSkin  = lobbyIn.skin_id;
                        plane.plane_class = selectedClass;
                        plane.skin_id     = selectedSkin;
                        accountXp = lobbyIn.xp;
                        accountGems = lobbyIn.gems;
                        strncpy(username, lobbyIn.username, MAX_USERNAME_LEN - 1);
                    }
                    if (currentRoom != -1) {
                        selectedCity = lobbyIn.city_type % 4;
                        if (cityMap.city_type != selectedCity) {
                            InitMap(&cityMap, 133740u + selectedCity);
                        }
                    }
                }
                else if (packetType == PACKET_GAME && received >= (ssize_t)SERVER_PACKET_BASE_SIZE) {
                    ServerPacket inPacket = { 0 };
                    memcpy(&inPacket, recvBuf, (size_t)received);

                    size_t expectedSize = SERVER_PACKET_BASE_SIZE + (size_t)inPacket.active_projectile_count * sizeof(NetProjectile);
                    if ((size_t)received != expectedSize || inPacket.active_projectile_count > MAX_PROJECTILES) {
                        continue;
                    }

                    if (currentRoom == -1) currentRoom = inPacket.room_id;
                    if (inPacket.room_id != currentRoom) continue;

                    if (inPacket.seq <= lastServerSeq && (lastServerSeq - inPacket.seq) < 100000u) continue;
                    lastServerSeq = inPacket.seq;

                    uint8_t prevTeam = plane.team;
                    bool prevAttacker = plane.is_attacker;
                    bool firstConnect = (latestServerData.your_id == -1);

                    latestServerData = inPacket;
                    connectedInRoom = inPacket.connected_count;

                    plane.team = (inPacket.your_id < 5) ? 0 : 1;
                    plane.is_attacker = (inPacket.current_round == 1) ? (plane.team == 1) : (plane.team == 0);

                    if (firstConnect && (plane.team != prevTeam || plane.is_attacker != prevAttacker)) {
                        plane.plane_class = selectedClass;
                        plane.skin_id = selectedSkin;
                        InitPlayerState(&plane);
                    }

                    if (inPacket.your_id >= 0 && inPacket.your_id < MAX_CLIENTS && inPacket.current_round <= 2) {
                        NetPlayer me = inPacket.players[inPacket.your_id];
                        if (plane.is_alive && !me.is_alive) {
                            SpawnExplosion(explosions, plane.position);
                            PlaySound(sfxBoom);
                        }
                        plane.hp = me.hp;
                        plane.is_alive = me.is_alive;
                        plane.plane_respawns_left = me.respawns_left;
                        if (me.is_drone && !plane.is_drone) {
                            ReincarnateAsDrone(&plane, me.position);
                        }
                    }

                    if (cityMap.seed != inPacket.map_seed && inPacket.map_seed != 0) {
                        InitMap(&cityMap, inPacket.map_seed);
                        selectedCity = cityMap.city_type;
                    }

                    if (inPacket.current_round != localRound) {
                        localRound = inPacket.current_round;
                        memset(projectile, 0, sizeof(projectile));
                        memset(remoteHasTarget, 0, sizeof(remoteHasTarget));
                        if (localRound <= 2) {
                            InitMap(&cityMap, inPacket.map_seed);
                            SpawnDrones(drones, MAX_DRONE_COUNT);
                            plane.plane_class = selectedClass;
                            plane.skin_id = selectedSkin;
                            InitPlayerState(&plane);
                            plane.plane_respawns_left = 3;
                        } else if (localRound == 3) {
                            LobbyClientPacket req = {
                                .type = PACKET_LOBBY, .action = LOBBY_LOGIN,
                                .room_id = (int8_t)currentRoom, .plane_class = selectedClass,
                                .skin_id = selectedSkin, .city_type = selectedCity
                            };
                            strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                            sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                        }
                    }

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        Vector3 netPos = inPacket.players[i].position;
                        float distSq = Vector3DistanceSqr(remotePlayers[i].position, netPos);
                        if (!remoteHasTarget[i] || !inPacket.players[i].is_alive || distSq > 2500.0f) {
                            remotePlayers[i].position = netPos;
                            remoteHasTarget[i] = inPacket.players[i].is_alive;
                        }
                        remoteTargetPos[i]           = netPos;
                        remotePlayers[i].pitch       = inPacket.players[i].pitch;
                        remotePlayers[i].yaw         = inPacket.players[i].yaw;
                        remotePlayers[i].roll        = inPacket.players[i].roll;
                        remotePlayers[i].hp          = inPacket.players[i].hp;
                        remotePlayers[i].is_alive    = inPacket.players[i].is_alive;
                        remotePlayers[i].is_drone    = inPacket.players[i].is_drone;
                        remotePlayers[i].team        = inPacket.players[i].team;
                        remotePlayers[i].plane_class = inPacket.players[i].plane_class;
                        remotePlayers[i].skin_id     = inPacket.players[i].skin_id;
                    }

                    for (int i = 0; i < MAX_PROJECTILES; i++) {
                        if (i < (int)inPacket.active_projectile_count) {
                            NetProjectile np = inPacket.projectiles[i];
                            remoteProjectiles[i].position = (Vector3){
                                DequantizeCoord(np.x),
                                DequantizeCoord(np.y),
                                DequantizeCoord(np.z)
                            };
                            remoteProjectiles[i].type   = (WeaponType)(np.type_and_active & 0x7F);
                            remoteProjectiles[i].active = ((np.type_and_active & 0x80) != 0);
                        } else {
                            remoteProjectiles[i].active = false;
                        }
                    }

                    for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                        drones[d].pos = (Vector3){
                            DequantizeCoord(inPacket.drones[d].x),
                            DequantizeCoord(inPacket.drones[d].y),
                            DequantizeCoord(inPacket.drones[d].z)
                        };
                        drones[d].bullet_pos = (Vector3){
                            DequantizeCoord(inPacket.drones[d].bx),
                            DequantizeCoord(inPacket.drones[d].by),
                            DequantizeCoord(inPacket.drones[d].bz)
                        };
                        drones[d].is_alive      = ((inPacket.drones[d].flags & 0x01) != 0);
                        drones[d].bullet_active = ((inPacket.drones[d].flags & 0x02) != 0);
                    }

                    for (int b = 0; b < cityMap.building_count; b++) {
                        bool wasDestroyed = cityMap.buildings[b].is_destroyed;
                        cityMap.buildings[b].hp = (int)inPacket.building_hp[b];
                        cityMap.buildings[b].is_destroyed = (inPacket.building_hp[b] == 0);
                        if (!wasDestroyed && cityMap.buildings[b].is_destroyed) {
                            SpawnExplosion(explosions, cityMap.buildings[b].position);
                            PlaySound(sfxBoom);
                        }
                    }
                }
            }

            accumulator -= TICK_DT;
        }

        // --- 3. AFFICHAGE ---
        BeginDrawing();
            Color skyCol = (cityMap.city_type == CITY_TOKYO) ? (Color){ 20, 24, 52, 255 } :
                           ((cityMap.city_type == CITY_DUBAI) ? (Color){ 135, 195, 250, 255 } : SKYBLUE);
            ClearBackground(skyCol);

            BeginMode3D(camera);
                DrawMap(&cityMap);
                DrawDrones(drones, MAX_DRONE_COUNT);

                for (int e = 0; e < MAX_EXPLOSIONS; e++) {
                    if (explosions[e].active) {
                        float p = explosions[e].timer / explosions[e].max_time;
                        float rad = 4.0f + p * 22.0f;
                        DrawSphere(explosions[e].pos, rad, Fade(ORANGE, 1.0f - p));
                        DrawSphereWires(explosions[e].pos, rad * 1.15f, 6, 6, Fade(YELLOW, 1.0f - p));
                    }
                }

                if (status == STATUS_PLAYING || status == STATUS_PAUSED) {
                    if (plane.is_alive) {
                        DrawPlanePlayer(&plane);
                        DrawCrossHair(&plane);
                    }

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (i == latestServerData.your_id) continue;
                        if (latestServerData.connected[i] && remotePlayers[i].is_alive) {
                            DrawPlanePlayer(&remotePlayers[i]);
                            Vector3 markerPos = remotePlayers[i].position;
                            markerPos.y += 4.5f;
                            Color teamColor = (latestServerData.players[i].team == 0) ? LIME : RED;
                            DrawSphere(markerPos, 1.2f, teamColor);
                        }
                    }

                    if (latestServerData.your_id != -1) {
                        DrawProjectiles(remoteProjectiles, MAX_PROJECTILES);
                    } else {
                        DrawProjectiles(projectile, MAX_PLAYER_PROJECTILE);
                    }
                }
            EndMode3D();

            // Correctif du bug à 180° : on vérifie que le joueur est DEVANT la caméra (Produit Scalaire > 0)
            if (status == STATUS_PLAYING || status == STATUS_PAUSED) {
                Vector3 camForward = Vector3Subtract(camera.target, camera.position);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (i == latestServerData.your_id) continue;
                    if (latestServerData.connected[i] && remotePlayers[i].is_alive) {
                        Vector3 tagPos = remotePlayers[i].position;
                        tagPos.y += 7.5f;
                        Vector3 toTarget = Vector3Subtract(tagPos, camera.position);

                        if (Vector3DotProduct(camForward, toTarget) > 0.0f) {
                            Vector2 screenPos = GetWorldToScreen(tagPos, camera);
                            if (screenPos.x > 0 && screenPos.x < GetScreenWidth() && screenPos.y > 0 && screenPos.y < GetScreenHeight()) {
                                const char *pName = latestServerData.players[i].username;
                                int tw = MeasureText(pName, 16);
                                Color col = (latestServerData.players[i].team == 0) ? LIME : RED;
                                DrawRectangle((int)screenPos.x - tw / 2 - 4, (int)screenPos.y - 2, tw + 8, 20, Fade(BLACK, 0.6f));
                                DrawText(pName, (int)screenPos.x - tw / 2, (int)screenPos.y, 16, col);
                            }
                        }
                    }
                }
            }

            DrawFPS(10, 10);

            if (status == STATUS_PLAYING) {
                DrawCityTopBar(&cityMap, latestServerData.current_round, latestServerData.round_time_left, plane.is_attacker);

                const char *cName = (plane.plane_class == PLANE_INTERCEPTOR) ? "INTERCEPTEUR" :
                                    ((plane.plane_class == PLANE_BOMBER) ? "BOMBARDIER" : "CHASSEUR");

                if (latestServerData.your_id != -1) {
                    DrawText(TextFormat("Pilote: %s (XP:%d | Gems:%d) | ROOM #%d (%d/10) | Equipe %d [%s]",
                             username, accountXp, accountGems, currentRoom + 1, connectedInRoom, plane.team + 1,
                             plane.is_drone ? "DRONE" : cName), 10, 80, 18, DARKGREEN);
                } else {
                    DrawText(TextFormat("HORS LIGNE | Pilote: %s [%s]", username, plane.is_drone ? "DRONE" : cName), 10, 80, 18, RED);
                }

                DrawWeaponSlots(&plane);
                DrawHpBar(&plane);

                if (latestServerData.current_round == 3) {
                    DrawMatchEndScreen(latestServerData.team_buildings_destroyed,
                                       latestServerData.team_city_damage,
                                       latestServerData.round_time_left);
                } else if (!plane.is_alive) {
                    DrawDeathScreen(&plane);
                }
            } else {
                int joinRoomId = -1;
                MenuCommand cmd = DrawMenu(&status, currentRoom, connectedInRoom, &joinRoomId,
                                           username, accountLoaded, &selectedClass, &selectedSkin, &selectedCity,
                                           accountXp, accountGems);

                if (cmd == MENU_CMD_LOGIN) {
                    accountLoaded = false;
                    LobbyClientPacket req = {
                        .type = PACKET_LOBBY, .action = LOBBY_LOGIN, .room_id = (int8_t)currentRoom,
                        .plane_class = selectedClass, .skin_id = selectedSkin, .city_type = selectedCity
                    };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_SELECT_CLASS) {
                    plane.plane_class = selectedClass;
                    plane.skin_id = selectedSkin;
                    LobbyClientPacket req = {
                        .type = PACKET_LOBBY, .action = LOBBY_SET_CLASS, .room_id = (int8_t)currentRoom,
                        .plane_class = selectedClass, .skin_id = selectedSkin, .city_type = selectedCity
                    };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_SELECT_CITY) {
                    InitMap(&cityMap, 133740u + selectedCity);
                }
                else if (cmd == MENU_CMD_CREATE_ROOM) {
                    LobbyClientPacket req = {
                        .type = PACKET_LOBBY, .action = LOBBY_CREATE, .room_id = -1,
                        .plane_class = selectedClass, .skin_id = selectedSkin, .city_type = selectedCity
                    };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_JOIN_ROOM && joinRoomId >= 0 && joinRoomId < MAX_ROOMS) {
                    LobbyClientPacket req = {
                        .type = PACKET_LOBBY, .action = LOBBY_JOIN, .room_id = (int8_t)joinRoomId,
                        .plane_class = selectedClass, .skin_id = selectedSkin, .city_type = selectedCity
                    };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_LEAVE_ROOM) {
                    LobbyClientPacket req = {
                        .type = PACKET_LOBBY, .action = LOBBY_LEAVE, .room_id = (int8_t)currentRoom,
                        .plane_class = selectedClass, .skin_id = selectedSkin, .city_type = selectedCity
                    };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                    currentRoom = -1;
                    connectedInRoom = 0;
                    latestServerData.your_id = -1;
                    lastServerSeq = 0;
                }
                else if (cmd == MENU_CMD_START_GAME) {
                    latestServerData.your_id = -1;
                    lastServerSeq = 0;
                    localRound = 1;
                    memset(projectile, 0, sizeof(projectile));
                    memset(remoteHasTarget, 0, sizeof(remoteHasTarget));
                    plane.plane_class = selectedClass;
                    plane.skin_id = selectedSkin;
                    plane.plane_respawns_left = 3;
                    InitPlayerState(&plane);
                    InitMap(&cityMap, 133740u + selectedCity);
                    SpawnDrones(drones, MAX_DRONE_COUNT);
                }
            }
        EndDrawing();
    }

    if (IsCursorHidden()) EnableCursor();
    UnloadSound(sfxShoot);
    UnloadSound(sfxBoom);
    CloseAudioDevice();
    close(netSocket);
    CloseWindow();
    return 0;
}