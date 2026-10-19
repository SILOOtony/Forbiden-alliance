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

    Weapon weapons[MAX_WEAPON_SLOTS]; // Les 3 slots d'armes
    int current_slot;                 // 0 = Slot 1, 1 = Slot 2, 2 = Slot 3
} PlayerState;

Player *InitPlayer(Player* p, int id, char username[50], int plan, int gems, int power);