#include "hud.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void DrawCrossHair(const PlayerState *plane) {
    (void)plane;
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;

    int gap   = 5;  // Espace libre au centre
    int len   = 10; // Longueur des 4 branches (en pixels)
    int thick = 2;  // Épaisseur

    // 1. Contours noirs (lisibilité parfaite sur le ciel comme sur les bâtiments)
    DrawRectangle(cx - gap - len - 1, cy - thick / 2 - 1, len + 2, thick + 2, BLACK);
    DrawRectangle(cx + gap - 1,       cy - thick / 2 - 1, len + 2, thick + 2, BLACK);
    DrawRectangle(cx - thick / 2 - 1, cy - gap - len - 1, thick + 2, len + 2, BLACK);
    DrawRectangle(cx - thick / 2 - 1, cy + gap - 1,       thick + 2, len + 2, BLACK);
    DrawRectangle(cx - 2,             cy - 2,             4,         4,       BLACK);

    // 2. Cœur Vert Fluo Valorant + Point central Blanc
    DrawRectangle(cx - gap - len, cy - thick / 2, len, thick, LIME);
    DrawRectangle(cx + gap,       cy - thick / 2, len, thick, LIME);
    DrawRectangle(cx - thick / 2, cy - gap - len, thick, len, LIME);
    DrawRectangle(cx - thick / 2, cy + gap,       thick, len, LIME);
    DrawRectangle(cx - 1,         cy - 1,         2,     2,   WHITE);
}

void DrawHitmarker(Vector2 crosshairScreenPos, float hitTimer, bool isKillHit) {
    if (hitTimer <= 0.0f) return;

    float maxTime = isKillHit ? 0.35f : 0.18f;
    float alpha = hitTimer / maxTime;
    if (alpha > 1.0f) alpha = 1.0f;

    float gap = isKillHit ? 7.0f : 5.0f;
    float len = isKillHit ? 16.0f : 11.0f;
    float thick = isKillHit ? 2.8f : 2.0f;
    Color col = isKillHit ? Fade(RED, alpha) : Fade(WHITE, alpha);
    Color outline = Fade(BLACK, alpha * 0.85f);

    float cx = crosshairScreenPos.x;
    float cy = crosshairScreenPos.y;

    int dirs[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
    for (int i = 0; i < 4; i++) {
        Vector2 p1 = { cx + dirs[i][0] * gap, cy + dirs[i][1] * gap };
        Vector2 p2 = { cx + dirs[i][0] * len, cy + dirs[i][1] * len };
        DrawLineEx(p1, p2, thick + 1.6f, outline);
        DrawLineEx(p1, p2, thick, col);
    }
}

void AddKillfeedEntry(KillfeedEntry feed[], const char *text, Color color) {
    for (int i = MAX_KILLFEED - 1; i > 0; i--) {
        feed[i] = feed[i - 1];
    }
    strncpy(feed[0].text, text, sizeof(feed[0].text) - 1);
    feed[0].text[sizeof(feed[0].text) - 1] = '\0';
    feed[0].color = color;
    feed[0].timer = 4.5f;
    feed[0].active = true;
}

void UpdateKillfeed(KillfeedEntry feed[], float dt) {
    for (int i = 0; i < MAX_KILLFEED; i++) {
        if (feed[i].active) {
            feed[i].timer -= dt;
            if (feed[i].timer <= 0.0f) {
                feed[i].active = false;
            }
        }
    }
}

void DrawKillfeed(const KillfeedEntry feed[]) {
    int rightEdge = GetScreenWidth() - 18;
    int startY = 232; // Juste sous le Radar Tactique
    int row = 0;

    for (int i = 0; i < MAX_KILLFEED; i++) {
        if (!feed[i].active) continue;

        float alpha = (feed[i].timer < 1.0f) ? feed[i].timer : 1.0f;
        int fontSize = 13;
        int tw = MeasureText(feed[i].text, fontSize);
        int boxW = tw + 18;
        int boxH = 22;
        int x = rightEdge - boxW;
        int y = startY + row * (boxH + 5);

        DrawRectangle(x, y, boxW, boxH, Fade((Color){ 10, 14, 22, 255 }, 0.78f * alpha));
        DrawRectangle(x, y, 3, boxH, Fade(feed[i].color, alpha));
        DrawRectangleLines(x, y, boxW, boxH, Fade(feed[i].color, 0.45f * alpha));
        DrawText(feed[i].text, x + 10, y + 4, fontSize, Fade(feed[i].color, alpha));
        row++;
    }
}

void DrawRoundBanner(uint8_t currentRound, bool isAttacker, float bannerTimer) {
    if (bannerTimer <= 0.0f || currentRound > 2) return;

    float alpha = (bannerTimer < 0.8f) ? (bannerTimer / 0.8f) : 1.0f;
    int sw = GetScreenWidth();
    int boxW = 480;
    int boxH = 58;
    int x = (sw - boxW) / 2;
    int y = 108;

    Color accent = isAttacker ? RED : SKYBLUE;
    DrawRectangle(x, y, boxW, boxH, Fade((Color){ 8, 12, 20, 255 }, 0.88f * alpha));
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)boxW, (float)boxH }, 2.0f, Fade(accent, alpha));

    const char *title = (currentRound == 1)
        ? "MANCHE 1 / 2  -  DEBUT DE L'OPERATION"
        : "MANCHE 2 / 2  -  CHANGEMENT DE CAMP !";
    const char *sub = isAttacker
        ? "VOUS ETES ATTAQUANT : DETRUISEZ LA VILLE ENNEMIE"
        : "VOUS ETES DEFENSEUR : PROTEGEZ LA VILLE ET SES DRONES";

    int tw1 = MeasureText(title, 16);
    int tw2 = MeasureText(sub, 13);

    DrawText(title, x + (boxW - tw1) / 2, y + 10, 16, Fade(GOLD, alpha));
    DrawText(sub,   x + (boxW - tw2) / 2, y + 34, 13, Fade(WHITE, alpha));
}

