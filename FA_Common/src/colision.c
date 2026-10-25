#include "colision.h"
#include "raylib.h"

void CheckProjectileMapCollisions(Projectile projectiles[], int maxProjectiles, GameMap *map, bool is_attacker, int16_t building_damage_dealt[]) {
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;

        for (int j = 0; j < map->building_count; j++) {
            if (map->buildings[j].is_destroyed) continue;

            BoundingBox buildingBox = { map->buildings[j].min, map->buildings[j].max };
            if (CheckCollisionBoxSphere(buildingBox, projectiles[i].position, 0.8f)) {
                projectiles[i].active = false;

                // Seuls les Attaquants peuvent endommager les bâtiments de la ville !
                if (is_attacker) {
                    int dmg = projectiles[i].damage;
                    if (projectiles[i].type == WEAPON_DRONE_BLASTER) {
                        dmg = (map->buildings[j].max_hp + 14) / 15; // 15 tirs de drone pour détruire un bâtiment
                    }
                    if (dmg > map->buildings[j].hp) dmg = map->buildings[j].hp;
                    map->buildings[j].hp -= dmg;
                    building_damage_dealt[j] += (int16_t)dmg;
                    if (map->buildings[j].hp <= 0) {
                        map->buildings[j].hp = 0;
                        map->buildings[j].is_destroyed = true;
                    }
                }
                break;
            }
        }
    }
}

void CheckProjectileDroneCollisions(Projectile projectiles[], int maxProjectiles, Drone drones[], int maxDrones, bool player_is_attacker) {
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;

        for (int d = 0; d < maxDrones; d++) {
            if (!drones[d].is_alive) continue;
            // Pas de tir ami sur les drones de son propre camp
            if (drones[d].is_attacker == player_is_attacker) continue;

            if (CheckCollisionSpheres(projectiles[i].position, 1.0f, drones[d].pos, 6.0f)) {
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

void CheckProjectilePlayersCollisions(Projectile projectiles[], int maxProjectiles, PlayerState players[], const bool connected[], int maxClients, int my_id, uint8_t my_team, int damage_dealt[]) {
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;

        for (int p = 0; p < maxClients; p++) {
            if (p == my_id || !connected[p] || !players[p].is_alive) continue;
            // Pas de tir ami entre joueurs de la même équipe
            if (players[p].team == my_team) continue;

            float targetRadius = players[p].is_drone ? 6.0f : 3.5f;
            if (CheckCollisionSpheres(projectiles[i].position, 1.0f, players[p].position, targetRadius)) {
                projectiles[i].active = false;
                damage_dealt[p] += projectiles[i].damage;
                break;
            }
        }
    }
}

bool CheckPlaneMapCollision(PlayerState *state, GameMap *map, int16_t building_damage_dealt[]) {
    if (!state->is_alive) return true;

    if (state->position.y <= 0.5f) {
        state->hp = 0;
        state->is_alive = false;
        return true;
    }

    float selfRadius = state->is_drone ? 3.0f : 1.5f;
    for (int j = 0; j < map->building_count; j++) {
        if (map->buildings[j].is_destroyed) continue;

        BoundingBox buildingBox = { map->buildings[j].min, map->buildings[j].max };
        if (CheckCollisionBoxSphere(buildingBox, state->position, selfRadius)) {
            state->hp = 0;
            state->is_alive = false;

            // Si c'est un Attaquant qui se crashe sur un immeuble, il lui inflige des dégâts
            if (state->is_attacker) {
                int dmg = 100;
                if (dmg > map->buildings[j].hp) dmg = map->buildings[j].hp;
                map->buildings[j].hp -= dmg;
                building_damage_dealt[j] += (int16_t)dmg;
                if (map->buildings[j].hp <= 0) {
                    map->buildings[j].hp = 0;
                    map->buildings[j].is_destroyed = true;
                }
            }
            return true;
        }
    }
    return false;
}