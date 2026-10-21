#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"
#include "map.h"
#include "drone.h"

#define SERVER_PORT 8080
#define MAX_ROOMS 4
#define MAX_CLIENTS 10
#define MAX_PLAYER_PROJECTILE 15
#define MAX_PROJECTILES (MAX_CLIENTS * MAX_PLAYER_PROJECTILE)

typedef enum {
    PACKET_LOBBY = 1,
    PACKET_GAME  = 2
} PacketType;

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

typedef struct {
    Vector3 position;
    uint8_t type;
    bool active;
} NetProjectile;

typedef struct {
    Vector3 pos;
    bool is_alive;
} NetDrone;

// --- PAQUETS DE LOBBY (Ultra-légers pour le menu des salons) ---
typedef struct {
    uint8_t type;   // Toujours PACKET_LOBBY
    int8_t action;  // 0 = Rafraîchir la liste des salons, 1 = Quitter le salon actuel
    int8_t room_id;
} LobbyClientPacket;

typedef struct {
    uint8_t type;   // Toujours PACKET_LOBBY
    uint8_t room_players[MAX_ROOMS]; // Nombre de joueurs connectés par salon (0 à 10)
} LobbyServerPacket;

// --- PAQUETS DE PARTIE (Envoyés uniquement en jeu) ---
typedef struct {
    uint8_t type;   // Toujours PACKET_GAME
    int8_t room_id; // Salon dans lequel joue le client (0 à MAX_ROOMS - 1)
    NetPlayer player;
    NetProjectile projectiles[MAX_PLAYER_PROJECTILE];
    int16_t damage_dealt[MAX_CLIENTS];
    bool building_destroyed[MAX_BUILDINGS];
    bool drone_destroyed[MAX_DRONE_COUNT];
} ClientPacket;

typedef struct {
    uint8_t type;   // Toujours PACKET_GAME
    int8_t room_id;
    int8_t your_id;
    int16_t damage_received;
    unsigned int map_seed;
    bool connected[MAX_CLIENTS];
    NetPlayer players[MAX_CLIENTS];
    NetProjectile projectiles[MAX_PROJECTILES];
    bool building_destroyed[MAX_BUILDINGS];
    NetDrone drones[MAX_DRONE_COUNT];
} ServerPacket;