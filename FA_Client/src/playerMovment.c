#include "playerMovment.h"
#include "rlgl.h"
#include <math.h>

void InitPlayerState(PlayerState *state) {
    state->position = (Vector3){ 0.0f, 80.0f, 250.0f };
    state->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    state->pitch = 0.0f;
    state->yaw = 0.0f;
    state->roll = 0.0f;
    state->speed = 60.0f;
    state->hp = 100;
    state->is_alive = true;

    // Initialisation des 3 slots d'armes et sélection du slot 1 par défaut
    InitWeaponSlots(state->weapons);
    state->current_slot = 0;
}

void UpdatePlayerMovement(PlayerState *state, KeyBindings *controls, float dt) {
    float turn_speed = 1.5f;  // Vitesse de virage
    float pitch_speed = 1.2f; // Vitesse de montée/descente
    float target_roll = 0.0f; // Inclinaison cible des ailes

    // Monter / Piquer du nez (W / S)
    if (IsKeyDown(controls->bindings[ACTION_UP])) {
        state->pitch += pitch_speed * dt;
    }
    if (IsKeyDown(controls->bindings[ACTION_DOWN])) {
        state->pitch -= pitch_speed * dt;
    }

    // Bloquer l'angle vertical pour éviter que la caméra se retourne complètement
    if (state->pitch > 1.2f) state->pitch = 1.2f;
    if (state->pitch < -1.2f) state->pitch = -1.2f;

    // Tourner à gauche / droite (A / D) + incliner les ailes
    if (IsKeyDown(controls->bindings[ACTION_LEFT])) {
        state->yaw += turn_speed * dt;
        target_roll = 0.6f; // Incline à gauche
    }
    if (IsKeyDown(controls->bindings[ACTION_RIGHT])) {
        state->yaw -= turn_speed * dt;
        target_roll = -0.6f; // Incline à droite
    }

    // Transition fluide du roulis (les ailes reviennent à plat quand on lâche la touche)
    state->roll += (target_roll - state->roll) * 8.0f * dt;

    // Calcul du vecteur de direction 3D à partir des angles pitch et yaw
    Vector3 forward = {
        -cosf(state->pitch) * sinf(state->yaw),
         sinf(state->pitch),
        -cosf(state->pitch) * cosf(state->yaw)
    };

    // Mise à jour de la vitesse et de la position
    state->velocity.x = forward.x * state->speed;
    state->velocity.y = forward.y * state->speed;
    state->velocity.z = forward.z * state->speed;

    state->position.x += state->velocity.x * dt;
    state->position.y += state->velocity.y * dt;
    state->position.z += state->velocity.z * dt;
}

void UpdatePlaneCamera(Camera3D *camera, const PlayerState *state) {
    // Vecteur de direction de l'avion
    Vector3 forward = {
        -cosf(state->pitch) * sinf(state->yaw),
         sinf(state->pitch),
        -cosf(state->pitch) * cosf(state->yaw)
    };

    // La caméra se place à 18 unités derrière l'avion et 5 unités au-dessus
    float dist = 18.0f;
    float height = 5.0f;

    camera->position.x = state->position.x - forward.x * dist;
    camera->position.y = state->position.y - forward.y * dist + height;
    camera->position.z = state->position.z - forward.z * dist;

    // La caméra regarde légèrement devant l'avion
    camera->target.x = state->position.x + forward.x * 10.0f;
    camera->target.y = state->position.y + forward.y * 10.0f;
    camera->target.z = state->position.z + forward.z * 10.0f;
}

void DrawPlanePlayer(const PlayerState *state) {
    rlPushMatrix();
        // 1. On se place à la position de l'avion
        rlTranslatef(state->position.x, state->position.y, state->position.z);

        // 2. On applique les rotations 3D (converties en degrés avec RAD2DEG)
        rlRotatef(state->yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
        rlRotatef(state->pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
        rlRotatef(state->roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

        // 3. Dessin du chasseur temporaire
        DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 1.2f, 0.8f, 5.0f, RED);                 // Fuselage
        DrawCube((Vector3){ 0.0f, 0.0f, 0.2f }, 7.0f, 0.15f, 1.5f, DARKGRAY);           // Ailes principales
        DrawCube((Vector3){ 0.0f, 0.8f, 2.0f }, 0.2f, 1.2f, 1.0f, RED);                 // Dérive verticale (queue)
        DrawCube((Vector3){ 0.0f, 0.4f, -0.8f }, 0.8f, 0.5f, 1.5f, SKYBLUE);            // Cockpit
    rlPopMatrix();
}