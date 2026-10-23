#include "weaponSlot.h"

void InitWeaponSlotsForClass(Weapon slots[MAX_WEAPON_SLOTS], PlaneClass plane_class) {
    if (plane_class == PLANE_INTERCEPTOR) {
        // Intercepteur : Spécialisé chasse Air-Air et tir rapide
        slots[0] = (Weapon){
            .type = WEAPON_ROCKET,
            .name = "Light Rockets",
            .ammo = 16,
            .max_ammo = 16,
            .reload_time = 1.0f,
            .reload_timer = 0.0f,
            .projectile_speed = 360.0f,
            .damage = 25
        };
        slots[1] = (Weapon){
            .type = WEAPON_AIR_TO_AIR,
            .name = "Fast Air-to-Air",
            .ammo = 10,
            .max_ammo = 10,
            .reload_time = 1.4f,
            .reload_timer = 0.0f,
            .projectile_speed = 520.0f,
            .damage = 65
        };
        slots[2] = (Weapon){
            .type = WEAPON_AIR_TO_GROUND,
            .name = "Light Bomb",
            .ammo = 2,
            .max_ammo = 2,
            .reload_time = 4.0f,
            .reload_timer = 0.0f,
            .projectile_speed = 240.0f,
            .damage = 75
        };
    } 
    else if (plane_class == PLANE_BOMBER) {
        // Bombardier : Spécialisé destruction de bâtiments (Air-Sol lourd)
        slots[0] = (Weapon){
            .type = WEAPON_ROCKET,
            .name = "Heavy Rockets",
            .ammo = 8,
            .max_ammo = 8,
            .reload_time = 2.0f,
            .reload_timer = 0.0f,
            .projectile_speed = 260.0f,
            .damage = 50
        };
        slots[1] = (Weapon){
            .type = WEAPON_AIR_TO_AIR,
            .name = "Defense A2A",
            .ammo = 4,
            .max_ammo = 4,
            .reload_time = 3.0f,
            .reload_timer = 0.0f,
            .projectile_speed = 380.0f,
            .damage = 45
        };
        slots[2] = (Weapon){
            .type = WEAPON_AIR_TO_GROUND,
            .name = "Mega Bomb",
            .ammo = 8,
            .max_ammo = 8,
            .reload_time = 2.5f,
            .reload_timer = 0.0f,
            .projectile_speed = 220.0f,
            .damage = 160
        };
    } 
    else {
        // Chasseur Polyvalent (par défaut)
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
}

void InitWeaponSlots(Weapon slots[MAX_WEAPON_SLOTS]) {
    InitWeaponSlotsForClass(slots, PLANE_FIGHTER);
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