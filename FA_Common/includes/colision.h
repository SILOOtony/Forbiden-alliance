#pragma once
#include "map.h"
#include "player.h"
#include "weapons.h"
#include "drone.h"

void CheckProjectileMapCollisions(Projectile projectiles[], int maxProjectiles, GameMap *map);
void CheckProjectileDroneCollisions(Projectile projectiles[], int maxProjectiles, Drone drones[], int maxDrones);
void CheckProjectilePlayersCollisions(Projectile projectiles[], int maxProjectiles, PlayerState players[], const bool connected[], int maxClients, int my_id, int damage_dealt[]);
bool CheckPlaneMapCollision(PlayerState *state, GameMap *map);