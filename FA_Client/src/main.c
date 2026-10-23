#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

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

int main(void) {
    InitWindow(1280, 720, "Forbidden Alliance - Vol 3D (5v5)");
    SetTargetFPS(60);
    ToggleFullscreen();

    int netSocket = socket(AF_INET, SOCK_DGRAM, 0);
    int flags = fcntl(netSocket, F_GETFL, 0);
    fcntl(netSocket, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "157.173.112.67", &serverAddr.sin_addr);

    ServerPacket latestServerData = { 0 };
    latestServerData.your_id = -1;
    latestServerData.current_round = 1;
    latestServerData.round_time_left = (uint16_t)ROUND_DURATION_SEC;

    int currentRoom = -1;
    uint8_t connectedInRoom = 0;
    float roomHeartbeatTimer = 0.0f;
    uint8_t localRound = 1;

    // Compte JSON et Classe d'avion sélectionnée
    char username[MAX_USERNAME_LEN] = "tony";
    bool accountLoaded = false;
    uint8_t selectedClass = PLANE_FIGHTER;

    PlayerState remotePlayers[MAX_CLIENTS] = { 0 };
    Projectile remoteProjectiles[MAX_PROJECTILES] = { 0 };
    bool local_drone_destroyed[MAX_DRONE_COUNT] = { 0 };

    KeyBindings controls = InitKeyBinds();
    GameMap cityMap = { 0 };
    InitMap(&cityMap, 133742);

    PlayerState plane = { 0 };
    plane.team = 1;
    plane.is_attacker = true;
    plane.plane_class = selectedClass;
    plane.plane_respawns_left = 3;
    InitPlayerState(&plane);

    // Demande automatiquement au VPS de charger/créer le profil JSON au lancement
    LobbyClientPacket initLogin = {
        .type = PACKET_LOBBY,
        .action = LOBBY_LOGIN,
        .room_id = -1,
        .plane_class = selectedClass
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

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(controls.bindings[ACTION_ATTACK])) {
                    SpawnProjectile(&plane, projectile, MAX_PLAYER_PROJECTILE);
                }
            } else if (!plane.is_alive && latestServerData.current_round <= 2) {
                if (plane.plane_respawns_left > 0) {
                    if (IsKeyPressed(KEY_R)) {
                        plane.plane_respawns_left--;
                        plane.plane_class = selectedClass;
                        InitPlayerState(&plane);
                    }
                } else {
                    if (IsKeyPressed(KEY_E)) {
                        int startIdx = plane.is_attacker ? DEFENDER_DRONE_COUNT : 0;
                        int endIdx   = plane.is_attacker ? MAX_DRONE_COUNT : DEFENDER_DRONE_COUNT;
                        for (int d = startIdx; d < endIdx; d++) {
                            if (drones[d].is_alive) {
                                drones[d].is_alive = false;
                                local_drone_destroyed[d] = true;
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
                            .plane_class = selectedClass
                        };
                        strncpy(hb.username, username, MAX_USERNAME_LEN - 1);
                        sendto(netSocket, &hb, sizeof(hb), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                        roomHeartbeatTimer = 0.3f;
                    }
                }
            } 
            else if (status == STATUS_PLAYING || status == STATUS_PAUSED) {
                int16_t building_damage_dealt[MAX_BUILDINGS] = { 0 };

                if (status == STATUS_PLAYING && plane.is_alive && latestServerData.current_round <= 2) {
                    UpdatePlayerMovement(&plane, &controls, TICK_DT);
                    UpdatePlaneCamera(&camera, &plane);
                    UpdateWeaponsReload(plane.weapons, TICK_DT);
                    CheckPlaneMapCollision(&plane, &cityMap, building_damage_dealt);
                }

                if (latestServerData.your_id == -1) {
                    UpdateDrones(drones, MAX_DRONE_COUNT, TICK_DT);
                }

                UpdateProjectiles(projectile, MAX_PLAYER_PROJECTILE, TICK_DT);
                CheckProjectileMapCollisions(projectile, MAX_PLAYER_PROJECTILE, &cityMap, plane.is_attacker, building_damage_dealt);
                CheckProjectileDroneCollisions(projectile, MAX_PLAYER_PROJECTILE, drones, MAX_DRONE_COUNT, plane.is_attacker);

                for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                    if (!drones[d].is_alive) {
                        local_drone_destroyed[d] = true;
                    }
                }

                int damage_dealt[MAX_CLIENTS] = { 0 };
                CheckProjectilePlayersCollisions(
                    projectile,
                    MAX_PLAYER_PROJECTILE,
                    remotePlayers,
                    latestServerData.connected,
                    MAX_CLIENTS,
                    latestServerData.your_id,
                    plane.team,
                    damage_dealt
                );

                // --- ENVOI DU PAQUET DE JEU ---
                ClientPacket outPacket = { 0 };
                outPacket.type = PACKET_GAME;
                outPacket.room_id = (int8_t)currentRoom;
                outPacket.player = (NetPlayer){
                    .position = plane.position,
                    .pitch = plane.pitch,
                    .yaw = plane.yaw,
                    .roll = plane.roll,
                    .hp = (int16_t)plane.hp,
                    .is_alive = plane.is_alive,
                    .is_drone = plane.is_drone,
                    .team = plane.team,
                    .plane_class = plane.plane_class
                };

                for (int i = 0; i < MAX_PLAYER_PROJECTILE; i++) {
                    outPacket.projectiles[i] = (NetProjectile){
                        .position = projectile[i].position,
                        .type = (uint8_t)projectile[i].type,
                        .active = projectile[i].active
                    };
                }

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    outPacket.damage_dealt[i] = (int16_t)damage_dealt[i];
                }

                memcpy(outPacket.building_damage_dealt, building_damage_dealt, sizeof(building_damage_dealt));
                memcpy(outPacket.drone_destroyed, local_drone_destroyed, sizeof(local_drone_destroyed));

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
                        plane.plane_class = selectedClass;
                        strncpy(username, lobbyIn.username, MAX_USERNAME_LEN - 1);
                    }
                }
                else if (packetType == PACKET_GAME && received == sizeof(ServerPacket)) {
                    ServerPacket inPacket;
                    memcpy(&inPacket, recvBuf, sizeof(ServerPacket));

                    if (currentRoom == -1) {
                        currentRoom = inPacket.room_id;
                    }
                    if (inPacket.room_id != currentRoom) continue;

                    uint8_t prevTeam = plane.team;
                    bool prevAttacker = plane.is_attacker;
                    bool firstConnect = (latestServerData.your_id == -1);

                    latestServerData = inPacket;
                    connectedInRoom = inPacket.connected_count;

                    plane.team = (inPacket.your_id < 5) ? 0 : 1;
                    plane.is_attacker = (inPacket.current_round == 1) ? (plane.team == 1) : (plane.team == 0);

                    if (firstConnect && (plane.team != prevTeam || plane.is_attacker != prevAttacker)) {
                        plane.plane_class = selectedClass;
                        InitPlayerState(&plane);
                    }

                    if (cityMap.seed != inPacket.map_seed && inPacket.map_seed != 0) {
                        InitMap(&cityMap, inPacket.map_seed);
                    }

                    if (inPacket.current_round != localRound) {
                        localRound = inPacket.current_round;
                        memset(local_drone_destroyed, 0, sizeof(local_drone_destroyed));
                        memset(projectile, 0, sizeof(projectile));
                        if (localRound <= 2) {
                            InitMap(&cityMap, inPacket.map_seed);
                            SpawnDrones(drones, MAX_DRONE_COUNT);
                            plane.plane_class = selectedClass;
                            InitPlayerState(&plane);
                            plane.plane_respawns_left = 3;
                        }
                    }

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        remotePlayers[i].position    = inPacket.players[i].position;
                        remotePlayers[i].pitch       = inPacket.players[i].pitch;
                        remotePlayers[i].yaw         = inPacket.players[i].yaw;
                        remotePlayers[i].roll        = inPacket.players[i].roll;
                        remotePlayers[i].hp          = inPacket.players[i].hp;
                        remotePlayers[i].is_alive    = inPacket.players[i].is_alive;
                        remotePlayers[i].is_drone    = inPacket.players[i].is_drone;
                        remotePlayers[i].team        = inPacket.players[i].team;
                        remotePlayers[i].plane_class = inPacket.players[i].plane_class;
                    }

                    for (int i = 0; i < MAX_PROJECTILES; i++) {
                        remoteProjectiles[i].position = inPacket.projectiles[i].position;
                        remoteProjectiles[i].type     = (WeaponType)inPacket.projectiles[i].type;
                        remoteProjectiles[i].active   = inPacket.projectiles[i].active;
                    }

                    for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                        drones[d].pos = inPacket.drones[d].pos;
                        drones[d].is_alive = inPacket.drones[d].is_alive;
                    }

                    for (int b = 0; b < cityMap.building_count; b++) {
                        cityMap.buildings[b].hp = inPacket.building_hp[b];
                        cityMap.buildings[b].is_destroyed = (inPacket.building_hp[b] <= 0);
                    }

                    if (inPacket.damage_received > 0 && plane.is_alive) {
                        plane.hp -= inPacket.damage_received;
                        if (plane.hp <= 0) {
                            plane.hp = 0;
                            plane.is_alive = false;
                        }
                    }
                }
            }

            accumulator -= TICK_DT;
        }

        // --- 3. AFFICHAGE ---
        BeginDrawing();
            ClearBackground(SKYBLUE);

            BeginMode3D(camera);
                DrawMap(&cityMap);
                DrawDrones(drones, MAX_DRONE_COUNT);

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

            DrawFPS(10, 10);

            if (status == STATUS_PLAYING) {
                DrawCityTopBar(&cityMap, latestServerData.current_round, latestServerData.round_time_left, plane.is_attacker);

                const char *cName = (plane.plane_class == PLANE_INTERCEPTOR) ? "INTERCEPTEUR" :
                                    ((plane.plane_class == PLANE_BOMBER) ? "BOMBARDIER" : "CHASSEUR");

                if (latestServerData.your_id != -1) {
                    DrawText(TextFormat(" Pilote: %s | ROOM #%d (%d/10) | Equipe %d [%s]",
                             username, currentRoom + 1, connectedInRoom, plane.team + 1,
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
                MenuCommand cmd = DrawMenu(&status, currentRoom, connectedInRoom, &joinRoomId, username, accountLoaded, &selectedClass);

                if (cmd == MENU_CMD_LOGIN) {
                    accountLoaded = false;
                    LobbyClientPacket req = { .type = PACKET_LOBBY, .action = LOBBY_LOGIN, .room_id = (int8_t)currentRoom, .plane_class = selectedClass };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_SELECT_CLASS) {
                    plane.plane_class = selectedClass;
                    LobbyClientPacket req = { .type = PACKET_LOBBY, .action = LOBBY_SET_CLASS, .room_id = (int8_t)currentRoom, .plane_class = selectedClass };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_CREATE_ROOM) {
                    LobbyClientPacket req = { .type = PACKET_LOBBY, .action = LOBBY_CREATE, .room_id = -1, .plane_class = selectedClass };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_JOIN_ROOM && joinRoomId >= 0 && joinRoomId < MAX_ROOMS) {
                    LobbyClientPacket req = { .type = PACKET_LOBBY, .action = LOBBY_JOIN, .room_id = (int8_t)joinRoomId, .plane_class = selectedClass };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                else if (cmd == MENU_CMD_LEAVE_ROOM) {
                    LobbyClientPacket req = { .type = PACKET_LOBBY, .action = LOBBY_LEAVE, .room_id = (int8_t)currentRoom, .plane_class = selectedClass };
                    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
                    sendto(netSocket, &req, sizeof(req), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
                    currentRoom = -1;
                    connectedInRoom = 0;
                    latestServerData.your_id = -1;
                }
                else if (cmd == MENU_CMD_START_GAME) {
                    latestServerData.your_id = -1;
                    localRound = 1;
                    memset(local_drone_destroyed, 0, sizeof(local_drone_destroyed));
                    memset(projectile, 0, sizeof(projectile));
                    plane.plane_class = selectedClass;
                    plane.plane_respawns_left = 3;
                    InitPlayerState(&plane);
                    int seedRoom = (currentRoom != -1) ? currentRoom : 0;
                    InitMap(&cityMap, 133742u + (unsigned int)(seedRoom * 1000));
                    SpawnDrones(drones, MAX_DRONE_COUNT);
                }
            }
        EndDrawing();
    }

    if (IsCursorHidden()) EnableCursor();
    close(netSocket);
    CloseWindow();
    return 0;
}