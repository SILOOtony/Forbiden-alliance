#include "menu.h"
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "raylib.h"
#include "weaponSlot.h"

static bool isTypingCode = false;
static bool isEditingUsername = false;
static bool isEditingPassword = false;
static int typedSquadCode = 1;

void UpdateMenuCamera(Camera3D *camera, float angle) {
    camera->position.x = sinf(angle * 0.35f) * 6.0f;
    camera->position.y = 112.0f;
    camera->position.z = 128.0f;
    camera->target = (Vector3){ 0.0f, 104.0f, 95.0f };
}

static bool DrawButtonRect(const char *text, Rectangle bounds, int fontSize, Color baseColor, Color hoverColor) {
    Vector2 mousePos = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mousePos, bounds);

    DrawRectangleRec(bounds, isHovered ? hoverColor : baseColor);
    DrawRectangleLinesEx(bounds, 2.0f, DARKGRAY);

    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(bounds.x + (bounds.width - textWidth) / 2.0f);
    int textY = (int)(bounds.y + (bounds.height - fontSize) / 2.0f);
    DrawText(text, textX, textY, fontSize, isHovered ? WHITE : RAYWHITE);

    return (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
}

static bool DrawCenteredButton(const char *text, float yPos, int width, int height, int fontSize, Color baseColor, Color hoverColor) {
    Rectangle bounds = {
        (float)(GetScreenWidth() - width) / 2.0f,
        yPos,
        (float)width,
        (float)height
    };
    return DrawButtonRect(text, bounds, fontSize, baseColor, hoverColor);
}

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
) {
    MenuCommand cmd = MENU_CMD_NONE;
    int screenW = GetScreenWidth();

    if (*status == STATUS_PAUSED) {
        DrawRectangle(0, 0, screenW, GetScreenHeight(), Fade(BLACK, 0.65f));
        if (DrawCenteredButton("REPRENDRE LA PARTIE", 260.0f, 340, 55, 22, DARKBLUE, SKYBLUE)) {
            *status = STATUS_PLAYING;
        }
        if (DrawCenteredButton("QUITTER LA PARTIE", 330.0f, 340, 55, 22, MAROON, RED)) {
            cmd = MENU_CMD_LEAVE_SQUAD;
            *status = STATUS_MENU;
        }
        if (DrawCenteredButton("QUITTER LE JEU", 400.0f, 340, 55, 22, DARKGRAY, GRAY)) {
            *status = STATUS_QUIT;
        }
        return cmd;
    }

    DrawRectangle(0, 0, screenW, 240, Fade(BLACK, 0.72f));
    DrawRectangle(0, 485, screenW, GetScreenHeight() - 485, Fade(BLACK, 0.72f));

    const char *title = "FORBIDDEN ALLIANCE";
    int titleSize = 42;
    int titleWidth = MeasureText(title, titleSize);
    int titleX = (screenW - titleWidth) / 2;
    DrawText(title, titleX + 3, 10, titleSize, BLACK);
    DrawText(title, titleX, 7, titleSize, GOLD);

    int level = 1 + (accountXp / 200);

    // --- 1. BARRE DE COMPTE JOUEUR + ELO / RANG ---
    Rectangle accBox = { (float)(screenW - 720) / 2.0f, 52.0f, 720.0f, 64.0f };
    DrawRectangleRec(accBox, Fade(BLACK, 0.8f));
    Color borderCol = authFailed ? RED : (accountLoaded ? LIME : GOLD);
    DrawRectangleLinesEx(accBox, 2.0f, borderCol);

    DrawText("PSEUDO:", (int)accBox.x + 12, (int)accBox.y + 10, 16, LIGHTGRAY);
    Rectangle userRect = { accBox.x + 90.0f, accBox.y + 5.0f, 155.0f, 30.0f };

    DrawText("PASS:", (int)accBox.x + 258, (int)accBox.y + 10, 16, LIGHTGRAY);
    Rectangle passRect = { accBox.x + 312.0f, accBox.y + 5.0f, 130.0f, 30.0f };

    Vector2 mousePos = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        isEditingUsername = CheckCollisionPointRec(mousePos, userRect);
        isEditingPassword = CheckCollisionPointRec(mousePos, passRect);
    }

    DrawRectangleRec(userRect, isEditingUsername ? DARKBLUE : DARKGRAY);
    DrawRectangleLinesEx(userRect, 2.0f, isEditingUsername ? GOLD : GRAY);
    DrawText(TextFormat("%s%s", username, isEditingUsername ? "_" : ""), (int)userRect.x + 8, (int)userRect.y + 6, 18, WHITE);

    char maskedPass[MAX_PASSWORD_LEN];
    size_t passLen = strlen(password);
    for (size_t i = 0; i < passLen && i < MAX_PASSWORD_LEN - 1; i++) maskedPass[i] = '*';
    maskedPass[passLen] = '\0';

    DrawRectangleRec(passRect, isEditingPassword ? DARKBLUE : DARKGRAY);
    DrawRectangleLinesEx(passRect, 2.0f, isEditingPassword ? GOLD : GRAY);
    DrawText(TextFormat("%s%s", isEditingPassword ? password : maskedPass, isEditingPassword ? "_" : ""),
             (int)passRect.x + 8, (int)passRect.y + 6, 18, WHITE);

    if (isEditingUsername) {
        int key = GetCharPressed();
        while (key > 0) {
            size_t len = strlen(username);
            if ((isalnum(key) || key == '_' || key == '-') && len < MAX_USERNAME_LEN - 2) {
                username[len] = (char)key;
                username[len + 1] = '\0';
                accountLoaded = false;
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t len = strlen(username);
            if (len > 0) {
                username[len - 1] = '\0';
                accountLoaded = false;
            }
        }
        if (IsKeyPressed(KEY_ENTER) && strlen(username) > 0) {
            isEditingUsername = false;
            cmd = MENU_CMD_LOGIN;
        }
    } else if (isEditingPassword) {
        int key = GetCharPressed();
        while (key > 0) {
            size_t len = strlen(password);
            if ((isalnum(key) || key == '_' || key == '-') && len < MAX_PASSWORD_LEN - 2) {
                password[len] = (char)key;
                password[len + 1] = '\0';
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t len = strlen(password);
            if (len > 0) password[len - 1] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER) && strlen(username) > 0) {
            isEditingPassword = false;
            cmd = MENU_CMD_LOGIN;
        }
    }

    Rectangle loginBtn = { accBox.x + 455.0f, accBox.y + 5.0f, 252.0f, 30.0f };
    const char *loginLabel = authFailed ? "ERREUR MOT DE PASSE" : (accountLoaded ? "PROFIL SYNCHRONISE" : "CONNECTER / CREER");
    Color loginCol = authFailed ? MAROON : (accountLoaded ? DARKGREEN : DARKBLUE);
    if (DrawButtonRect(loginLabel, loginBtn, 14, loginCol, SKYBLUE) && strlen(username) > 0) {
        isEditingUsername = false;
        isEditingPassword = false;
        cmd = MENU_CMD_LOGIN;
    }

    if (authFailed) {
        const char *errTxt = "MOT DE PASSE INCORRECT POUR CE PSEUDO !";
        DrawText(errTxt, (screenW - MeasureText(errTxt, 16)) / 2, (int)accBox.y + 41, 16, RED);
    } else {
        const char *progTxt = TextFormat("RANG : %s (%d ELO)   |   NIVEAU %d   |   XP : %d   |   GEMMES : %d",
                                         GetRankName(accountElo), accountElo, level, accountXp, accountGems);
        DrawText(progTxt, (screenW - MeasureText(progTxt, 16)) / 2, (int)accBox.y + 41, 16, GOLD);
    }

    // --- 2. CLASSES D'AVIONS (DÉBLOCAGE PAR NIVEAU) ---
    float startX = (float)(screenW - 720) / 2.0f;
    const int reqLevels[3] = { 1, 2, 4 };
    const char *cLabels[3] = {
        "1. CHASSEUR (Niv.1)",
        (level >= 2) ? "2. INTERCEPTEUR (Niv.2)" : "2. INTERCEPTEUR [BLOQUE Niv.2]",
        (level >= 4) ? "3. BOMBARDIER (Niv.4)"   : "3. BOMBARDIER [BLOQUE Niv.4]"
    };

    for (int c = 0; c < 3; c++) {
        Rectangle cBtn = { startX + c * 244.0f, 122.0f, 232.0f, 34.0f };
        bool unlocked = (level >= reqLevels[c]);
        bool isSel = (*selectedClass == (uint8_t)c);
        Color bg = !unlocked ? (Color){ 60, 25, 25, 255 } : (isSel ? DARKGREEN : DARKGRAY);
        if (DrawButtonRect(cLabels[c], cBtn, 13, bg, unlocked ? LIME : RED) && unlocked) {
            *selectedClass = (uint8_t)c;
            cmd = MENU_CMD_SET_LOADOUT;
        }
        if (isSel) DrawRectangleLinesEx(cBtn, 2.5f, GOLD);
    }

    // --- 3. BOUTIQUE DE SKINS (ACHAT EN GEMMES & ÉQUIPEMENT) ---
    const char *sBaseNames[5] = { "CLASSIQUE", "FURTIF", "OR", "CYBER", "ARCTIQUE" };
    const int skinCosts[5] = { 0, 50, 100, 150, 200 };

    for (int s = 0; s < 5; s++) {
        Rectangle sBtn = { startX + s * 146.0f, 162.0f, 136.0f, 32.0f };
        bool isOwned = ((unlockedSkins & (1 << s)) != 0);
        bool isSel   = (*selectedSkin == (uint8_t)s);

        const char *label = isSel ? TextFormat("%s [EQUIPE]", sBaseNames[s]) :
                            (isOwned ? TextFormat("%s [POSSEDE]", sBaseNames[s]) :
                             TextFormat("%s (%dG)", sBaseNames[s], skinCosts[s]));

        Color bg = isSel ? DARKPURPLE : (isOwned ? (Color){ 45, 55, 75, 255 } : (Color){ 75, 55, 20, 255 });
        if (DrawButtonRect(label, sBtn, 11, bg, PURPLE)) {
            if (isOwned || accountGems >= skinCosts[s]) {
                *selectedSkin = (uint8_t)s;
                cmd = MENU_CMD_SET_LOADOUT;
            }
        }
        if (isSel) DrawRectangleLinesEx(sBtn, 2.0f, GOLD);
    }

    // --- 4. CHOIX DE LA VILLE ---
    const char *cityNames[4] = { "VILLE : PARIS", "VILLE : NEW YORK", "VILLE : TOKYO", "VILLE : DUBAI" };
    for (int m = 0; m < 4; m++) {
        Rectangle mBtn = { startX + m * 183.0f, 200.0f, 171.0f, 32.0f };
        bool isSel = (*selectedCity == (uint8_t)m);
        if (DrawButtonRect(cityNames[m], mBtn, 13, isSel ? DARKBLUE : DARKGRAY, SKYBLUE)) {
            *selectedCity = (uint8_t)m;
            cmd = MENU_CMD_SELECT_CITY;
        }
        if (isSel) DrawRectangleLinesEx(mBtn, 2.0f, GOLD);
    }

    // --- 5. ZONE INFÉRIEURE : CADRE ÉLARGI (720px) + NUMÉRO DE ROOM BIEN VISIBLE ---
    int squadId = lobbyState->squad_id;
    Rectangle squadBox = { (float)(screenW - 720) / 2.0f, 492.0f, 720.0f, 54.0f };
    DrawRectangleRec(squadBox, Fade(BLACK, 0.85f));
    DrawRectangleLinesEx(squadBox, 2.0f, (squadId != -1) ? LIME : GRAY);

    if (squadId != -1) {
        const char *sqTitle = TextFormat("CODE ROOM / ESCOUADE : #%d   |   JOUEURS : %d / 5   |   VILLE : %s",
                                         squadId + 1, lobbyState->squad_count, GetCityName(*selectedCity));
        DrawText(sqTitle, (screenW - MeasureText(sqTitle, 17)) / 2, 498, 17, GOLD);

        const char *statusLine = lobbyState->in_matchmaking
            ? TextFormat("MATCHMAKING EN COURS... Recherche de partie (%us)", lobbyState->matchmaking_secs)
            : "Donnez le CODE ci-dessus a vos amis, puis cliquez tous sur PRET !";
        DrawText(statusLine, (screenW - MeasureText(statusLine, 14)) / 2, 523, 14,
                 lobbyState->in_matchmaking ? SKYBLUE : LIME);
    } else {
        const char *noSq = "MODE SOLO / AUCUNE ROOM (Creez une Room 1-16 ou cliquez sur PRET)";
        DrawText(noSq, (screenW - MeasureText(noSq, 15)) / 2, 500, 15, LIGHTGRAY);
        const char *subSq = "Les avions de votre escouade s'affichent en 3D au centre de l'ecran !";
        DrawText(subSq, (screenW - MeasureText(subSq, 14)) / 2, 523, 14, GRAY);
    }

    const char *readyTxt = lobbyState->is_ready
        ? (lobbyState->in_matchmaking ? "ANNULER (MATCHMAKING EN COURS...)" : "PRET ! (EN ATTENTE DE LA ROOM)")
        : "PRET (LANCER LE MATCHMAKING)";
    Color readyBg = lobbyState->is_ready ? ORANGE : DARKGREEN;
    Color readyHover = lobbyState->is_ready ? RED : LIME;

    if (DrawCenteredButton(readyTxt, 554.0f, 440, 48, 20, readyBg, readyHover)) {
        isTypingCode = false;
        isEditingUsername = false;
        isEditingPassword = false;
        cmd = MENU_CMD_TOGGLE_READY;
    }

    if (squadId == -1) {
        if (!isTypingCode) {
            Rectangle createBtn = { (float)(screenW / 2 - 310), 610.0f, 300.0f, 42.0f };
            Rectangle joinBtn   = { (float)(screenW / 2 + 10),  610.0f, 300.0f, 42.0f };
            if (DrawButtonRect("CREER UNE ROOM ESCOUADE", createBtn, 16, DARKBLUE, SKYBLUE)) {
                cmd = MENU_CMD_CREATE_SQUAD;
            }
            if (DrawButtonRect("REJOINDRE AVEC UN CODE", joinBtn, 16, DARKPURPLE, PURPLE)) {
                isTypingCode = true;
            }
        } else {
            const char *prompt = TextFormat("Code de Room Escouade (1 a %d) : [ %d ]", MAX_LOBBY_ROOMS, typedSquadCode);
            DrawText(prompt, (screenW - MeasureText(prompt, 18)) / 2, 608, 18, GOLD);

            Rectangle minusBtn = { (float)(screenW / 2 - 190), 632.0f, 45.0f, 34.0f };
            Rectangle plusBtn  = { (float)(screenW / 2 - 135), 632.0f, 45.0f, 34.0f };
            Rectangle okBtn    = { (float)(screenW / 2 - 75),  632.0f, 150.0f, 34.0f };
            Rectangle cancelB  = { (float)(screenW / 2 + 85),  632.0f, 110.0f, 34.0f };

            if (DrawButtonRect("-", minusBtn, 22, DARKGRAY, SKYBLUE)) {
                typedSquadCode = (typedSquadCode > 1) ? typedSquadCode - 1 : MAX_LOBBY_ROOMS;
            }
            if (DrawButtonRect("+", plusBtn, 22, DARKGRAY, SKYBLUE)) {
                typedSquadCode = (typedSquadCode < MAX_LOBBY_ROOMS) ? typedSquadCode + 1 : 1;
            }
            if (DrawButtonRect("REJOINDRE", okBtn, 16, DARKGREEN, LIME)) {
                *outJoinSquadId = typedSquadCode - 1;
                isTypingCode = false;
                cmd = MENU_CMD_JOIN_SQUAD;
            }
            if (DrawButtonRect("ANNULER", cancelB, 15, MAROON, RED)) {
                isTypingCode = false;
            }
        }
    } else {
        if (DrawCenteredButton(TextFormat("QUITTER LA ROOM #%d", squadId + 1), 610.0f, 280, 40, 17, MAROON, RED)) {
            cmd = MENU_CMD_LEAVE_SQUAD;
        }
    }

    if (!isTypingCode && DrawCenteredButton("QUITTER LE JEU", 662.0f, 220, 36, 16, DARKGRAY, GRAY)) {
        *status = STATUS_QUIT;
    }

    return cmd;
}