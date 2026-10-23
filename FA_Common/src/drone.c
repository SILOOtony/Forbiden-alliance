#include "drone.h"
#include <math.h>

static unsigned int drone_rng_state = 98765;

static int Drone_GetRandomValue(int min, int max) {
    if (min > max) { int tmp = max; max = min; min = tmp; }
    drone_rng_state = drone_rng_state * 1103515245u + 12345u;
    unsigned int rand_val = (drone_rng_state / 65536u) % 32768u;
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

void InitDrone(Drone *drone, bool is_attacker) {
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
    drone->is_attacker = is_attacker;
}

void SpawnDrones(Drone drones[], int max_drone_count) {
    drone_rng_state = 98765;
    for (int i = 0; i < max_drone_count; i++) {
        // Les 40 premiers sont Défenseurs, les 20 suivants sont Attaquants
        bool is_attacker = (i >= DEFENDER_DRONE_COUNT);
        InitDrone(&drones[i], is_attacker);
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
        Color bodyColor = drones[i].is_attacker ? MAROON : DARKBLUE;
        Color wireColor = drones[i].is_attacker ? RED : SKYBLUE;
        DrawCube(drones[i].pos, 10.0f, 4.0f, 10.0f, bodyColor);
        DrawCubeWires(drones[i].pos, 10.0f, 4.0f, 10.0f, wireColor);
    }
}
#endif