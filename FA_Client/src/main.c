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

    PlayerState remotePlayers[MAX_CLIENTS] = { 0 };
    Projectile remoteProjectiles[MAX_PROJECTILES] = { 0 };
    bool local_drone_destroyed[MAX_DRONE_COUNT] = { 0 };

    KeyBindings controls = InitKeyBinds();
    GameMap cityMap = { 0 };
    InitMap(&cityMap, 133742);

    PlayerState plane;
    InitPlayerState(&plane);

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

        if (status == STATUS_PLAYING && plane.is_alive) {
            if (!IsCursorHidden()) DisableCursor();
        } else {
            if (IsCursorHidden()) EnableCursor();
        }

        // --- 1. ENTRÉES INSTANTANÉES ---
        if (status == STATUS_PLAYING) {
            if (IsKeyPressed(KEY_P)) status = STATUS_PAUSED;

            if (plane.is_alive) {
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
            } else {
                // Réapparition en avion [R]
                if (IsKeyPressed(KEY_R)) {
                    InitPlayerState(&plane);
                }
                // Réincarnation dans un drone [E]
                if (IsKeyPressed(KEY_E)) {
                    for (int d = 0; d < MAX_DRONE_COUNT; d++) {
                        if (drones[d].is_alive) {
                            drones[d].is_alive = false;
                            local_drone_destroyed[d] = true;
                            ReincarnateAsDrone(&plane, drones[d].pos);
                            break;
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
            } 
            else if (status == STATUS_PLAYING) {
                if (plane.is_alive) {
                    UpdatePlayerMovement(&plane, &controls, TICK_DT);
                    UpdatePlaneCamera(&camera, &plane);
                    UpdateWeaponsReload(plane.weapons, TICK_DT);
                    CheckPlaneMapCollision(&plane, &cityMap);
                }

                if (latestServerData.your_id == -1) {
                    UpdateDrones(drones, MAX_DRONE_COUNT, TICK_DT);
                }

                UpdateProjectiles(projectile, MAX_PLAYER_PROJECTILE, TICK_DT);
                CheckProjectileMapCollisions(projectile, MAX_PLAYER_PROJECTILE, &cityMap);
                CheckProjectileDroneCollisions(projectile, MAX_PLAYER_PROJECTILE, drones, MAX_DRONE_COUNT);

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
                    damage_dealt
                );

                // --- COMPRESSION ET ENVOI DU SLIM PACKET AU SERVEUR ---
                ClientPacket outPacket = { 0 };
                outPacket.player = (NetPlayer){
                    .position = plane.position,
                    .pitch = plane.pitch,
                    .yaw = plane.yaw,
                    .roll = plane.roll,
                    .hp = (int16_t)plane.hp,
                    .is_alive = plane.is_alive,
                    .is_drone = plane.is_drone,
                    .team = 0
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

                for (int b = 0; b < cityMap.building_count; b++) {
                    outPacket.building_destroyed[b] = cityMap.buildings[b].is_destroyed;
                }

                memcpy(outPacket.drone_destroyed, local_drone_destroyed, sizeof(local_drone_destroyed));

                sendto(netSocket, &outPacket, sizeof(outPacket), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
            }

            // --- RÉCEPTION ET DÉCOMPRESSION DU SLIM PACKET SERVEUR ---
            ServerPacket inPacket;
            while (recvfrom(netSocket, &inPacket, sizeof(inPacket), 0, NULL, NULL) == sizeof(inPacket)) {
                latestServerData = inPacket;

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    remotePlayers[i].position = inPacket.players[i].position;
                    remotePlayers[i].pitch    = inPacket.players[i].pitch;
                    remotePlayers[i].yaw      = inPacket.players[i].yaw;
                    remotePlayers[i].roll     = inPacket.players[i].roll;
                    remotePlayers[i].hp       = inPacket.players[i].hp;
                    remotePlayers[i].is_alive = inPacket.players[i].is_alive;
                    remotePlayers[i].is_drone = inPacket.players[i].is_drone;
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

                if (inPacket.damage_received > 0 && plane.is_alive) {
                    plane.hp -= inPacket.damage_received;
                    if (plane.hp <= 0) {
                        plane.hp = 0;
                        plane.is_alive = false;
                    }
                }

                if (cityMap.seed != latestServerData.map_seed && latestServerData.map_seed != 0) {
                    InitMap(&cityMap, latestServerData.map_seed);
                }

                for (int b = 0; b < cityMap.building_count; b++) {
                    if (latestServerData.building_destroyed[b]) {
                        cityMap.buildings[b].is_destroyed = true;
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

                if (status != STATUS_MENU) {
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
                DrawText("Souris: Voler | W/S: Vitesse | Clic Gauche: Tirer | Molette/1/2/3: Armes | P: Pause", 10, 40, 20, DARKBLUE);
                if (latestServerData.your_id != -1) {
                    int myTeam = (latestServerData.your_id < 5) ? 1 : 2;
                    DrawText(TextFormat("EN LIGNE - Joueur #%d (Equipe %d) [%s]", latestServerData.your_id, myTeam, plane.is_drone ? "DRONE" : "AVION"), 10, 70, 20, DARKGREEN);
                } else {
                    DrawText(TextFormat("HORS LIGNE [%s]", plane.is_drone ? "DRONE" : "AVION"), 10, 70, 20, RED);
                }

                DrawWeaponSlots(&plane);
                DrawHpBar(&plane);

                if (!plane.is_alive) {
                    DrawDeathScreen();
                }
            } else {
                DrawMenu(&status);
            }
        EndDrawing();
    }

    if (IsCursorHidden()) EnableCursor();
    close(netSocket);
    CloseWindow();
    return 0;
}