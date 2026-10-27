#include "weapons.h"
#include <math.h>

void InitProjectile(Projectile *projectile, const Weapon *weapon, const PlayerState *state) {
    projectile->type = weapon->type;

    if (weapon->type == WEAPON_FLARES) {
        // Les leurres sont éjectés vers l'arrière et légèrement vers le bas
        projectile->direction = (Vector3){
             cosf(state->pitch) * sinf(state->yaw),
            -sinf(state->pitch) - 0.35f,
             cosf(state->pitch) * cosf(state->yaw)
        };
        projectile->lifetime = 1.8f;
    } else {
        projectile->direction = (Vector3){
            -cosf(state->pitch) * sinf(state->yaw),
             sinf(state->pitch),
            -cosf(state->pitch) * cosf(state->yaw)
        };
        projectile->lifetime = 3.0f;
    }

    projectile->position = state->position;
    projectile->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    projectile->speed = weapon->projectile_speed;
    projectile->damage = weapon->damage;
    projectile->active = true;
}

bool SpawnProjectile(PlayerState *state, Projectile projectiles[], int maxProjectiles) {
    Weapon *currentWeapon = &state->weapons[state->current_slot];
    if (currentWeapon->ammo <= 0) {
        return false;
    }

    int chosenSlot = -1;
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) {
            chosenSlot = i;
            break;
        }
    }

    if (chosenSlot == -1) {
        chosenSlot = 0;
        float minLife = projectiles[0].lifetime;
        for (int i = 1; i < maxProjectiles; i++) {
            if (projectiles[i].lifetime < minLife) {
                minLife = projectiles[i].lifetime;
                chosenSlot = i;
            }
        }
    }

    InitProjectile(&projectiles[chosenSlot], currentWeapon, state);
    currentWeapon->ammo--;
    return true;
}

void UpdateProjectiles(Projectile projectiles[], int maxProjectiles, float dt) {
    for (int i = 0; i < maxProjectiles; i++) {
        if (!projectiles[i].active) continue;

        projectiles[i].lifetime -= dt;
        if (projectiles[i].lifetime <= 0.0f || projectiles[i].position.y <= 0.0f) {
            projectiles[i].active = false;
            continue;
        }

        projectiles[i].velocity.x = projectiles[i].direction.x * projectiles[i].speed;
        projectiles[i].velocity.y = projectiles[i].direction.y * projectiles[i].speed;
        projectiles[i].velocity.z = projectiles[i].direction.z * projectiles[i].speed;

        projectiles[i].position.x += projectiles[i].velocity.x * dt;
        projectiles[i].position.y += projectiles[i].velocity.y * dt;
        projectiles[i].position.z += projectiles[i].velocity.z * dt;
    }
}

void DrawProjectiles(const Projectile projectiles[], int maxProjectiles) {
    for (int i = 0; i < maxProjectiles; i++) {
        if (projectiles[i].active) {
            if (projectiles[i].type == WEAPON_ROCKET) {
                DrawSphere(projectiles[i].position, 0.8f, ORANGE);
            }
            else if (projectiles[i].type == WEAPON_AIR_TO_AIR) {
                DrawSphere(projectiles[i].position, 0.65f, SKYBLUE);
            }
            else if (projectiles[i].type == WEAPON_AIR_TO_GROUND) {
                DrawSphere(projectiles[i].position, 1.4f, DARKGRAY);
            }
            else if (projectiles[i].type == WEAPON_DRONE_BLASTER) {
                DrawSphere(projectiles[i].position, 0.4f, YELLOW);
            }
            else if (projectiles[i].type == WEAPON_FLARES) {
                DrawSphere(projectiles[i].position, 1.15f, GOLD);
                DrawSphere(projectiles[i].position, 0.65f, WHITE);
            }
        }
    }
}