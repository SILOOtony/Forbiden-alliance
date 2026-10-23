#include "playerMovment.h"
#include "rlgl.h"
#include <math.h>
#include "raymath.h"

void InitPlayerState(PlayerState *state) {
    if (state->is_attacker) {
        state->position = (Vector3){ 0.0f, 95.0f, 450.0f };
        state->yaw = 0.0f;
    } else {
        state->position = (Vector3){ 0.0f, 80.0f, -250.0f };
        state->yaw = PI;
    }

    state->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    state->pitch = 0.0f;
    state->roll = 0.0f;
    state->is_alive = true;
    state->is_drone = false;

    // Statistiques de vie et vitesse selon la classe d'avion choisie
    if (state->plane_class == PLANE_INTERCEPTOR) {
        state->max_hp = 75;
        state->speed = 70.0f;
    } else if (state->plane_class == PLANE_BOMBER) {
        state->max_hp = 150;
        state->speed = 42.0f;
    } else {
        state->max_hp = 100;
        state->speed = 60.0f;
    }
    state->hp = state->max_hp;

    InitWeaponSlotsForClass(state->weapons, (PlaneClass)state->plane_class);
    state->current_slot = 0;
}

void ReincarnateAsDrone(PlayerState *state, Vector3 dronePos) {
    state->position = dronePos;
    state->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    state->pitch = 0.0f;
    state->yaw = 0.0f;
    state->roll = 0.0f;
    state->speed = 35.0f;
    state->max_hp = 100;
    state->hp = 100;
    state->is_alive = true;
    state->is_drone = true;

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

    // Maniabilité adaptée à la classe (Intercepteur vif, Bombardier lourd)
    float maxTurnRate = 2.0f;
    float smoothing   = 4.5f;
    if (plane->is_drone) {
        maxTurnRate = 2.8f;
        smoothing   = 7.0f;
    } else if (plane->plane_class == PLANE_INTERCEPTOR) {
        maxTurnRate = 2.6f;
        smoothing   = 6.5f;
    } else if (plane->plane_class == PLANE_BOMBER) {
        maxTurnRate = 1.45f;
        smoothing   = 3.0f;
    }

    targetPitchRate = Clamp(targetPitchRate, -maxTurnRate, maxTurnRate);
    targetYawRate   = Clamp(targetYawRate, -maxTurnRate, maxTurnRate);

    static float currentPitchRate = 0.0f;
    static float currentYawRate   = 0.0f;

    currentPitchRate += (targetPitchRate - currentPitchRate) * smoothing * dt;
    currentYawRate   += (targetYawRate - currentYawRate) * smoothing * dt;

    plane->pitch += currentPitchRate * dt;
    plane->yaw   += currentYawRate * dt;

    float turnSpeed = (plane->plane_class == PLANE_BOMBER) ? 0.85f : 1.25f;
    if (IsKeyDown(controls->bindings[ACTION_LEFT]))  plane->yaw += turnSpeed * dt;
    if (IsKeyDown(controls->bindings[ACTION_RIGHT])) plane->yaw -= turnSpeed * dt;

    if (plane->pitch > 1.3f)  plane->pitch = 1.3f;
    if (plane->pitch < -1.3f) plane->pitch = -1.3f;

    float targetRoll = Clamp(currentYawRate * 0.7f, -0.85f, 0.85f);
    if (IsKeyDown(controls->bindings[ACTION_LEFT]))  targetRoll =  0.85f;
    if (IsKeyDown(controls->bindings[ACTION_RIGHT])) targetRoll = -0.85f;

    plane->roll += (targetRoll - plane->roll) * 4.0f * dt;

    // Vitesses adaptées à chaque classe
    static float currentSpeed = 45.0f;
    float baseSpeed = 45.0f, boostSpeed = 75.0f, slowSpeed = 22.0f;

    if (plane->is_drone) {
        baseSpeed = 32.0f; boostSpeed = 55.0f; slowSpeed = 10.0f;
    } else if (plane->plane_class == PLANE_INTERCEPTOR) {
        baseSpeed = 60.0f; boostSpeed = 95.0f; slowSpeed = 30.0f;
    } else if (plane->plane_class == PLANE_BOMBER) {
        baseSpeed = 35.0f; boostSpeed = 55.0f; slowSpeed = 18.0f;
    }

    float targetSpeed = baseSpeed;
    if (IsKeyDown(controls->bindings[ACTION_UP]))   targetSpeed = boostSpeed;
    if (IsKeyDown(controls->bindings[ACTION_DOWN])) targetSpeed = slowSpeed;

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

    float dist = (state->plane_class == PLANE_BOMBER && !state->is_drone) ? 22.0f : 18.0f;
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
            // Drone (10.0 x 4.0 x 10.0)
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, MAROON);
            DrawCubeWires((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, GOLD);
            DrawCube((Vector3){ 0.0f, 0.0f, -5.2f }, 2.5f, 1.5f, 1.5f, YELLOW);
        } 
        else if (state->plane_class == PLANE_INTERCEPTOR) {
            // Classe 2 : Intercepteur léger (fuselage fin allongé, ailes en flèche, double dérive)
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.9f, 0.6f, 6.2f, BLUE);          // Fuselage élancé
            DrawCube((Vector3){ 0.0f, 0.0f, -3.4f }, 0.4f, 0.3f, 1.2f, SKYBLUE);      // Nez pointu
            DrawCube((Vector3){ 0.0f, 0.0f, 0.4f }, 5.6f, 0.12f, 2.2f, SKYBLUE);      // Ailes delta rapides
            DrawCube((Vector3){ -0.6f, 0.7f, 2.3f }, 0.15f, 1.1f, 0.9f, DARKBLUE);    // Dérive gauche
            DrawCube((Vector3){  0.6f, 0.7f, 2.3f }, 0.15f, 1.1f, 0.9f, DARKBLUE);    // Dérive droite
            DrawCube((Vector3){ 0.0f, 0.35f, -1.0f }, 0.65f, 0.4f, 1.8f, GOLD);       // Cockpit doré
        } 
        else if (state->plane_class == PLANE_BOMBER) {
            // Classe 3 : Bombardier lourd (gros fuselage blindé, grandes ailes + 2 moteurs sous les ailes)
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 2.2f, 1.3f, 6.5f, DARKGREEN);     // Gros fuselage blindé
            DrawCube((Vector3){ 0.0f, 0.1f, 0.0f }, 10.5f, 0.25f, 2.4f, DARKGRAY);    // Grandes ailes larges
            DrawCube((Vector3){ -3.0f, -0.35f, 0.2f }, 0.9f, 0.7f, 2.2f, BLACK);      // Réacteur gauche
            DrawCube((Vector3){  3.0f, -0.35f, 0.2f }, 0.9f, 0.7f, 2.2f, BLACK);      // Réacteur droit
            DrawCube((Vector3){ -1.0f, 1.1f, 2.6f }, 0.25f, 1.4f, 1.2f, DARKGREEN);   // Double queue lourde G
            DrawCube((Vector3){  1.0f, 1.1f, 2.6f }, 0.25f, 1.4f, 1.2f, DARKGREEN);   // Double queue lourde D
            DrawCube((Vector3){ 0.0f, 0.6f, -1.6f }, 1.5f, 0.55f, 1.6f, SKYBLUE);     // Verrière large
        } 
        else {
            // Classe 1 : Chasseur Polyvalent (Classique Rouge)
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 1.2f, 0.8f, 5.0f, RED);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.2f }, 7.0f, 0.15f, 1.5f, DARKGRAY);
            DrawCube((Vector3){ 0.0f, 0.8f, 2.0f }, 0.2f, 1.2f, 1.0f, RED);
            DrawCube((Vector3){ 0.0f, 0.4f, -0.8f }, 0.8f, 0.5f, 1.5f, SKYBLUE);
        }
    rlPopMatrix();
}