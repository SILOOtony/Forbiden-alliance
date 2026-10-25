#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "fa_types.h"
#include "map.h"
#include "drone.h"

#define SERVER_PORT 8080
#define MAX_ROOMS 4
#define MAX_CLIENTS 10
#define MAX_PLAYER_PROJECTILE 15
#define MAX_PROJECTILES (MAX_CLIENTS * MAX_PLAYER_PROJECTILE)
#define ROUND_DURATION_SEC 180.0f
#define MAX_USERNAME_LEN 24

typedef enum {
    PACKET_LOBBY = 1,
    PACKET_GAME  = 2
} PacketType;

typedef enum {
    LOBBY_CREATE    = 1,
    LOBBY_JOIN      = 2,
    LOBBY_HEARTBEAT = 3,
    LOBBY_LEAVE     = 4,
    LOBBY_LOGIN     = 5, // Charge ou crée le fichier JSON du joueur sur le VPS
    LOBBY_SET_CLASS = 6  // Sauvegarde la nouvelle classe d'avion dans le JSON sur le VPS
} LobbyAction;

// Alignement mémoire strict au bit/octet près entre macOS (ARM64) et Linux (x86_64)
#pragma pack(push, 1)

typedef struct {
    Vector3 position;
    float pitch;
    float yaw;
    float roll;
    int16_t hp;
    bool is_alive;
    bool is_drone;
    uint8_t team;
    uint8_t plane_class;
} NetPlayer;

typedef struct {
    Vector3 position;
    uint8_t type;
    bool active;
} NetProjectile;

typedef struct {
    Vector3 pos;
    bool is_alive;
    Vector3 bullet_pos;
    bool bullet_active;
} NetDrone;

// --- PAQUETS DE ROOM & COMPTE JSON ---
typedef struct {
    uint8_t type;
    uint8_t action;
    int8_t room_id;
    uint8_t plane_class;
    char username[MAX_USERNAME_LEN];
} LobbyClientPacket;

typedef struct {
    uint8_t type;
    int8_t room_id;
    uint8_t connected_count;
    uint8_t plane_class;
    bool account_loaded;
    char username[MAX_USERNAME_LEN];
} LobbyServerPacket;

// --- PAQUETS DE PARTIE (5v5) ---
typedef struct {
    uint8_t type;
    int8_t room_id;
    NetPlayer player;
    NetProjectile projectiles[MAX_PLAYER_PROJECTILE];
    int16_t damage_dealt[MAX_CLIENTS];
    int16_t building_damage_dealt[MAX_BUILDINGS];
    bool drone_destroyed[MAX_DRONE_COUNT];
} ClientPacket;

typedef struct {
    uint8_t type;
    int8_t room_id;
    int8_t your_id;
    uint8_t connected_count;
    int16_t damage_received;
    unsigned int map_seed;

    uint8_t current_round;
    uint16_t round_time_left;
    uint16_t team_buildings_destroyed[2];
    uint32_t team_city_damage[2];

    bool connected[MAX_CLIENTS];
    NetPlayer players[MAX_CLIENTS];
    NetProjectile projectiles[MAX_PROJECTILES];
    int16_t building_hp[MAX_BUILDINGS];
    NetDrone drones[MAX_DRONE_COUNT];
} ServerPacket;

#pragma pack(pop)