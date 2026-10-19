#pragma once

#include "raylib.h"
#include "player.h"
#include "input.h"

// Initialise la position et la vitesse de départ de l'avion
void InitPlayerState(PlayerState *state);

// Met à jour les angles et la position de l'avion selon les touches pressées
void UpdatePlayerMovement(PlayerState *state, KeyBindings *controls, float dt);

// Met à jour la caméra 3D pour qu'elle suive l'avion
void UpdatePlaneCamera(Camera3D *camera, const PlayerState *state);

// Dessine l'avion en 3D avec ses rotations (pitch, yaw, roll)
void DrawPlanePlayer(const PlayerState *state);