#pragma once
#include "fa_types.h"
#include <stdbool.h>
#include <stdint.h>
#include "map.h"

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

    // Projectile tiré par ce drone
    float shoot_timer;
    Vector3 bullet_pos;
    Vector3 bullet_dir;
    float bullet_speed;
    float bullet_lifetime;
    bool bullet_active;
} Drone;

void InitDrone(Drone *drone, bool is_attacker);
void SpawnDrones(Drone drones[], int max_drone_count);
void UpdateDrones(
    Drone drones[],
    int max_drone_count,
    GameMap *map,
    uint16_t *out_buildings_destroyed,
    uint32_t *out_city_damage,
    float dt
);
void DrawDrones(const Drone drones[], int max_drone_count);

#ifndef SERVER_BUILD
Model LoadSafeDroneModel(const char *objPath, bool is_attacker);
#endif