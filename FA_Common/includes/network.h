#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
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
    LOBBY_LOGIN     = 5,
    LOBBY_SET_CLASS = 6
} LobbyAction;

typedef enum {
    RESPAWN_NONE  = 0,
    RESPAWN_PLANE = 1,
    RESPAWN_DRONE = 2
} RespawnCommand;

static inline int16_t QuantizeCoord(float v) {
    if (v > 3200.0f) v = 3200.0f;
    if (v < -3200.0f) v = -3200.0f;
    return (int16_t)(v * 10.0f);
}

static inline float DequantizeCoord(int16_t q) {
    return (float)q / 10.0f;
}

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
    uint8_t skin_id;
    uint8_t respawns_left;
    char username[16];
} NetPlayer;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    uint8_t type_and_active;
} NetProjectile;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t bx;
    int16_t by;
    int16_t bz;
    uint8_t flags;
} NetDrone;

typedef struct {
    uint8_t type;
    uint8_t action;
    int8_t room_id;
    uint8_t plane_class;
    uint8_t skin_id;
    uint8_t city_type;
    char username[MAX_USERNAME_LEN];
} LobbyClientPacket;

typedef struct {
    uint8_t type;
    int8_t room_id;
    uint8_t connected_count;
    uint8_t plane_class;
    uint8_t skin_id;
    uint8_t city_type;
    bool account_loaded;
    int32_t xp;
    int32_t gems;
    char username[MAX_USERNAME_LEN];
} LobbyServerPacket;

typedef struct {
    uint8_t type;
    int8_t room_id;
    uint32_t seq;
    Vector3 position;
    float pitch;
    float yaw;
    float roll;
    uint8_t plane_class;
    uint8_t skin_id;
    uint8_t city_type;
    uint8_t fire_slot;
    uint8_t respawn_cmd;
    char username[16];
} ClientPacket;

typedef struct {
    uint8_t type;
    int8_t room_id;
    int8_t your_id;
    uint8_t connected_count;
    uint32_t seq;
    unsigned int map_seed;

    uint8_t current_round;
    uint16_t round_time_left;
    uint16_t team_buildings_destroyed[2];
    uint32_t team_city_damage[2];

    bool connected[MAX_CLIENTS];
    NetPlayer players[MAX_CLIENTS];
    uint8_t building_hp[MAX_BUILDINGS];
    NetDrone drones[MAX_DRONE_COUNT];

    uint8_t active_projectile_count;
    NetProjectile projectiles[MAX_PROJECTILES];
} ServerPacket;

#pragma pack(pop)

#define SERVER_PACKET_BASE_SIZE offsetof(ServerPacket, projectiles)