#pragma once

#include "raylib.h"
#include <stdbool.h>
#include "player.h"

#define MAX_PROJECTILES 60
#define MAX_PLAYER_PROJECTILE 15

typedef struct {
    Vector3 position;
    Vector3 direction;
    Vector3 velocity;
    float speed;
    float lifetime;
    int damage;
    WeaponType type;
    bool active;
} Projectile;

void InitProjectile(Projectile *projectile, const Weapon *weapon, const PlayerState *state);
bool SpawnProjectile(PlayerState *state, Projectile projectiles[], int maxProjectiles);
void UpdateProjectiles(Projectile projectiles[], int maxProjectiles, float dt);
void DrawProjectiles(const Projectile projectiles[], int maxProjectiles);