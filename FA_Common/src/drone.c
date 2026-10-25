#include "drone.h"
#include <math.h>
#include <stddef.h>

static unsigned int drone_rng_state = 98765;

static int Drone_GetRandomValue(int min, int max) {
    if (min > max) { int tmp = max; max = min; min = tmp; }
    drone_rng_state = drone_rng_state * 1103515245u + 12345u;
    unsigned int rand_val = (drone_rng_state / 65536u) % 32768u;
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

// Collisions en pur C (compatibles client + serveur sans Raylib)
static bool Drone_CheckSpheres(Vector3 a, float ra, Vector3 b, float rb) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    float r = ra + rb;
    return (dx * dx + dy * dy + dz * dz) <= (r * r);
}

static bool Drone_CheckBoxSphere(Vector3 bmin, Vector3 bmax, Vector3 center, float radius) {
    float cx = (center.x < bmin.x) ? bmin.x : ((center.x > bmax.x) ? bmax.x : center.x);
    float cy = (center.y < bmin.y) ? bmin.y : ((center.y > bmax.y) ? bmax.y : center.y);
    float cz = (center.z < bmin.z) ? bmin.z : ((center.z > bmax.z) ? bmax.z : center.z);
    float dx = center.x - cx;
    float dy = center.y - cy;
    float dz = center.z - cz;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
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

    drone->shoot_timer = (float)Drone_GetRandomValue(15, 50) / 10.0f;
    drone->bullet_pos = drone->pos;
    drone->bullet_dir = (Vector3){ 0.0f, -1.0f, 0.0f };
    drone->bullet_speed = 240.0f;
    drone->bullet_lifetime = 0.0f;
    drone->bullet_active = false;
}

void SpawnDrones(Drone drones[], int max_drone_count) {
    drone_rng_state = 98765;
    for (int i = 0; i < max_drone_count; i++) {
        // Les 40 premiers sont Défenseurs, les 20 suivants sont Attaquants
        bool is_attacker = (i >= DEFENDER_DRONE_COUNT);
        InitDrone(&drones[i], is_attacker);
    }
}