void DrawWeaponSlots(const PlayerState *plane) {
    int slotWidth = 148;
    int slotHeight = 62;
    int spacing = 10;
    int count = plane->is_drone ? 1 : MAX_WEAPON_SLOTS;
    int totalWidth = count * slotWidth + (count - 1) * spacing;
    int startX = (GetScreenWidth() - totalWidth) / 2;
    int y = GetScreenHeight() - slotHeight - 16;

    for (int i = 0; i < count; i++) {
        int x = startX + i * (slotWidth + spacing);
        bool selected = (plane->current_slot == i);
        Weapon w = plane->weapons[i];

        Color bg = selected ? Fade((Color){ 20, 35, 52, 255 }, 0.92f) : Fade((Color){ 12, 16, 24, 255 }, 0.80f);
        Color border = (i == 3) ? GOLD : (selected ? SKYBLUE : GRAY);

        DrawRectangle(x, y, slotWidth, slotHeight, bg);
        DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)slotWidth, (float)slotHeight }, selected ? 2.5f : 1.2f, border);

        const char *keyLabel = (i == 3) ? "[4 / F]" : TextFormat("[%d]", i + 1);
        DrawText(keyLabel, x + 8, y + 6, 13, border);
        DrawText(w.name, x + 8, y + 22, 14, WHITE);

        if (w.ammo > 0) {
            DrawText(TextFormat("%d / %d", w.ammo, w.max_ammo), x + 8, y + 40, 15, LIME);
        } else {
            DrawText("RECHARGE...", x + 8, y + 40, 14, ORANGE);
        }

        if (w.ammo < w.max_ammo && w.reload_time > 0.0f) {
            float ratio = w.reload_timer / w.reload_time;
            if (ratio > 1.0f) ratio = 1.0f;
            DrawRectangle(x + 4, y + slotHeight - 5, (int)((slotWidth - 8) * ratio), 3, SKYBLUE);
        }
    }
}

