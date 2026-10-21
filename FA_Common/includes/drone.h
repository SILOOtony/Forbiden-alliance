#pragma once
#include "raylib.h"
#include <stdbool.h>

#define MAX_DRONE_COUNT 60

typedef struct {
    int hp;
    int ammo;
    int max_ammo;
    float reload_timer;
    float speed;
    Vector3 direction;
    Vector3 pos;
    bool is_alive;
} Drone;

void InitDrone(Drone *drone);
void SpawnDrones(Drone drones[], int max_drone_count);
void UpdateDrones(Drone drones[], int max_drone_count, float dt);
void DrawDrones(const Drone drones[], int max_drone_count);