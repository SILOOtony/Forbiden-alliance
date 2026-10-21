#include "drone.h"
#include <math.h>

// Générateur aléatoire pur C pour que le serveur compile sans Raylib
static unsigned int drone_rng_state = 98765;

static int Drone_GetRandomValue(int min, int max) {
    if (min > max) { int tmp = max; max = min; min = tmp; }
    drone_rng_state = drone_rng_state * 1103515245u + 12345u;
    unsigned int rand_val = (drone_rng_state / 65536u) % 32768u;
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

void InitDrone(Drone *drone) {
    drone->hp = 100;
    drone->ammo = 30;
    drone->max_ammo = 30;
    drone->reload_timer = 0.0f;
    drone->speed = (float)Drone_GetRandomValue(25, 45);
    drone->pos = (Vector3){
        (float)Drone_GetRandomValue(-450, 450),
        (float)Drone_GetRandomValue(40, 130),
        (float)Drone_GetRandomValue(-450, 450)
    };

    float randomAngle = (float)Drone_GetRandomValue(0, 360) * DEG2RAD;
    drone->direction = (Vector3){ cosf(randomAngle), 0.0f, sinf(randomAngle) };
    drone->is_alive = true;
}

void SpawnDrones(Drone drones[], int max_drone_count) {
    drone_rng_state = 98765;
    for (int i = 0; i < max_drone_count; i++) {
        InitDrone(&drones[i]);
    }
}

void UpdateDrones(Drone drones[], int max_drone_count, float dt) {
    for (int i = 0; i < max_drone_count; i++) {
        if (!drones[i].is_alive) continue;

        drones[i].pos.x += drones[i].direction.x * drones[i].speed * dt;
        drones[i].pos.z += drones[i].direction.z * drones[i].speed * dt;

        if (drones[i].pos.x > 500.0f || drones[i].pos.x < -500.0f) {
            drones[i].direction.x = -drones[i].direction.x;
        }
        if (drones[i].pos.z > 500.0f || drones[i].pos.z < -500.0f) {
            drones[i].direction.z = -drones[i].direction.z;
        }
    }
}

#ifndef SERVER_BUILD
void DrawDrones(const Drone drones[], int max_drone_count) {
    for (int i = 0; i < max_drone_count; i++) {
        if (!drones[i].is_alive) continue;
        // Taille doublée : 10.0f x 4.0f x 10.0f (au lieu de 5.0f x 2.0f x 5.0f)
        DrawCube(drones[i].pos, 10.0f, 4.0f, 10.0f, MAROON);
        DrawCubeWires(drones[i].pos, 10.0f, 4.0f, 10.0f, RED);
    }
}
#endif