void DrawHpBar(const PlayerState *plane) {
    int barW = 240;
    int barH = 22;
    int x = 20;
    int y = GetScreenHeight() - 44;

    float ratio = (plane->max_hp > 0) ? ((float)plane->hp / (float)plane->max_hp) : 0.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    Color hpCol = (ratio > 0.5f) ? LIME : ((ratio > 0.25f) ? ORANGE : RED);

    DrawRectangle(x, y, barW, barH, Fade(BLACK, 0.75f));
    DrawRectangle(x + 2, y + 2, (int)((barW - 4) * ratio), barH - 4, hpCol);
    DrawRectangleLines(x, y, barW, barH, WHITE);
    DrawText(TextFormat("PV : %d / %d  |  Vies Avion : %d", plane->hp, plane->max_hp, plane->plane_respawns_left),
             x + 10, y + 3, 15, WHITE);
}

void DrawCityTopBar(const GameMap *map, uint8_t current_round, uint16_t time_left, bool is_attacker) {
    int sw = GetScreenWidth();
    int boxW = 460;
    int boxH = 54;
    int boxX = (sw - boxW) / 2;
    int boxY = 8;

    int barW = boxW - 24;
    int barH = 20;
    int barX = boxX + 12;
    int barY = boxY + 6;

    float pct = GetCityHpPercent(map);
    int remBld = GetCityRemainingBuildings(map);
    int mins = time_left / 60;
    int secs = time_left % 60;

    DrawRectangle(boxX, boxY, boxW, boxH, Fade((Color){ 10, 14, 22, 255 }, 0.84f));
    DrawRectangleLines(boxX, boxY, boxW, boxH, is_attacker ? RED : SKYBLUE);

    DrawRectangle(barX, barY, barW, barH, DARKGRAY);
    DrawRectangle(barX, barY, (int)(barW * (pct / 100.0f)), barH, (pct > 40.0f) ? SKYBLUE : ORANGE);
    DrawRectangleLines(barX, barY, barW, barH, WHITE);

    const char *cityTxt = TextFormat("VILLE : %s (%.0f%% - %d batiments)", GetCityName(map->city_type), pct, remBld);
    int cw = MeasureText(cityTxt, 13);
    DrawText(cityTxt, boxX + (boxW - cw) / 2, barY + 4, 13, BLACK);

    const char *roleTxt = is_attacker ? "ATTAQUANT (Detruire la ville)" : "DEFENSEUR (Proteger la ville)";
    const char *subTxt  = TextFormat("MANCHE %u/2  |  %02d:%02d  |  %s",
                                     (current_round <= 2) ? current_round : 2, mins, secs, roleTxt);
    int swTxt = MeasureText(subTxt, 13);
    DrawText(subTxt, boxX + (boxW - swTxt) / 2, barY + 25, 13,
             is_attacker ? (Color){ 255, 115, 115, 255 } : LIME);
}

void DrawLockOnHUD(bool has_target, Vector2 screen_pos, float lock_progress, bool incoming_missile) {
    int sw = GetScreenWidth();

    if (has_target && screen_pos.x > 0 && screen_pos.x < sw && screen_pos.y > 0 && screen_pos.y < GetScreenHeight()) {
        bool locked = (lock_progress >= 1.0f);
        Color col = locked ? RED : GOLD;
        float size = 24.0f - 9.0f * lock_progress;

        DrawRectangleLinesEx((Rectangle){ screen_pos.x - size, screen_pos.y - size, size * 2.0f, size * 2.0f }, 1.6f, col);
        DrawCircleLines((int)screen_pos.x, (int)screen_pos.y, size * 1.15f, col);

        const char *lbl = locked ? "LOCKED" : TextFormat("%.0f%%", lock_progress * 100.0f);
        int fontSize = 11;
        int tw = MeasureText(lbl, fontSize);
        DrawRectangle((int)screen_pos.x - tw / 2 - 3, (int)(screen_pos.y + size + 4), tw + 6, 15, Fade(BLACK, 0.65f));
        DrawText(lbl, (int)screen_pos.x - tw / 2, (int)(screen_pos.y + size + 6), fontSize, col);
    }

    if (incoming_missile) {
        const char *warn = "ALERTE MISSILE - [F] LEURRES";
        int fontSize = 15;
        int tw = MeasureText(warn, fontSize);
        DrawRectangle((sw - tw) / 2 - 10, 68, tw + 20, 24, Fade(RED, 0.85f));
        DrawRectangleLines((sw - tw) / 2 - 10, 68, tw + 20, 24, YELLOW);
        DrawText(warn, (sw - tw) / 2, 73, fontSize, WHITE);
    }
}

