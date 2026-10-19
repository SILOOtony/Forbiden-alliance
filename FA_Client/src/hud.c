#include "hud.h"
#include "raymath.h"

void DrawCrossHair(const PlayerState *state) {
    Vector3 forward = {
        -cosf(state->pitch) * sinf(state->yaw),
         sinf(state->pitch),
        -cosf(state->pitch) * cosf(state->yaw)
    };

    Vector3 startPos = Vector3Add(state->position, (Vector3){ 0.0f, -0.3f, 0.0f });
    Vector3 endPos = Vector3Add(startPos, Vector3Scale(forward, 400.0f));
    DrawLine3D(startPos, endPos, RED);
}

void DrawWeaponSlots(PlayerState *state) {
    int slotWidth = 170;
    int slotHeight = 70;
    int spacing = 15;
    int startX = 700;
    int startY = 630;

    Vector2 mousePos = GetMousePosition();

    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        Weapon *w = &state->weapons[i];
        Rectangle slotRect = { startX + i * (slotWidth + spacing), startY, slotWidth, slotHeight };

        // Permet de sélectionner le slot en cliquant dessus à la souris
        if (CheckCollisionPointRec(mousePos, slotRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            state->current_slot = i;
        }

        bool isSelected = (state->current_slot == i);

        // Fond et bordure (dorée si le slot est sélectionné)
        DrawRectangleRec(slotRect, isSelected ? Fade(DARKBLUE, 0.85f) : Fade(BLACK, 0.6f));
        DrawRectangleLinesEx(slotRect, isSelected ? 3.0f : 1.0f, isSelected ? GOLD : GRAY);

        // Nom du slot et munitions
        DrawText(TextFormat("[%d] %s", i + 1, w->name), (int)slotRect.x + 10, (int)slotRect.y + 8, 16, isSelected ? GOLD : RAYWHITE);
        DrawText(TextFormat("%d / %d", w->ammo, w->max_ammo), (int)slotRect.x + 10, (int)slotRect.y + 30, 20, WHITE);

        // Barre de rechargement individuelle sous chaque slot
        if (w->ammo < w->max_ammo) {
            float ratio = w->reload_timer / w->reload_time;
            DrawRectangle((int)slotRect.x + 10, (int)slotRect.y + 55, slotWidth - 20, 6, DARKGRAY);
            DrawRectangle((int)slotRect.x + 10, (int)slotRect.y + 55, (int)((slotWidth - 20) * ratio), 6, ORANGE);
        }
    }
}

void DrawHpBar(const PlayerState *state) {
    int barWidth = 250;
    int barHeight = 25;
    int x = 20;
    int y = 660;

    DrawRectangle(x, y, barWidth, barHeight, DARKGRAY);
    int currentWidth = (state->hp * barWidth) / 100;
    DrawRectangle(x, y, currentWidth, barHeight, RED);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);
    DrawText(TextFormat("HP : %d / 100", state->hp), x + 65, y + 4, 18, RAYWHITE);
}

void DrawDeathScreen(void) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));
    DrawText("VOUS ETES MORT (CRASH)", 380, 300, 40, RED);
    DrawText("Appuyez sur [R] pour reapparaitre", 430, 360, 25, RAYWHITE);
}