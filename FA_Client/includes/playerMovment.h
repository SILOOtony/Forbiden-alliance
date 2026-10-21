#pragma once
#include "raylib.h"
#include "player.h"
#include "input.h"

void InitPlayerState(PlayerState *state);
void ReincarnateAsDrone(PlayerState *state, Vector3 dronePos);
void UpdatePlayerMovement(PlayerState *state, KeyBindings *controls, float dt);
void UpdatePlaneCamera(Camera3D *camera, const PlayerState *state);
void DrawPlanePlayer(const PlayerState *state);