void DrawTacticalRadar(
    const PlayerState *localPlane,
    const ServerPacket *serverData,
    const PlayerState remotePlayers[],
    const Drone drones[],
    int maxDrones,
    float flareActiveTimer
) {
    const float innerDogfightRange = 250.0f;
    const float outerRadarRange    = 450.0f;
    const float radarRadiusPx      = 84.0f;

    int cx = GetScreenWidth() - (int)radarRadiusPx - 22;
    int cy = (int)radarRadiusPx + 22;
    Vector2 center = { (float)cx, (float)cy };

    DrawCircleV(center, radarRadiusPx + 4.0f, Fade((Color){ 8, 12, 20, 255 }, 0.88f));
    DrawCircleV(center, radarRadiusPx, Fade((Color){ 16, 26, 38, 255 }, 0.85f));

    float r250Px = radarRadiusPx * (innerDogfightRange / outerRadarRange);
    DrawCircleLines(cx, cy, r250Px, Fade(SKYBLUE, 0.40f));
    DrawCircleLines(cx, cy, radarRadiusPx, (flareActiveTimer > 0.0f) ? GOLD : SKYBLUE);

    DrawLine(cx - (int)radarRadiusPx, cy, cx + (int)radarRadiusPx, cy, Fade(SKYBLUE, 0.18f));
    DrawLine(cx, cy - (int)radarRadiusPx, cx, cy + (int)radarRadiusPx, Fade(SKYBLUE, 0.18f));

    Vector2 fovLeft  = { cx - sinf(30.0f * DEG2RAD) * radarRadiusPx, cy - cosf(30.0f * DEG2RAD) * radarRadiusPx };
    Vector2 fovRight = { cx + sinf(30.0f * DEG2RAD) * radarRadiusPx, cy - cosf(30.0f * DEG2RAD) * radarRadiusPx };
    DrawLineV(center, fovLeft, Fade(WHITE, 0.30f));
    DrawLineV(center, fovRight, Fade(WHITE, 0.30f));

    float fwdX = -sinf(localPlane->yaw);
    float fwdZ = -cosf(localPlane->yaw);
    float rightX =  cosf(localPlane->yaw);
    float rightZ = -sinf(localPlane->yaw);

    for (int d = 0; d < maxDrones; d++) {
        if (!drones[d].is_alive) continue;
        float dx = drones[d].pos.x - localPlane->position.x;
        float dz = drones[d].pos.z - localPlane->position.z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (dist > innerDogfightRange) continue;

        float localRight = dx * rightX + dz * rightZ;
        float localFwd   = dx * fwdX   + dz * fwdZ;

        float px = cx + (localRight / outerRadarRange) * radarRadiusPx;
        float py = cy - (localFwd   / outerRadarRange) * radarRadiusPx;

        bool isAllyDrone = (drones[d].is_attacker == localPlane->is_attacker);
        DrawCircle((int)px, (int)py, 2.2f, isAllyDrone ? Fade(LIME, 0.55f) : Fade(ORANGE, 0.65f));
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i == serverData->your_id || !serverData->connected[i] || !remotePlayers[i].is_alive) continue;

        bool isAlly = (remotePlayers[i].team == localPlane->team);
        float dx = remotePlayers[i].position.x - localPlane->position.x;
        float dz = remotePlayers[i].position.z - localPlane->position.z;
        float dist = sqrtf(dx * dx + dz * dz);

        if (!isAlly && dist > outerRadarRange) continue;

        float localRight = dx * rightX + dz * rightZ;
        float localFwd   = dx * fwdX   + dz * fwdZ;

        float normDist = dist / outerRadarRange;
        if (normDist > 0.94f) {
            float scale = (0.94f * outerRadarRange) / (dist > 0.001f ? dist : 1.0f);
            localRight *= scale;
            localFwd   *= scale;
        }

        float px = cx + (localRight / outerRadarRange) * radarRadiusPx;
        float py = cy - (localFwd   / outerRadarRange) * radarRadiusPx;

        Color dotCol = isAlly ? LIME : RED;
        DrawCircle((int)px, (int)py, 5.0f, dotCol);
        DrawCircleLines((int)px, (int)py, 5.5f, BLACK);

        float relYaw = remotePlayers[i].yaw - localPlane->yaw;
        float dirX = -sinf(relYaw) * 9.0f;
        float dirY = -cosf(relYaw) * 9.0f;
        DrawLineEx((Vector2){ px, py }, (Vector2){ px + dirX, py + dirY }, 1.8f, dotCol);
    }

    DrawTriangle(
        (Vector2){ (float)cx, (float)(cy - 6) },
        (Vector2){ (float)(cx - 5), (float)(cy + 5) },
        (Vector2){ (float)(cx + 5), (float)(cy + 5) },
        WHITE
    );

    DrawText("250m", cx + 4, cy - (int)r250Px + 2, 11, Fade(SKYBLUE, 0.8f));
    DrawText("RADAR", cx - 22, cy + (int)radarRadiusPx + 8, 12, SKYBLUE);
    if (flareActiveTimer > 0.0f) {
        DrawText(TextFormat("LEURRES %.1fs", flareActiveTimer), cx - 42, cy + (int)radarRadiusPx + 22, 12, GOLD);
    }
}

