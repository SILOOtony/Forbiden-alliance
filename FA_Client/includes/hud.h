#pragma once
#include "raylib.h"
#include "player.h"
#include "map.h"
#include "drone.h"
#include "network.h"

#define MAX_KILLFEED 5

typedef struct {
    char text[64];
    Color color;
    float timer;
    bool active;
} KillfeedEntry;

void DrawCrossHair(const PlayerState *plane);
void DrawWeaponSlots(const PlayerState *plane);
void DrawHpBar(const PlayerState *plane);
void DrawDeathScreen(const PlayerState *plane);
void DrawCityTopBar(const GameMap *map, uint8_t current_round, uint16_t time_left, bool is_attacker);
void DrawMatchEndScreen(const uint16_t buildings_destroyed[2], const uint32_t city_damage[2], uint16_t time_left);
void DrawLockOnHUD(bool has_target, Vector2 screen_pos, float lock_progress, bool incoming_missile);
void DrawTacticalRadar(
    const PlayerState *localPlane,
    const ServerPacket *serverData,
    const PlayerState remotePlayers[],
    const Drone drones[],
    int maxDrones,
    float flareActiveTimer
);

// Étape 1 & 2 : Hitmarker, Killfeed et Bannière de Manche
void DrawHitmarker(Vector2 crosshairScreenPos, float hitTimer, bool isKillHit);
void AddKillfeedEntry(KillfeedEntry feed[], const char *text, Color color);
void UpdateKillfeed(KillfeedEntry feed[], float dt);
void DrawKillfeed(const KillfeedEntry feed[]);
void DrawRoundBanner(uint8_t currentRound, bool isAttacker, float bannerTimer);