#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include "weaponSlot.h"

typedef struct {
    int id;
    char username[50];
    int plan;
    int gems;
    int power;
} Player;

typedef struct {
    Player player;
    Vector3 position;
    Vector3 velocity;
    float pitch, yaw, roll;
    float speed;
    int hp;
    int max_hp;
    bool is_alive;
    bool is_drone;
    uint8_t team;
    uint8_t plane_class; // 0 = Chasseur, 1 = Intercepteur, 2 = Bombardier
    uint8_t skin_id;     // 0 = Classique, 1 = Furtif, 2 = Or, 3 = Cyber, 4 = Arctique
    bool is_attacker;
    int plane_respawns_left;
    Weapon weapons[MAX_WEAPON_SLOTS];
    int current_slot;
} PlayerState;

Player *InitPlayer(Player* p, int id, char username[50], int plan, int gems, int power);