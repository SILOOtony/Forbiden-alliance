#pragma once
#include "raylib.h"
#include <stdbool.h>

#define MAX_DRONE_COUNT 60
#define DEFENDER_DRONE_COUNT 40 // 0 à 39 = Défenseurs, 40 à 59 = Attaquants

typedef struct {
    int hp;
    int ammo;
    int max_ammo;
    float reload_timer;
    float speed;
    Vector3 direction;
    Vector3 pos;
    bool is_alive;
    bool is_attacker; // false = Drone Défenseur (x40), true = Drone Attaquant (x20)
} Drone;

void InitDrone(Drone *drone, bool is_attacker);
void SpawnDrones(Drone drones[], int max_drone_count);
void UpdateDrones(Drone drones[], int max_drone_count, float dt);
void DrawDrones(const Drone drones[], int max_drone_count);