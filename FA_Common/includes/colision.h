#pragma once

#include "map.h"
#include "player.h"
#include "weapons.h"
#include "drone.h"

void CheckProjectileMapCollisions(Projectile projectiles[], int maxProjectiles, GameMap *map);
void CheckProjectileDroneCollisions(Projectile projectiles[], int maxProjectiles, Drone drones[], int maxDrones);
bool CheckPlaneMapCollision(PlayerState *state, GameMap *map);