void UpdateDrones(
    Drone drones[],
    int max_drone_count,
    GameMap *map,
    uint16_t *out_buildings_destroyed,
    uint32_t *out_city_damage,
    float dt
) {
    for (int i = 0; i < max_drone_count; i++) {
        Drone *d = &drones[i];

        // 1. Déplacement et tir aléatoire si le drone est en vie
        if (d->is_alive) {
            d->pos.x += d->direction.x * d->speed * dt;
            d->pos.z += d->direction.z * d->speed * dt;

            if (d->pos.x > 500.0f || d->pos.x < -500.0f) d->direction.x = -d->direction.x;
            if (d->pos.z > 500.0f || d->pos.z < -500.0f) d->direction.z = -d->direction.z;

            d->shoot_timer -= dt;
            if (d->shoot_timer <= 0.0f && !d->bullet_active) {
                d->shoot_timer = (float)Drone_GetRandomValue(20, 55) / 10.0f;

                Vector3 targetPos = { 0 };
                bool foundTarget = false;

                if (d->is_attacker && map != NULL && map->building_count > 0) {
                    // Drone Attaquant : choisit un bâtiment intact au hasard
                    for (int tries = 0; tries < 15; tries++) {
                        int bIdx = Drone_GetRandomValue(0, map->building_count - 1);
                        if (!map->buildings[bIdx].is_destroyed && map->buildings[bIdx].hp > 0) {
                            targetPos = map->buildings[bIdx].position;
                            foundTarget = true;
                            break;
                        }
                    }
                } else if (!d->is_attacker) {
                    // Drone Défenseur : choisit un drone Attaquant vivant au hasard
                    for (int tries = 0; tries < 15; tries++) {
                        int tIdx = Drone_GetRandomValue(DEFENDER_DRONE_COUNT, max_drone_count - 1);
                        if (drones[tIdx].is_alive) {
                            targetPos = drones[tIdx].pos;
                            foundTarget = true;
                            break;
                        }
                    }
                }

                if (foundTarget) {
                    // Dispersion aléatoire pour que les tirs se fassent "au hasard" vers la cible
                    targetPos.x += (float)Drone_GetRandomValue(-20, 20);
                    targetPos.y += (float)Drone_GetRandomValue(-10, 10);
                    targetPos.z += (float)Drone_GetRandomValue(-20, 20);

                    float dx = targetPos.x - d->pos.x;
                    float dy = targetPos.y - d->pos.y;
                    float dz = targetPos.z - d->pos.z;
                    float len = sqrtf(dx * dx + dy * dy + dz * dz);

                    if (len > 0.001f) {
                        d->bullet_dir = (Vector3){ dx / len, dy / len, dz / len };
                        d->bullet_pos = d->pos;
                        d->bullet_speed = 250.0f;
                        d->bullet_lifetime = 3.5f;
                        d->bullet_active = true;
                    }
                }
            }
        }

        // 2. Mise à jour et collision de la petite balle du drone
        if (d->bullet_active) {
            d->bullet_lifetime -= dt;
            d->bullet_pos.x += d->bullet_dir.x * d->bullet_speed * dt;
            d->bullet_pos.y += d->bullet_dir.y * d->bullet_speed * dt;
            d->bullet_pos.z += d->bullet_dir.z * d->bullet_speed * dt;

            if (d->bullet_lifetime <= 0.0f || d->bullet_pos.y <= 0.0f) {
                d->bullet_active = false;
                continue;
            }

            if (d->is_attacker && map != NULL) {
                // Balle d'un drone Attaquant -> touche les bâtiments (15 tirs = détruit)
                for (int b = 0; b < map->building_count; b++) {
                    Building *bld = &map->buildings[b];
                    if (bld->is_destroyed || bld->hp <= 0) continue;

                    if (Drone_CheckBoxSphere(bld->min, bld->max, d->bullet_pos, 0.8f)) {
                        d->bullet_active = false;

                        int dmg = (bld->max_hp + 14) / 15; // Exactement 1/15ème des PV max
                        if (dmg < 1) dmg = 1;
                        if (dmg > bld->hp) dmg = bld->hp;

                        bld->hp -= dmg;
                        if (out_city_damage != NULL) {
                            *out_city_damage += (uint32_t)dmg;
                        }

                        if (bld->hp <= 0) {
                            bld->hp = 0;
                            bld->is_destroyed = true;
                            if (out_buildings_destroyed != NULL) {
                                (*out_buildings_destroyed)++;
                            }
                        }
                        break;
                    }
                }
            } else if (!d->is_attacker) {
                // Balle d'un drone Défenseur -> touche les drones Attaquants (2 tirs = 100 PV = détruit)
                for (int t = DEFENDER_DRONE_COUNT; t < max_drone_count; t++) {
                    if (!drones[t].is_alive) continue;

                    if (Drone_CheckSpheres(d->bullet_pos, 1.0f, drones[t].pos, 6.0f)) {
                        d->bullet_active = false;
                        drones[t].hp -= 10; // 10 tirs (10 x 10 = 100) pour abattre un drone
                        if (drones[t].hp <= 0) {
                            drones[t].hp = 0;
                            drones[t].is_alive = false;
                            drones[t].bullet_active = false;
                        }
                        break;
                    }
                }
            }
        }
    }
}

#ifndef SERVER_BUILD
void DrawDrones(const Drone drones[], int max_drone_count) {
    for (int i = 0; i < max_drone_count; i++) {
        if (drones[i].is_alive) {
            Color bodyColor = drones[i].is_attacker ? MAROON : DARKBLUE;
            Color wireColor = drones[i].is_attacker ? RED : SKYBLUE;
            DrawCube(drones[i].pos, 10.0f, 4.0f, 10.0f, bodyColor);
            DrawCubeWires(drones[i].pos, 10.0f, 4.0f, 10.0f, wireColor);
        }

        // Dessin des petites balles tirées par les drones
        if (drones[i].bullet_active) {
            Color bulletColor = drones[i].is_attacker ? ORANGE : YELLOW;
            DrawSphere(drones[i].bullet_pos, 0.55f, bulletColor);
        }
    }
}
#endif