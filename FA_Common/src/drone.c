#include "drone.h"
#include <math.h>

void InitDrone(Drone *drone) {
    drone->hp = 100;
    drone->ammo = 5;
    drone->max_ammo = 5;
    drone->reload_timer = 0.0f;
    drone->speed = (float)GetRandomValue(25, 45);
    drone->pos = (Vector3){
        (float)GetRandomValue(-450, 450),
        (float)GetRandomValue(40, 130),
        (float)GetRandomValue(-450, 450)
    };

    // Direction horizontale aléatoire pour que chaque drone patrouille
    float randomAngle = (float)GetRandomValue(0, 360) * DEG2RAD;
    drone->direction = (Vector3){ cosf(randomAngle), 0.0f, sinf(randomAngle) };
    drone->is_alive = true;
}

void SpawnDrones(Drone drones[], int max_drone_count) {
    for (int i = 0; i < max_drone_count; i++) {
        InitDrone(&drones[i]);
    }
}

void UpdateDrones(Drone drones[], int max_drone_count, float dt) {
    for (int i = 0; i < max_drone_count; i++) {
        if (!drones[i].is_alive) continue;

        // Déplacement du drone
        drones[i].pos.x += drones[i].direction.x * drones[i].speed * dt;
        drones[i].pos.z += drones[i].direction.z * drones[i].speed * dt;

        // Si le drone sort des limites de la ville (-500 à 500), il fait demi-tour
        if (drones[i].pos.x > 500.0f || drones[i].pos.x < -500.0f) {
            drones[i].direction.x = -drones[i].direction.x;
        }
        if (drones[i].pos.z > 500.0f || drones[i].pos.z < -500.0f) {
            drones[i].direction.z = -drones[i].direction.z;
        }
    }
}

void DrawDrones(const Drone drones[], int max_drone_count) {
    for (int i = 0; i < max_drone_count; i++) {
        if (!drones[i].is_alive) continue;

        // Corps du drone + contour pour bien le voir dans le ciel
        DrawCube(drones[i].pos, 5.0f, 2.0f, 5.0f, MAROON);
        DrawCubeWires(drones[i].pos, 5.0f, 2.0f, 5.0f, RED);
    }
}