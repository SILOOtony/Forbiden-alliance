#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>
#include "network.h"
#include "weaponSlot.h"

#define TICK_RATE 60
#define TICK_DT (1.0 / (double)TICK_RATE)

typedef struct {
    struct sockaddr_in addr;
    bool connected;
    double last_seen;
    char username[MAX_USERNAME_LEN];
    uint8_t plane_class;
    uint8_t skin_id;
    uint8_t unlocked_skins;
    int xp;
    int gems;
    int elo;
    bool is_ready;
    int8_t assigned_game_room;
    uint32_t session_token;
} SquadMemberSlot;

typedef struct {
    SquadMemberSlot members[MAX_SQUAD_SIZE];
    uint8_t preferred_city;
    bool in_matchmaking;
    double matchmaking_start;
} LobbyRoom;

typedef struct {
    Vector3 position;
    Vector3 direction;
    float speed;
    float lifetime;
    int damage;
    uint8_t type;
    bool active;
    int8_t target_kind; // 0 = aucun, 1 = avion ennemi, 2 = drone ennemi
    int8_t target_idx;
} ServerProjectile;

typedef struct {
    struct sockaddr_in addr;
    bool connected;
    double last_seen;
    double last_move_time;
    uint32_t session_token;
    uint32_t last_seq;
    NetPlayer state;
    bool has_pos;
    float flare_timer; // Immunité aux missiles Air-Air pendant que les leurres sont actifs
    Weapon weapons[MAX_WEAPON_SLOTS];
    ServerProjectile projectiles[MAX_PLAYER_PROJECTILE];
} ClientSlot;

typedef struct {
    unsigned int map_seed;
    GameMap map;
    ClientSlot clients[MAX_CLIENTS];
    Drone drones[MAX_DRONE_COUNT];
    bool had_players;
    bool rewards_distributed;
    uint32_t server_seq;

    uint8_t current_round;
    float round_timer;
    uint16_t team_buildings_destroyed[2];
    uint32_t team_city_damage[2];
} GameRoom;

// server_accounts.c
void SanitizeUsername(const char *in, char *out, size_t max_len);
void SanitizePassword(const char *in, char *out, size_t max_len);
void SaveAccountJSON(
    const char *clean_user,
    const char *clean_pass,
    uint8_t plane_class,
    uint8_t skin_id,
    uint8_t unlocked_skins,
    int xp,
    int gems,
    int elo
);
bool AuthenticateAccountJSON(
    const char *raw_user,
    const char *raw_pass,
    bool allow_create_if_missing,
    char *clean_out,
    char *clean_pass_out,
    uint8_t *out_class,
    uint8_t *out_skin,
    uint8_t *out_unlocked_skins,
    int *out_xp,
    int *out_gems,
    int *out_elo
);
void RewardMatchPlayersJSON(GameRoom *room);

// server_game.c
bool IsSameClient(struct sockaddr_in a, struct sockaddr_in b);
int16_t GetClassMaxHp(uint8_t plane_class, bool is_drone);
float GetClassMaxAllowedSpeed(uint8_t plane_class, bool is_drone);
void InitServerPlayerWeapons(ClientSlot *c);
void StartRoomRound(GameRoom *room, uint8_t round_num);
void ResetGameRoom(GameRoom *room, int room_index);
uint8_t CountGameRoomPlayers(const GameRoom *room);
void FindHomingTarget(
    const GameRoom *room,
    int shooterSlot,
    Vector3 startPos,
    Vector3 forward,
    bool shooterIsAttacker,
    int8_t *outKind,
    int8_t *outIdx
);
void UpdateGameRoomTick(
    GameRoom *room,
    int room_id,
    LobbyRoom lobbyRooms[],
    int sock,
    double now,
    float dt
);