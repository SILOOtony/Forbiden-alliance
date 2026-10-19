#include "menu.h"
#include <math.h>
#include "raylib.h"

void UpdateMenuCamera(Camera3D *camera, float angle) {
    float radius = 320.0f;
    camera->position.x = cosf(angle) * radius;
    camera->position.y = 140.0f;
    camera->position.z = sinf(angle) * radius;
    camera->target = (Vector3){ 0.0f, 30.0f, 0.0f }; // Regarde le centre de la ville
}

// Fonction utilitaire interne pour dessiner un bouton centré et retourner true si cliqué
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
    // Voile sombre semi-transparent par-dessus la ville 3D
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.45f));

    // Titre centré avec ombre portée
    const char *title = "FORBIDDEN ALLIANCE";
    int titleSize = 70;
    int titleWidth = MeasureText(title, titleSize);
    int titleX = (GetScreenWidth() - titleWidth) / 2;

    DrawText(title, titleX + 4, 164, titleSize, BLACK);
    DrawText(title, titleX, 160, titleSize, GOLD);

    // Sous-titre si le jeu est en pause
    if (*status == STATUS_PAUSED) {
        const char *pauseText = "- JEU EN PAUSE -";
        int pauseWidth = MeasureText(pauseText, 24);
        DrawText(pauseText, (GetScreenWidth() - pauseWidth) / 2, 250, 24, RAYWHITE);
    }

    // Bouton 1 : Start / Reprendre
    const char *playLabel = (*status == STATUS_PAUSED) ? "REPRENDRE" : "START GAME";
    if (DrawCenteredButton(playLabel, 330.0f, 240, 60, 24)) {
        *status = STATUS_PLAYING;
    }

    // Bouton 2 : Quitter
    if (DrawCenteredButton("QUITTER", 410.0f, 240, 60, 24)) {
        *status = STATUS_QUIT;
    }
}