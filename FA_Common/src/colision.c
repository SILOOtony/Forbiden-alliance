#include "colision.h"
#include "raylib.h"

void CheckProjectileMapCollisions(Projectile projectiles[], int maxProjectiles, GameMap *map) {
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;

        for (int j = 0; j < map->building_count; j++) { 
            if (map->buildings[j].is_destroyed) continue;

            BoundingBox buildingBox = { map->buildings[j].min, map->buildings[j].max };

            if (CheckCollisionBoxSphere(buildingBox, projectiles[i].position, 0.8f)) {
                projectiles[i].active = false;

                map->buildings[j].hp -= projectiles[i].damage;
                if (map->buildings[j].hp <= 0) {
                    map->buildings[j].is_destroyed = true;
                }
                break;
            }
        }
    }
}

void CheckProjectileDroneCollisions(Projectile projectiles[], int maxProjectiles, Drone drones[], int maxDrones) {
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;

        for (int d = 0; d < maxDrones; d++) {
            if (!drones[d].is_alive) continue;

            // Collision sphère-sphère entre le projectile et le drone (rayon 3.0f)
            if (CheckCollisionSpheres(projectiles[i].position, 1.0f, drones[d].pos, 3.0f)) {
                projectiles[i].active = false;
                drones[d].hp -= projectiles[i].damage;

                if (drones[d].hp <= 0) {
                    drones[d].is_alive = false;
                }
                break;
            }
        }
    }
}

bool CheckPlaneMapCollision(PlayerState *state, GameMap *map) {
    if (!state->is_alive) return true;

    if (state->position.y <= 0.5f) {
        state->hp = 0;
        state->is_alive = false;
        return true;
    }

    for (int j = 0; j < map->building_count; j++) { 
        if (map->buildings[j].is_destroyed) continue;

        BoundingBox buildingBox = { map->buildings[j].min, map->buildings[j].max };

        if (CheckCollisionBoxSphere(buildingBox, state->position, 1.5f)) {
            state->hp = 0;
            state->is_alive = false;

            map->buildings[j].hp -= 100;
            if (map->buildings[j].hp <= 0) {
                map->buildings[j].is_destroyed = true;
            }
            return true;
        }
    }

    return false;
}