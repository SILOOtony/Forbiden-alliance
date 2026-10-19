#pragma once
#include "raylib.h"
#include "player.h"

void DrawCrossHair(const PlayerState *state);
void DrawWeaponSlots(PlayerState *state);
void DrawHpBar(const PlayerState *state);
void DrawDeathScreen(void);