#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "raylib.h"
#include "string.h"
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
    InitWindow(1280, 720, "Forbidden Alliance - Vol 3D (Multiplayer)");
    SetTargetFPS(60);

    // --- INITIALISATION RÉSEAU (UDP Non-Bloquant) ---
    int netSocket = socket(AF_INET, SOCK_DGRAM, 0);
    int flags = fcntl(netSocket, F_GETFL, 0);
    fcntl(netSocket, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    ServerPacket latestServerData = { 0 };
    latestServerData.your_id = -1;

    // --- INITIALISATION DU JEU ---
    KeyBindings controls = InitKeyBinds();
    GameMap cityMap = { 0 };
    InitMap(&cityMap, 133742); // Génère la ville dès le menu

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

        // --- 1. ENTRÉES INSTANTANÉES ---
        if (status == STATUS_PLAYING) {
            if (IsKeyPressed(KEY_P)) status = STATUS_PAUSED;

            if (plane.is_alive) {
                if (IsKeyPressed(controls.bindings[ACTION_SLOT_1])) plane.current_slot = 0;
                if (IsKeyPressed(controls.bindings[ACTION_SLOT_2])) plane.current_slot = 1;
                if (IsKeyPressed(controls.bindings[ACTION_SLOT_3])) plane.current_slot = 2;

                if (IsKeyPressed(controls.bindings[ACTION_ATTACK])) {
                    SpawnProjectile(&plane, projectile, MAX_PLAYER_PROJECTILE);
                }
            } else {
                if (IsKeyPressed(KEY_R)) {
                    InitPlayerState(&plane);
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

                UpdateDrones(drones, MAX_DRONE_COUNT, TICK_DT);
                UpdateProjectiles(projectile, MAX_PLAYER_PROJECTILE, TICK_DT);
                CheckProjectileMapCollisions(projectile, MAX_PLAYER_PROJECTILE, &cityMap);
                CheckProjectileDroneCollisions(projectile, MAX_PLAYER_PROJECTILE, drones, MAX_DRONE_COUNT);

                // --- ENVOI DE NOTRE POSITION ET DES BÂTIMENTS DÉTRUITS AU SERVEUR ---
                ClientPacket outPacket = {
                    .position = plane.position,
                    .pitch = plane.pitch,
                    .yaw = plane.yaw,
                    .roll = plane.roll,
                    .hp = plane.hp,
                    .is_alive = plane.is_alive
                };
                memcpy(outPacket.projectiles, projectile, sizeof(outPacket.projectiles));

                for (int b = 0; b < cityMap.building_count; b++) {
                    outPacket.building_destroyed[b] = cityMap.buildings[b].is_destroyed;
                }
                sendto(netSocket, &outPacket, sizeof(outPacket), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
            }

            // --- RÉCEPTION DES DONNÉES DU SERVEUR ---
            ServerPacket inPacket;
            while (recvfrom(netSocket, &inPacket, sizeof(inPacket), 0, NULL, NULL) == sizeof(inPacket)) {
                latestServerData = inPacket;

                // Si le serveur utilise une graine différente, on régénère la même carte que lui
                if (cityMap.seed != latestServerData.map_seed && latestServerData.map_seed != 0) {
                    InitMap(&cityMap, latestServerData.map_seed);
                }

                // On met à jour les bâtiments détruits par les autres joueurs !
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
                        if (latestServerData.connected[i] && latestServerData.players[i].is_alive) {
                            DrawPlanePlayer(&latestServerData.players[i]);
                            Vector3 markerPos = latestServerData.players[i].position;
                            markerPos.y += 4.0f;
                            DrawSphere(markerPos, 1.2f, LIME);
                        }
                    }

                    DrawProjectiles(latestServerData.projectiles, MAX_PROJECTILES);
                }
            EndMode3D();

            DrawFPS(10, 10);

            if (status == STATUS_PLAYING) {
                DrawText("W/S: Pitch | A/D: Virer | 1/2/3: Armes | Shift: Tirer | P: Pause", 10, 40, 20, DARKBLUE);
                if (latestServerData.your_id != -1) {
                    DrawText(TextFormat("EN LIGNE - Joueur #%d", latestServerData.your_id), 10, 70, 20, DARKGREEN);
                } else {
                    DrawText("HORS LIGNE (Lancez ./server)", 10, 70, 20, RED);
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

    close(netSocket);
    CloseWindow();
    return 0;
}