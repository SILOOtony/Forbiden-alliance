#include "menu.h"
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "raylib.h"
#include "weaponSlot.h"
#include "network.h"

static bool isTypingCode = false;
static bool isEditingUsername = false;
static int typedRoomCode = 1;

void UpdateMenuCamera(Camera3D *camera, float angle) {
    float radius = 320.0f;
    camera->position.x = cosf(angle) * radius;
    camera->position.y = 140.0f;
    camera->position.z = sinf(angle) * radius;
    camera->target = (Vector3){ 0.0f, 30.0f, 0.0f };
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
    int currentRoom,
    uint8_t connectedCount,
    int *outJoinRoomId,
    char *username,
    bool accountLoaded,
    uint8_t *selectedClass,
    int accountXp,
    int accountGems
) {
    MenuCommand cmd = MENU_CMD_NONE;
    int screenW = GetScreenWidth();

    DrawRectangle(0, 0, screenW, GetScreenHeight(), Fade(BLACK, 0.6f));

    const char *title = "FORBIDDEN ALLIANCE";
    int titleSize = 54;
    int titleWidth = MeasureText(title, titleSize);
    int titleX = (screenW - titleWidth) / 2;
    DrawText(title, titleX + 4, 24, titleSize, BLACK);
    DrawText(title, titleX, 20, titleSize, GOLD);

    if (*status == STATUS_PAUSED) {
        if (DrawCenteredButton("REPRENDRE LA PARTIE", 260.0f, 340, 55, 22, DARKBLUE, SKYBLUE)) {
            *status = STATUS_PLAYING;
        }
        if (DrawCenteredButton("QUITTER LA ROOM", 330.0f, 340, 55, 22, MAROON, RED)) {
            cmd = MENU_CMD_LEAVE_ROOM;
            *status = STATUS_MENU;
        }
        if (DrawCenteredButton("QUITTER LE JEU", 400.0f, 340, 55, 22, DARKGRAY, GRAY)) {
            *status = STATUS_QUIT;
        }
        return cmd;
    }

    // --- 1. BARRE DE COMPTE JOUEUR (JSON VPS + NIVEAU / XP / GEMMES) ---
    Rectangle accBox = { (float)(screenW - 620) / 2.0f, 82.0f, 620.0f, 75.0f };
    DrawRectangleRec(accBox, Fade(BLACK, 0.75f));
    DrawRectangleLinesEx(accBox, 2.0f, accountLoaded ? LIME : GOLD);

    DrawText("IDENTIFIANT :", (int)accBox.x + 15, (int)accBox.y + 14, 18, LIGHTGRAY);

    Rectangle inputRect = { accBox.x + 155.0f, accBox.y + 6.0f, 230.0f, 34.0f };
    Vector2 mousePos = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        isEditingUsername = CheckCollisionPointRec(mousePos, inputRect);
    }

    DrawRectangleRec(inputRect, isEditingUsername ? DARKBLUE : DARKGRAY);
    DrawRectangleLinesEx(inputRect, 2.0f, isEditingUsername ? GOLD : GRAY);
    DrawText(TextFormat("%s%s", username, isEditingUsername ? "_" : ""), (int)inputRect.x + 10, (int)inputRect.y + 7, 20, WHITE);

    if (isEditingUsername) {
        int key = GetCharPressed();
        while (key > 0) {
            size_t len = strlen(username);
            if ((isalnum(key) || key == '_' || key == '-') && len < MAX_USERNAME_LEN - 2) {
                username[len] = (char)key;
                username[len + 1] = '\0';
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t len = strlen(username);
            if (len > 0) username[len - 1] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER)) {
            isEditingUsername = false;
            cmd = MENU_CMD_LOGIN;
        }
    }

    Rectangle loginBtn = { accBox.x + 400.0f, accBox.y + 6.0f, 205.0f, 34.0f };
    const char *loginLabel = accountLoaded ? "COMPTE SYNCHRONISE" : "CHARGER / CREER JSON";
    if (DrawButtonRect(loginLabel, loginBtn, 15, accountLoaded ? DARKGREEN : DARKBLUE, SKYBLUE)) {
        isEditingUsername = false;
        cmd = MENU_CMD_LOGIN;
    }

    int level = 1 + (accountXp / 200);
    const char *progTxt = TextFormat("NIVEAU %d   |   XP : %d   |   GEMMES : %d", level, accountXp, accountGems);
    DrawText(progTxt, (screenW - MeasureText(progTxt, 18)) / 2, (int)accBox.y + 48, 18, GOLD);

    // --- 2. SÉLECTION DES 3 CLASSES D'AVIONS ---
    DrawText("CHOISISSEZ VOTRE CLASSE D'AVION :", (screenW - MeasureText("CHOISISSEZ VOTRE CLASSE D'AVION :", 20)) / 2, 168, 20, GOLD);

    const char *classNames[3] = { "1. CHASSEUR", "2. INTERCEPTEUR", "3. BOMBARDIER" };
    const char *classStats[3] = {
        "100 PV | Vitesse Normale | Polyvalent Air & Sol",
        "75 PV  | Ultra Rapide    | Specialiste Chasse Air-Air",
        "150 PV | Blindage Lourd  | Bombes Lourdes Anti-Ville"
    };

    float classStartX = (float)(screenW - 620) / 2.0f;
    for (int c = 0; c < 3; c++) {
        Rectangle cBtn = { classStartX + c * 210.0f, 195.0f, 200.0f, 48.0f };
        bool isSel = (*selectedClass == (uint8_t)c);
        Color baseCol = isSel ? DARKGREEN : DARKGRAY;
        if (DrawButtonRect(classNames[c], cBtn, 18, baseCol, LIME)) {
            *selectedClass = (uint8_t)c;
            cmd = MENU_CMD_SELECT_CLASS;
        }
        if (isSel) DrawRectangleLinesEx(cBtn, 3.0f, GOLD);
    }

    const char *statLine = classStats[*selectedClass];
    DrawText(statLine, (screenW - MeasureText(statLine, 18)) / 2, 250, 18, SKYBLUE);

    // --- 3. ENCADRÉ D'ÉQUIPE / ROOM ---
    Rectangle roomBox = { (float)(screenW - 500) / 2.0f, 280.0f, 500.0f, 82.0f };
    DrawRectangleRec(roomBox, Fade(BLACK, 0.7f));
    DrawRectangleLinesEx(roomBox, 2.0f, (currentRoom != -1) ? LIME : GRAY);

    if (currentRoom != -1) {
        const char *codeTxt = TextFormat("CODE DE LA ROOM : %d", currentRoom + 1);
        DrawText(codeTxt, (screenW - MeasureText(codeTxt, 25)) / 2, 290, 25, GOLD);

        const char *countTxt = TextFormat("%d / 10 connectes sur cette room", connectedCount);
        DrawText(countTxt, (screenW - MeasureText(countTxt, 20)) / 2, 325, 20, LIME);
    } else {
        const char *noRoomTxt = "AUCUNE ROOM SELECTIONNEE";
        DrawText(noRoomTxt, (screenW - MeasureText(noRoomTxt, 22)) / 2, 294, 22, LIGHTGRAY);

        const char *subTxt = "Creez/rejoignez un code, ou cliquez sur PRET pour rejoindre une partie";
        DrawText(subTxt, (screenW - MeasureText(subTxt, 14)) / 2, 328, 14, GRAY);
    }

    // --- 4. BOUTON PRINCIPAL : PRÊT ---
    const char *readyLabel = (currentRoom != -1) ? "PRET (LANCER LA ROOM)" : "PRET (REJOINDRE UNE PARTIE)";
    if (DrawCenteredButton(readyLabel, 378.0f, 380, 54, 22, DARKGREEN, LIME)) {
        isTypingCode = false;
        isEditingUsername = false;
        *status = STATUS_PLAYING;
        return MENU_CMD_START_GAME;
    }

    // --- 5. BOUTONS DE ROOM ---
    if (currentRoom == -1) {
        if (!isTypingCode) {
            if (DrawCenteredButton("CREER UNE ROOM", 444.0f, 380, 48, 20, DARKBLUE, SKYBLUE)) {
                cmd = MENU_CMD_CREATE_ROOM;
            }
            if (DrawCenteredButton("REJOINDRE AVEC UN CODE", 502.0f, 380, 48, 20, DARKPURPLE, PURPLE)) {
                isTypingCode = true;
            }
        } else {
            if (!isEditingUsername) {
                if (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) typedRoomCode = 1;
                if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) typedRoomCode = 2;
                if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) typedRoomCode = 3;
                if (IsKeyPressed(KEY_FOUR)  || IsKeyPressed(KEY_KP_4)) typedRoomCode = 4;
            }

            const char *prompt = TextFormat("Choisissez le code de Room (1 a 4) : [ %d ]", typedRoomCode);
            DrawText(prompt, (screenW - MeasureText(prompt, 20)) / 2, 442, 20, GOLD);

            float startX = (float)(screenW - 230) / 2.0f;
            for (int c = 1; c <= 4; c++) {
                Rectangle numBtn = { startX + (c - 1) * 60.0f, 470.0f, 50.0f, 38.0f };
                Color bg = (typedRoomCode == c) ? GOLD : DARKGRAY;
                if (DrawButtonRect(TextFormat("%d", c), numBtn, 20, bg, SKYBLUE)) {
                    typedRoomCode = c;
                }
            }

            if (DrawCenteredButton("VALIDER LE CODE", 518.0f, 220, 42, 18, DARKGREEN, LIME)) {
                *outJoinRoomId = typedRoomCode - 1;
                isTypingCode = false;
                cmd = MENU_CMD_JOIN_ROOM;
            }
            if (DrawCenteredButton("ANNULER", 566.0f, 180, 36, 16, MAROON, RED)) {
                isTypingCode = false;
            }
        }
    } else {
        if (DrawCenteredButton("QUITTER LA ROOM", 444.0f, 380, 48, 20, MAROON, RED)) {
            cmd = MENU_CMD_LEAVE_ROOM;
        }
    }

    if (!isTypingCode && DrawCenteredButton("QUITTER LE JEU", 615.0f, 260, 45, 18, DARKGRAY, GRAY)) {
        *status = STATUS_QUIT;
    }

    return cmd;
}