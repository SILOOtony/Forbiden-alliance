#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include "network.h"

typedef enum {
    STATUS_MENU,
    STATUS_LOBBY,
    STATUS_PLAYING,
    STATUS_PAUSED,
    STATUS_QUIT
} GameStatus;

void DrawMenu(GameStatus *status);
int DrawLobby(GameStatus *status, const uint8_t room_players[MAX_ROOMS]);
void UpdateMenuCamera(Camera3D *camera, float angle);