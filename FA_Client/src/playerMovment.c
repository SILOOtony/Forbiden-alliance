#include "playerMovment.h"
#include "rlgl.h"
#include <math.h>
#include "raymath.h"

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

void UpdatePlayerMovement(PlayerState *plane, KeyBindings *controls, float dt) {
    if (!plane->is_alive) return;

    // --- 1. LECTURE DE LA SOURIS AVEC LIMITATION ET INERTIE ---
    Vector2 mouseDelta = GetMouseDelta();
    float mouseSensitivity = 0.0038f;

    // Calcul des taux cibles basés sur la souris
    float targetPitchRate = -mouseDelta.y * mouseSensitivity * 30.0f;
    float targetYawRate   = -mouseDelta.x * mouseSensitivity * 30.0f;

    // BRIDAGE : Empêche l'avion de tourner instantanément à une vitesse folle lors d'un coup de souris brusque
    float maxTurnRate = 2.0f; 
    targetPitchRate = Clamp(targetPitchRate, -maxTurnRate, maxTurnRate);
    targetYawRate   = Clamp(targetYawRate, -maxTurnRate, maxTurnRate);

    // Inertie / Lissage progressif (accélération et décélération des commandes)
    static float currentPitchRate = 0.0f;
    static float currentYawRate   = 0.0f;
    float smoothing = 4.5f; // Plus c'est bas, plus l'avion est "lourd"

    currentPitchRate += (targetPitchRate - currentPitchRate) * smoothing * dt;
    currentYawRate   += (targetYawRate - currentYawRate) * smoothing * dt;

    plane->pitch += currentPitchRate * dt;
    plane->yaw   += currentYawRate * dt;

    // Assistance clavier (A/D)
    float turnSpeed = 1.2f;
    if (IsKeyDown(controls->bindings[ACTION_LEFT]))  plane->yaw += turnSpeed * dt;
    if (IsKeyDown(controls->bindings[ACTION_RIGHT])) plane->yaw -= turnSpeed * dt;

    // Limite l'angle de piqué/cabré (~75 degrés max)
    if (plane->pitch > 1.3f)  plane->pitch = 1.3f;
    if (plane->pitch < -1.3f) plane->pitch = -1.3f;

    // --- 2. ROULIS AUTOMATIQUE FLUIDE ---
    float targetRoll = Clamp(currentYawRate * 0.7f, -0.85f, 0.85f);
    if (IsKeyDown(controls->bindings[ACTION_LEFT]))  targetRoll =  0.85f;
    if (IsKeyDown(controls->bindings[ACTION_RIGHT])) targetRoll =  -0.85f;

    plane->roll += (targetRoll - plane->roll) * 4.0f * dt;

    // --- 3. GESTION DE LA VITESSE ---
    static float currentSpeed = 45.0f;
    float targetSpeed = 45.0f;

    if (IsKeyDown(controls->bindings[ACTION_UP]))   targetSpeed = 75.0f;
    if (IsKeyDown(controls->bindings[ACTION_DOWN])) targetSpeed = 22.0f;

    currentSpeed += (targetSpeed - currentSpeed) * 3.0f * dt;

    // --- 4. DÉPLACEMENT 3D ---
    Vector3 forward = {
        -cosf(plane->pitch) * sinf(plane->yaw),
        sinf(plane->pitch),
        -cosf(plane->pitch) * cosf(plane->yaw)
    };

    plane->position.x += forward.x * currentSpeed * dt;
    plane->position.y += forward.y * currentSpeed * dt;
    plane->position.z += forward.z * currentSpeed * dt;
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