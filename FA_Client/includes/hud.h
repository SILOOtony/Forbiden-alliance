#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"
#include "player.h"
#include "map.h"

void DrawCrossHair(const PlayerState *state);
void DrawWeaponSlots(PlayerState *state);
void DrawHpBar(const PlayerState *state);
void DrawCityTopBar(const GameMap *map, uint8_t currentRound, uint16_t timeLeft, bool isAttacker);
void DrawLockOnHUD(bool hasTarget, Vector2 targetScreenPos, float lockProgress, bool incomingMissile);
void DrawDeathScreen(const PlayerState *state);
void DrawMatchEndScreen(const uint16_t buildingsDestroyed[2], const uint32_t cityDamage[2], uint16_t timeLeft);