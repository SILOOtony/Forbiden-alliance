#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATUS_MENU,
    STATUS_PLAYING,
    STATUS_PAUSED,
    STATUS_QUIT
} GameStatus;

typedef enum {
    MENU_CMD_NONE = 0,
    MENU_CMD_CREATE_ROOM,
    MENU_CMD_JOIN_ROOM,
    MENU_CMD_LEAVE_ROOM,
    MENU_CMD_START_GAME,
    MENU_CMD_LOGIN,
    MENU_CMD_SELECT_CLASS
} MenuCommand;

MenuCommand DrawMenu(
    GameStatus *status,
    int currentRoom,
    uint8_t connectedCount,
    int *outJoinRoomId,
    char *username,
    bool accountLoaded,
    uint8_t *selectedClass
);
void UpdateMenuCamera(Camera3D *camera, float angle);