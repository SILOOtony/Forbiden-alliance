#pragma once
#include "raylib.h"
#include <stdbool.h>

typedef enum {
    STATUS_MENU,
    STATUS_PLAYING,
    STATUS_PAUSED,
    STATUS_QUIT
} GameStatus;

// Affiche le menu et met à jour le statut du jeu si on clique sur un bouton
void DrawMenu(GameStatus *status);

// Fait tourner la caméra en orbite au-dessus de la ville quand on est dans le menu
void UpdateMenuCamera(Camera3D *camera, float angle);