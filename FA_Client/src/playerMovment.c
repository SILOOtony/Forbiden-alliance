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
    state->is_drone = false;

    InitWeaponSlots(state->weapons);
    state->current_slot = 0;
}

void ReincarnateAsDrone(PlayerState *state, Vector3 dronePos) {
    state->position = dronePos;
    state->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    state->pitch = 0.0f;
    state->yaw = 0.0f;
    state->roll = 0.0f;
    state->speed = 35.0f;
    state->hp = 100;
    state->is_alive = true;
    state->is_drone = true;

    // Équipe le drone de petits projectiles rapides sur les 3 slots
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        state->weapons[i] = (Weapon){
            .type = WEAPON_DRONE_BLASTER,
            .name = "Drone Blaster",
            .ammo = 25,
            .max_ammo = 25,
            .reload_time = 0.25f,
            .reload_timer = 0.0f,
            .projectile_speed = 360.0f,
            .damage = 15
        };
    }
    state->current_slot = 0;
}

void UpdatePlayerMovement(PlayerState *plane, KeyBindings *controls, float dt) {
    if (!plane->is_alive) return;

    Vector2 mouseDelta = GetMouseDelta();
    float mouseSensitivity = 0.0038f;

    float targetPitchRate = -mouseDelta.y * mouseSensitivity * 30.0f;
    float targetYawRate   = -mouseDelta.x * mouseSensitivity * 30.0f;

    float maxTurnRate = plane->is_drone ? 2.8f : 2.0f;
    targetPitchRate = Clamp(targetPitchRate, -maxTurnRate, maxTurnRate);
    targetYawRate   = Clamp(targetYawRate, -maxTurnRate, maxTurnRate);

    static float currentPitchRate = 0.0f;
    static float currentYawRate   = 0.0f;
    float smoothing = plane->is_drone ? 7.0f : 4.5f;

    currentPitchRate += (targetPitchRate - currentPitchRate) * smoothing * dt;
    currentYawRate   += (targetYawRate - currentYawRate) * smoothing * dt;

    plane->pitch += currentPitchRate * dt;
    plane->yaw   += currentYawRate * dt;

    float turnSpeed = 1.2f;
    if (IsKeyDown(controls->bindings[ACTION_LEFT]))  plane->yaw += turnSpeed * dt;
    if (IsKeyDown(controls->bindings[ACTION_RIGHT])) plane->yaw -= turnSpeed * dt;

    if (plane->pitch > 1.3f)  plane->pitch = 1.3f;
    if (plane->pitch < -1.3f) plane->pitch = -1.3f;

    float targetRoll = Clamp(currentYawRate * 0.7f, -0.85f, 0.85f);
    if (IsKeyDown(controls->bindings[ACTION_LEFT]))  targetRoll =  0.85f;
    if (IsKeyDown(controls->bindings[ACTION_RIGHT])) targetRoll = -0.85f;

    plane->roll += (targetRoll - plane->roll) * 4.0f * dt;

    static float currentSpeed = 45.0f;
    float targetSpeed = plane->is_drone ? 32.0f : 45.0f;

    if (IsKeyDown(controls->bindings[ACTION_UP]))   targetSpeed = plane->is_drone ? 55.0f : 75.0f;
    if (IsKeyDown(controls->bindings[ACTION_DOWN])) targetSpeed = plane->is_drone ? 10.0f : 22.0f;

    currentSpeed += (targetSpeed - currentSpeed) * 3.0f * dt;

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
    Vector3 forward = {
        -cosf(state->pitch) * sinf(state->yaw),
         sinf(state->pitch),
        -cosf(state->pitch) * cosf(state->yaw)
    };

    float dist = 18.0f;
    float height = 5.0f;

    camera->position.x = state->position.x - forward.x * dist;
    camera->position.y = state->position.y - forward.y * dist + height;
    camera->position.z = state->position.z - forward.z * dist;

    camera->target.x = state->position.x + forward.x * 10.0f;
    camera->target.y = state->position.y + forward.y * 10.0f;
    camera->target.z = state->position.z + forward.z * 10.0f;
}

void DrawPlanePlayer(const PlayerState *state) {
    rlPushMatrix();
        rlTranslatef(state->position.x, state->position.y, state->position.z);
        rlRotatef(state->yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
        rlRotatef(state->pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
        rlRotatef(state->roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

        if (state->is_drone) {
            // Dessin du Drone 2x plus gros (10.0 x 4.0 x 10.0)
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, MAROON);
            DrawCubeWires((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, GOLD);
            DrawCube((Vector3){ 0.0f, 0.0f, -5.2f }, 2.5f, 1.5f, 1.5f, YELLOW); // Canon frontal
        } else {
            // Dessin du chasseur
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 1.2f, 0.8f, 5.0f, RED);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.2f }, 7.0f, 0.15f, 1.5f, DARKGRAY);
            DrawCube((Vector3){ 0.0f, 0.8f, 2.0f }, 0.2f, 1.2f, 1.0f, RED);
            DrawCube((Vector3){ 0.0f, 0.4f, -0.8f }, 0.8f, 0.5f, 1.5f, SKYBLUE);
        }
    rlPopMatrix();
}