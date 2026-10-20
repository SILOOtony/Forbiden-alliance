#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"
#include "map.h"

#define SERVER_PORT 8080
#define MAX_CLIENTS 10
#define MAX_PLAYER_PROJECTILE 15
#define MAX_PROJECTILES (MAX_CLIENTS * MAX_PLAYER_PROJECTILE)

// Structure ultra-légère pour un joueur sur le réseau (32 octets au lieu de ~160)
typedef struct {
    Vector3 position;
    float pitch;
    float yaw;
    float roll;
    int16_t hp;
    bool is_alive;
    bool is_drone;
    uint8_t team;
} NetPlayer;

// Structure ultra-légère pour un projectile sur le réseau (16 octets au lieu de 48)
typedef struct {
    Vector3 position;
    uint8_t type;
    bool active;
} NetProjectile;

// Paquet envoyé par le Client -> Serveur
typedef struct {
    NetPlayer player;
    NetProjectile projectiles[MAX_PLAYER_PROJECTILE];
    int16_t damage_dealt[MAX_CLIENTS];
    bool building_destroyed[MAX_BUILDINGS];
} ClientPacket;

// Paquet envoyé par le Serveur -> Client
typedef struct {
    int8_t your_id;
    int16_t damage_received;
    unsigned int map_seed;
    bool connected[MAX_CLIENTS];
    NetPlayer players[MAX_CLIENTS];
    NetProjectile projectiles[MAX_PROJECTILES];
    bool building_destroyed[MAX_BUILDINGS];
} ServerPacket;