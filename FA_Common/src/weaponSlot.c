#include "weaponSlot.h"

void InitWeaponSlots(Weapon slots[MAX_WEAPON_SLOTS]) {
    // Slot 1 : Roquettes (Rapide, 15 munitions, 0.8s de recharge)
    slots[0] = (Weapon){
        .type = WEAPON_ROCKET,
        .name = "Rockets",
        .ammo = 15,
        .max_ammo = 15,
        .reload_time = 0.8f,
        .reload_timer = 0.0f,
        .projectile_speed = 350.0f,
        .damage = 30
    };

    // Slot 2 : Air-to-Air (Très rapide, 6 munitions, 2.0s de recharge)
    slots[1] = (Weapon){
        .type = WEAPON_AIR_TO_AIR,
        .name = "Air-to-Air",
        .ammo = 6,
        .max_ammo = 6,
        .reload_time = 2.0f,
        .reload_timer = 0.0f,
        .projectile_speed = 550.0f,
        .damage = 50
    };

    // Slot 3 : Air-to-Ground (Lent mais dévastateur, 2 munitions, 4.0s de recharge)
    slots[2] = (Weapon){
        .type = WEAPON_AIR_TO_GROUND,
        .name = "Air-to-Ground",
        .ammo = 2,
        .max_ammo = 2,
        .reload_time = 4.0f,
        .reload_timer = 0.0f,
        .projectile_speed = 220.0f,
        .damage = 200
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