void DrawDeathScreen(const PlayerState *plane) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.55f));

    const char *title = "AVION DETRUIT";
    int tw = MeasureText(title, 32);
    DrawText(title, (sw - tw) / 2, sh / 2 - 55, 32, RED);

    const char *sub = (plane->plane_respawns_left > 0)
        ? TextFormat("Appuyez sur [R] pour deployer un nouvel avion (%d restants)", plane->plane_respawns_left)
        : "Plus d'avions disponibles ! Appuyez sur [E] pour controler un DRONE";
    int sw2 = MeasureText(sub, 18);
    DrawText(sub, (sw - sw2) / 2, sh / 2, 18, RAYWHITE);
}

void DrawMatchEndScreen(const uint16_t buildings_destroyed[2], const uint32_t city_damage[2], uint16_t time_left) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.78f));

    int winner = -1;
    if (buildings_destroyed[0] > buildings_destroyed[1]) winner = 0;
    else if (buildings_destroyed[1] > buildings_destroyed[0]) winner = 1;
    else if (city_damage[0] > city_damage[1]) winner = 0;
    else if (city_damage[1] > city_damage[0]) winner = 1;

    const char *title = (winner == -1) ? "EGALITE PARFAITE !" :
                        ((winner == 0) ? "VICTOIRE DE L'EQUIPE 1 !" : "VICTOIRE DE L'EQUIPE 2 !");
    int tw = MeasureText(title, 28);
    DrawText(title, (sw - tw) / 2, sh / 2 - 80, 28, GOLD);

    DrawText(TextFormat("Equipe 1 : %u batiments detruits (%u degats)", buildings_destroyed[0], city_damage[0]),
             sw / 2 - 200, sh / 2 - 20, 18, LIME);
    DrawText(TextFormat("Equipe 2 : %u batiments detruits (%u degats)", buildings_destroyed[1], city_damage[1]),
             sw / 2 - 200, sh / 2 + 12, 18, RED);

    DrawText(TextFormat("Retour au salon dans %u s...", time_left), sw / 2 - 110, sh / 2 + 65, 18, SKYBLUE);
}