#pragma once
#include "raylib.h"
#include <stdbool.h>
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
    bool is_alive;
    bool is_drone;                    // true si le joueur est réincarné en drone
    Weapon weapons[MAX_WEAPON_SLOTS];
    int current_slot;
} PlayerState;

Player *InitPlayer(Player* p, int id, char username[50], int plan, int gems, int power);