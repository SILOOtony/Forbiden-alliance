#pragma once
#include <stdbool.h>

#define MAX_WEAPON_SLOTS 3

typedef enum {
    PLANE_FIGHTER     = 0, // Chasseur Polyvalent (100 PV)
    PLANE_INTERCEPTOR = 1, // Intercepteur Léger (75 PV)
    PLANE_BOMBER      = 2  // Bombardier Lourd (150 PV)
} PlaneClass;

typedef enum {
    WEAPON_ROCKET = 0,
    WEAPON_AIR_TO_AIR = 1,
    WEAPON_AIR_TO_GROUND = 2,
    WEAPON_DRONE_BLASTER = 3
} WeaponType;

typedef struct {
    WeaponType type;
    const char *name;
    int ammo;
    int max_ammo;
    float reload_time;
    float reload_timer;
    float projectile_speed;
    int damage;
} Weapon;

void InitWeaponSlots(Weapon slots[MAX_WEAPON_SLOTS]);
void InitWeaponSlotsForClass(Weapon slots[MAX_WEAPON_SLOTS], PlaneClass plane_class);
void UpdateWeaponsReload(Weapon slots[MAX_WEAPON_SLOTS], float dt);