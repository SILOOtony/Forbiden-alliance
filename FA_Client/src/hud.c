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
        Rectangle slotRect = { (float)(startX + i * (slotWidth + spacing)), (float)startY, (float)slotWidth, (float)slotHeight };

        // Empêche le clic de tir en vol de cliquer sur les slots quand le curseur est masqué
        if (!IsCursorHidden() && CheckCollisionPointRec(mousePos, slotRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            state->current_slot = i;
        }

        bool isSelected = (state->current_slot == i);
        DrawRectangleRec(slotRect, isSelected ? Fade(DARKBLUE, 0.85f) : Fade(BLACK, 0.6f));
        DrawRectangleLinesEx(slotRect, isSelected ? 3.0f : 1.0f, isSelected ? GOLD : GRAY);

        DrawText(TextFormat("[%d] %s", i + 1, w->name), (int)slotRect.x + 10, (int)slotRect.y + 8, 16, isSelected ? GOLD : RAYWHITE);
        DrawText(TextFormat("%d / %d", w->ammo, w->max_ammo), (int)slotRect.x + 10, (int)slotRect.y + 30, 20, WHITE);

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

    int maxHp = (state->max_hp > 0) ? state->max_hp : 100;
    DrawRectangle(x, y, barWidth, barHeight, DARKGRAY);
    int currentWidth = (state->hp * barWidth) / maxHp;
    if (currentWidth < 0) currentWidth = 0;
    if (currentWidth > barWidth) currentWidth = barWidth;

    DrawRectangle(x, y, currentWidth, barHeight, RED);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);
    DrawText(TextFormat("HP : %d / %d", state->hp, maxHp), x + 55, y + 4, 18, RAYWHITE);
    DrawText(TextFormat("Avions restants : %d / 3", state->plane_respawns_left), x, y - 26, 20, GOLD);
}

void DrawCityTopBar(const GameMap *map, uint8_t currentRound, uint16_t timeLeft, bool isAttacker) {
    int screenW = GetScreenWidth();
    int barW = 520;
    int barH = 30;
    int barX = (screenW - barW) / 2;
    int barY = 18;

    float pct = GetCityHpPercent(map);
    int remaining = GetCityRemainingBuildings(map);

    DrawRectangle(barX, barY, barW, barH, Fade(BLACK, 0.75f));
    int fillW = (int)((pct / 100.0f) * (float)barW);
    Color barColor = (pct > 50.0f) ? LIME : ((pct > 25.0f) ? ORANGE : RED);
    DrawRectangle(barX, barY, fillW, barH, barColor);
    DrawRectangleLines(barX, barY, barW, barH, RAYWHITE);

    const char *cityTxt = TextFormat("VILLE : %.1f%% (%d batiments restants)", pct, remaining);
    int txtW = MeasureText(cityTxt, 20);
    DrawText(cityTxt, barX + (barW - txtW) / 2, barY + 5, 20, WHITE);

    int mins = timeLeft / 60;
    int secs = timeLeft % 60;
    const char *roleTxt = isAttacker ? "ROLE : ATTAQUANT (Detruisez la ville !)" : "ROLE : DEFENSEUR (Protegez la ville !)";
    const char *subTxt = TextFormat("MANCHE %d / 2   |   %02d:%02d   |   %s",
                                    (currentRound <= 2) ? currentRound : 2, mins, secs, roleTxt);
    int subW = MeasureText(subTxt, 18);
    DrawRectangle((screenW - subW) / 2 - 10, barY + barH + 4, subW + 20, 24, Fade(BLACK, 0.6f));
    DrawText(subTxt, (screenW - subW) / 2, barY + barH + 7, 18, isAttacker ? ORANGE : SKYBLUE);
}

void DrawDeathScreen(const PlayerState *state) {
    int screenW = GetScreenWidth();
    DrawRectangle(0, 0, screenW, GetScreenHeight(), Fade(BLACK, 0.65f));

    const char *deadTxt = "VOUS ETES MORT (CRASH)";
    DrawText(deadTxt, (screenW - MeasureText(deadTxt, 40)) / 2, 260, 40, RED);

    if (state->plane_respawns_left > 0) {
        const char *respTxt = TextFormat("Appuyez sur [R] pour reapparaitre en AVION (%d / 3 restants)", state->plane_respawns_left);
        DrawText(respTxt, (screenW - MeasureText(respTxt, 26)) / 2, 330, 26, RAYWHITE);

        const char *infoTxt = "(Vous devez utiliser vos 3 avions avant de devenir un Drone)";
        DrawText(infoTxt, (screenW - MeasureText(infoTxt, 20)) / 2, 370, 20, GRAY);
    } else {
        const char *droneTxt = "Plus d'avions ! Appuyez sur [E] pour vous reincarner dans un DRONE allie";
        DrawText(droneTxt, (screenW - MeasureText(droneTxt, 26)) / 2, 340, 26, GOLD);
    }
}

void DrawMatchEndScreen(const uint16_t buildingsDestroyed[2], const uint32_t cityDamage[2], uint16_t timeLeft) {
    int screenW = GetScreenWidth();
    DrawRectangle(0, 0, screenW, GetScreenHeight(), Fade(BLACK, 0.8f));

    const char *title = "FIN DE LA PARTIE (2 MANCHES)";
    DrawText(title, (screenW - MeasureText(title, 44)) / 2, 160, 44, GOLD);

    int winner = -1;
    if (buildingsDestroyed[0] > buildingsDestroyed[1]) winner = 0;
    else if (buildingsDestroyed[1] > buildingsDestroyed[0]) winner = 1;
    else if (cityDamage[0] > cityDamage[1]) winner = 0;
    else if (cityDamage[1] > cityDamage[0]) winner = 1;

    const char *winTxt = (winner == 0) ? "VICTOIRE DE L'EQUIPE 1 !" :
                         ((winner == 1) ? "VICTOIRE DE L'EQUIPE 2 !" : "EGALITE PARFAITE !");
    Color winCol = (winner == 0) ? LIME : ((winner == 1) ? ORANGE : RAYWHITE);
    DrawText(winTxt, (screenW - MeasureText(winTxt, 38)) / 2, 235, 38, winCol);

    const char *t1 = TextFormat("Equipe 1 : %u batiments detruits  (%u degats infliges)", buildingsDestroyed[0], cityDamage[0]);
    const char *t2 = TextFormat("Equipe 2 : %u batiments detruits  (%u degats infliges)", buildingsDestroyed[1], cityDamage[1]);
    DrawText(t1, (screenW - MeasureText(t1, 24)) / 2, 320, 24, LIME);
    DrawText(t2, (screenW - MeasureText(t2, 24)) / 2, 365, 24, RED);

    const char *nextTxt = TextFormat("Nouvelle partie dans %u secondes...", timeLeft);
    DrawText(nextTxt, (screenW - MeasureText(nextTxt, 20)) / 2, 450, 20, LIGHTGRAY);
}