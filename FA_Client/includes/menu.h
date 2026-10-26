#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include "network.h"

typedef enum {
    STATUS_MENU,
    STATUS_PLAYING,
    STATUS_PAUSED,
    STATUS_QUIT
} GameStatus;

typedef enum {
    MENU_CMD_NONE = 0,
    MENU_CMD_CREATE_SQUAD,
    MENU_CMD_JOIN_SQUAD,
    MENU_CMD_LEAVE_SQUAD,
    MENU_CMD_TOGGLE_READY,
    MENU_CMD_LOGIN,
    MENU_CMD_SET_LOADOUT,
    MENU_CMD_SELECT_CITY
} MenuCommand;

MenuCommand DrawMenu(
    GameStatus *status,
    const LobbyServerPacket *lobbyState,
    int *outJoinSquadId,
    char *username,
    char *password,
    bool accountLoaded,
    bool authFailed,
    uint8_t *selectedClass,
    uint8_t *selectedSkin,
    uint8_t *selectedCity,
    int accountXp,
    int accountGems,
    int accountElo,
    uint8_t unlockedSkins
);
void UpdateMenuCamera(Camera3D *camera, float angle);