#include "menu.h"
#include <math.h>
#include "raylib.h"

void UpdateMenuCamera(Camera3D *camera, float angle) {
    float radius = 320.0f;
    camera->position.x = cosf(angle) * radius;
    camera->position.y = 140.0f;
    camera->position.z = sinf(angle) * radius;
    camera->target = (Vector3){ 0.0f, 30.0f, 0.0f };
}

static bool DrawCenteredButton(const char *text, float yPos, int width, int height, int fontSize) {
    Rectangle bounds = {
        (float)(GetScreenWidth() - width) / 2.0f,
        yPos,
        (float)width,
        (float)height
    };

    Vector2 mousePos = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mousePos, bounds);

    DrawRectangleRec(bounds, isHovered ? SKYBLUE : LIGHTGRAY);
    DrawRectangleLinesEx(bounds, 2.0f, DARKGRAY);

    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(bounds.x + (bounds.width - textWidth) / 2.0f);
    int textY = (int)(bounds.y + (bounds.height - fontSize) / 2.0f);
    DrawText(text, textX, textY, fontSize, isHovered ? WHITE : DARKGRAY);

    return (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
}

void DrawMenu(GameStatus *status) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.45f));

    const char *title = "FORBIDDEN ALLIANCE";
    int titleSize = 70;
    int titleWidth = MeasureText(title, titleSize);
    int titleX = (GetScreenWidth() - titleWidth) / 2;
    DrawText(title, titleX + 4, 154, titleSize, BLACK);
    DrawText(title, titleX, 150, titleSize, GOLD);

    if (*status == STATUS_PAUSED) {
        const char *pauseText = "- JEU EN PAUSE -";
        int pauseWidth = MeasureText(pauseText, 24);
        DrawText(pauseText, (GetScreenWidth() - pauseWidth) / 2, 240, 24, RAYWHITE);

        if (DrawCenteredButton("REPRENDRE", 300.0f, 280, 55, 22)) {
            *status = STATUS_PLAYING;
        }
        if (DrawCenteredButton("CHANGER DE SALON", 370.0f, 280, 55, 22)) {
            *status = STATUS_LOBBY;
        }
        if (DrawCenteredButton("QUITTER", 440.0f, 280, 55, 22)) {
            *status = STATUS_QUIT;
        }
    } else {
        if (DrawCenteredButton("TROUVER UNE PARTIE", 320.0f, 280, 60, 22)) {
            *status = STATUS_LOBBY;
        }
        if (DrawCenteredButton("QUITTER", 400.0f, 280, 60, 22)) {
            *status = STATUS_QUIT;
        }
    }
}

int DrawLobby(GameStatus *status, const uint8_t room_players[MAX_ROOMS]) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));

    const char *title = "SELECTION DU SALON (5v5)";
    int titleSize = 45;
    int titleWidth = MeasureText(title, titleSize);
    DrawText(title, (GetScreenWidth() - titleWidth) / 2, 110, titleSize, GOLD);

    const char *sub = "Choisissez le meme salon que vos amis pour jouer ensemble";
    int subWidth = MeasureText(sub, 20);
    DrawText(sub, (GetScreenWidth() - subWidth) / 2, 165, 20, LIGHTGRAY);

    int chosenRoom = -1;
    for (int r = 0; r < MAX_ROOMS; r++) {
        uint8_t count = room_players[r];
        const char *btnText = TextFormat("SALON #%d   -   %d / %d JOUEURS", r + 1, count, MAX_CLIENTS);
        if (DrawCenteredButton(btnText, 220.0f + r * 70.0f, 420, 55, 22)) {
            chosenRoom = r;
            *status = STATUS_PLAYING;
        }
    }

    if (DrawCenteredButton("RETOUR AU MENU", 530.0f, 240, 50, 20)) {
        *status = STATUS_MENU;
    }

    return chosenRoom;
}