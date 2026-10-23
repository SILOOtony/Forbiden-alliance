#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "map.h"
#include "weapons.h"
#include "drone.h"
#include "player.h"

void CheckProjectileMapCollisions(Projectile projectiles[], int maxProjectiles, GameMap *map, bool is_attacker, int16_t building_damage_dealt[]);
void CheckProjectileDroneCollisions(Projectile projectiles[], int maxProjectiles, Drone drones[], int maxDrones, bool player_is_attacker);
void CheckProjectilePlayersCollisions(Projectile projectiles[], int maxProjectiles, PlayerState players[], const bool connected[], int maxClients, int my_id, uint8_t my_team, int damage_dealt[]);
bool CheckPlaneMapCollision(PlayerState *state, GameMap *map, int16_t building_damage_dealt[]);