#include "weaponSlot.h"

void InitWeaponSlots(Weapon slots[MAX_WEAPON_SLOTS]) {
    slots[0] = (Weapon){
        .type = WEAPON_ROCKET,
        .name = "Rockets",
        .ammo = 12,
        .max_ammo = 12,
        .reload_time = 1.5f,
        .reload_timer = 0.0f,
        .projectile_speed = 300.0f,
        .damage = 35
    };

    slots[1] = (Weapon){
        .type = WEAPON_AIR_TO_AIR,
        .name = "Air-to-Air",
        .ammo = 6,
        .max_ammo = 6,
        .reload_time = 2.5f,
        .reload_timer = 0.0f,
        .projectile_speed = 420.0f,
        .damage = 55
    };

    slots[2] = (Weapon){
        .type = WEAPON_AIR_TO_GROUND,
        .name = "Air-to-Ground",
        .ammo = 4,
        .max_ammo = 4,
        .reload_time = 3.5f,
        .reload_timer = 0.0f,
        .projectile_speed = 240.0f,
        .damage = 120
    };
}

void UpdateWeaponsReload(Weapon slots[MAX_WEAPON_SLOTS], float dt) {
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        if (slots[i].ammo < slots[i].max_ammo) {
            slots[i].reload_timer += dt;
            if (slots[i].reload_timer >= slots[i].reload_time) {
                slots[i].ammo++;
                slots[i].reload_timer = 0.0f;
            }
        } else {
            slots[i].reload_timer = 0.0f;
        }
    }
}