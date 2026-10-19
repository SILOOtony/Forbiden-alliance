#pragma once
#include "raylib.h"
#include "player.h"
#include "map.h"
#include <stdbool.h>
#include "weapons.h"

#define SERVER_PORT 8080
#define MAX_CLIENTS 4

// Paquet envoyé par le Client -> au Serveur (à chaque Tick)
typedef struct {
    Vector3 position;
    float pitch;
    float yaw;
    float roll;
    int hp;
    bool is_alive;
    bool building_destroyed[MAX_BUILDINGS]; // Informe le serveur des bâtiments qu'on a détruits
    Projectile projectiles[MAX_PLAYER_PROJECTILE];
} ClientPacket;

// Paquet envoyé par le Serveur -> à tous les Clients (à chaque Tick)
typedef struct {
    int your_id;
    unsigned int map_seed;// 4 octets : permet de générer la même ville !
    bool connected[MAX_CLIENTS];
    bool building_destroyed[MAX_BUILDINGS]; // 200 octets : synchronise les immeubles détruits
    PlayerState players[MAX_CLIENTS];
    Projectile projectiles[MAX_PROJECTILES];
    
} ServerPacket;