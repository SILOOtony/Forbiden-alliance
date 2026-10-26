#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "fa_types.h"
#include "map.h"
#include "drone.h"

#define SERVER_PORT 8080
#define MAX_LOBBY_ROOMS 16
#define MAX_SQUAD_SIZE 5
#define MAX_GAME_ROOMS 8
#define MAX_ROOMS MAX_GAME_ROOMS
#define MAX_CLIENTS 10
#define MAX_PLAYER_PROJECTILE 15
#define MAX_PROJECTILES (MAX_CLIENTS * MAX_PLAYER_PROJECTILE)
#define ROUND_DURATION_SEC 180.0f
#define MAX_USERNAME_LEN 24
#define MAX_PASSWORD_LEN 16

typedef enum {
    PACKET_LOBBY = 1,
    PACKET_GAME  = 2
} PacketType;

typedef enum {
    LOBBY_CREATE_SQUAD = 1,
    LOBBY_JOIN_SQUAD   = 2,
    LOBBY_HEARTBEAT    = 3,
    LOBBY_LEAVE_SQUAD  = 4,
    LOBBY_LOGIN        = 5,
    LOBBY_SET_LOADOUT  = 6,
    LOBBY_TOGGLE_READY = 7,
    LOBBY_SET_CITY     = 8  // Nouveau : change immédiatement la ville du salon sur le serveur
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

static inline const char *GetRankName(int elo) {
    if (elo >= 1500) return "DIAMANT";
    if (elo >= 1300) return "OR";
    if (elo >= 1100) return "ARGENT";
    return "BRONZE";
}

#pragma pack(push, 1)

typedef struct {
    char username[16];
    uint8_t plane_class;
    uint8_t skin_id;
    bool is_ready;
    int16_t elo;
    uint8_t level;
} SquadMemberNet;

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
    int16_t elo;
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
    int8_t squad_id;
    uint8_t plane_class;
    uint8_t skin_id;
    uint8_t city_type;
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
} LobbyClientPacket;

typedef struct {
    uint8_t type;
    int8_t squad_id;
    uint8_t squad_count;
    SquadMemberNet squad_members[MAX_SQUAD_SIZE];

    uint8_t plane_class;
    uint8_t skin_id;
    uint8_t unlocked_skins;
    uint8_t city_type;
    bool account_loaded;
    bool auth_failed;
    bool is_ready;
    bool in_matchmaking;
    uint8_t matchmaking_secs;

    int8_t assigned_game_room;
    uint32_t session_token;

    int32_t xp;
    int32_t gems;
    int32_t elo;
    char username[MAX_USERNAME_LEN];
} LobbyServerPacket;

typedef struct {
    uint8_t type;
    int8_t room_id;
    uint32_t session_token;
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