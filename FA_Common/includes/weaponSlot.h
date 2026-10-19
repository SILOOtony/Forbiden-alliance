#pragma once

#include <stdbool.h>

#define MAX_WEAPON_SLOTS 3

typedef enum {
    WEAPON_ROCKET = 0,
    WEAPON_AIR_TO_AIR = 1,
    WEAPON_AIR_TO_GROUND = 2
} WeaponType;

typedef struct {
    WeaponType type;
    const char *name;       // Nom affiché sur le HUD
    int ammo;               // Munitions actuelles
    int max_ammo;           // Capacité du chargeur
    float reload_time;      // Temps pour recharger 1 munition
    float reload_timer;     // Chronomètre interne de l'arme
    float projectile_speed; // Vitesse du missile tiré
    int damage;             // Dégâts infligés à l'impact
} Weapon;

// Initialise les 3 slots avec leurs statistiques respectives
void InitWeaponSlots(Weapon slots[MAX_WEAPON_SLOTS]);

// Recharge automatiquement toutes les armes qui ne sont pas pleines
void UpdateWeaponsReload(Weapon slots[MAX_WEAPON_SLOTS], float dt);