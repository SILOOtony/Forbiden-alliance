#include "playerMovment.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include "raymath.h"

static bool modelsChecked = false;
static bool hasCustomModel[3] = { false, false, false };
static Model customModels[3];

static void EnsurePlaneModelsLoaded(void) {
    if (modelsChecked) return;
    modelsChecked = true;

    const char *paths[3] = {
        "assets/fighter.obj",
        "assets/interceptor.obj",
        "assets/bomber.obj"
    };

    for (int i = 0; i < 3; i++) {
        const char *pathToLoad = NULL;
        if (FileExists(paths[i])) {
            pathToLoad = paths[i];
        } else if (FileExists("assets/interceptor.obj")) {
            // Utilise interceptor.obj par défaut si fighter.obj ou bomber.obj ne sont pas encore dans assets/
            pathToLoad = "assets/interceptor.obj";
        }

        if (pathToLoad != NULL) {
            customModels[i] = LoadModel(pathToLoad);
            if (customModels[i].meshCount > 0) {
                // Force l'opacité à 255 sur tous les matériaux (corrige le bug d 0.000000 des fichiers .mtl Blender)
                for (int m = 0; m < customModels[i].materialCount; m++) {
                    customModels[i].materials[m].maps[MATERIAL_MAP_DIFFUSE].color.a = 255;
                }
                hasCustomModel[i] = true;
                printf("[3D Model] Classe %d chargee depuis : %s (%d meshes, %d materiaux)\n",
                       i, pathToLoad, customModels[i].meshCount, customModels[i].materialCount);
            }
        }
    }
}

static void GetSkinColors(uint8_t skin_id, uint8_t plane_class, Color *primary, Color *secondary, Color *cockpit) {
    *cockpit = SKYBLUE;
    switch (skin_id % 5) {
        case 1: // FURTIF NOIR
            *primary   = (Color){ 35, 38, 45, 255 };
            *secondary = (Color){ 190, 30, 40, 255 };
            *cockpit   = RED;
            break;
        case 2: // OR PRESTIGE
            *primary   = GOLD;
            *secondary = (Color){ 70, 58, 35, 255 };
            *cockpit   = RAYWHITE;
            break;
        case 3: // CYBER NÉON
            *primary   = (Color){ 65, 25, 115, 255 };
            *secondary = (Color){ 0, 240, 255, 255 };
            *cockpit   = MAGENTA;
            break;
        case 4: // ARCTIQUE
            *primary   = RAYWHITE;
            *secondary = SKYBLUE;
            *cockpit   = DARKBLUE;
            break;
        default: // CLASSIQUE PAR CLASSE
            if (plane_class == PLANE_INTERCEPTOR) {
                *primary = (Color){ 70, 95, 115, 255 };
                *secondary = (Color){ 135, 160, 175, 255 };
                *cockpit = (Color){ 128, 222, 234, 255 };
            } else if (plane_class == PLANE_BOMBER) {
                *primary = DARKGREEN;
                *secondary = DARKGRAY;
                *cockpit = SKYBLUE;
            } else {
                *primary = RED;
                *secondary = DARKGRAY;
                *cockpit = SKYBLUE;
            }
            break;
    }
}

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
            .damage = 50
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

    float maxTurnRate = 2.0f;
    float smoothing   = 4.5f;
    if (plane->is_drone) {
        maxTurnRate = 2.8f; smoothing = 7.0f;
    } else if (plane->plane_class == PLANE_INTERCEPTOR) {
        maxTurnRate = 2.6f; smoothing = 6.5f;
    } else if (plane->plane_class == PLANE_BOMBER) {
        maxTurnRate = 1.45f; smoothing = 3.0f;
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

    static float currentSpeed = 45.0f;
    float baseSpeed = 45.0f, boostSpeed = 75.0f, slowSpeed = 22.0f;

    if (plane->is_drone) {
        baseSpeed = 32.0f; boostSpeed = 55.0f; slowSpeed = 10.0f;
    } else if (plane->plane_class == PLANE_INTERCEPTOR) {
        baseSpeed = 85.0f; boostSpeed = 135.0f; slowSpeed = 30.0f;
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
    EnsurePlaneModelsLoaded();

    Color primary, secondary, cockpit;
    GetSkinColors(state->skin_id, state->plane_class, &primary, &secondary, &cockpit);

    rlPushMatrix();
        rlTranslatef(state->position.x, state->position.y, state->position.z);
        rlRotatef(state->yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
        rlRotatef(state->pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
        rlRotatef(state->roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

        uint8_t cls = (state->plane_class <= 2) ? state->plane_class : 0;

        if (state->is_drone) {
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, MAROON);
            DrawCubeWires((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, GOLD);
            DrawCube((Vector3){ 0.0f, 0.0f, -5.2f }, 2.5f, 1.5f, 1.5f, YELLOW);
        }
        else if (hasCustomModel[cls]) {
            Model *mdl = &customModels[cls];
            // Applique les couleurs du Skin ou de la classe aux différents matériaux du fichier .obj
            if (mdl->materialCount >= 4) {
                mdl->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = primary;   // Fuselage
                mdl->materials[1].maps[MATERIAL_MAP_DIFFUSE].color = primary;   // Fuselage principal (455A64)
                mdl->materials[2].maps[MATERIAL_MAP_DIFFUSE].color = cockpit;   // Verrière du cockpit (80DEEA)
                mdl->materials[3].maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 30, 30, 35, 255 }; // Tuyères (1A1A1A)
                if (mdl->materialCount >= 5) {
                    mdl->materials[4].maps[MATERIAL_MAP_DIFFUSE].color = secondary; // Ailes & missiles (78909C)
                }
            }

            // Rotation de 180° sur Y car le nez de interceptor.obj pointe vers +Z dans Blender
            rlPushMatrix();
                rlRotatef(180.0f, 0.0f, 1.0f, 0.0f);
                float scale = (cls == PLANE_BOMBER) ? 1.25f : ((cls == PLANE_INTERCEPTOR) ? 0.9f : 1.0f);
                DrawModel(*mdl, (Vector3){ 0.0f, 0.0f, 0.0f }, scale, WHITE);
            rlPopMatrix();

            // Flammes des 2 réacteurs à l'arrière du jet
            DrawSphere((Vector3){ -0.7f * (cls == PLANE_BOMBER ? 1.25f : 1.0f), 0.08f, 4.8f }, 0.32f, ORANGE);
            DrawSphere((Vector3){  0.7f * (cls == PLANE_BOMBER ? 1.25f : 1.0f), 0.08f, 4.8f }, 0.32f, ORANGE);
        }
        else if (cls == PLANE_INTERCEPTOR) {
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.9f, 0.6f, 6.2f, primary);
            DrawCube((Vector3){ 0.0f, 0.0f, -3.4f }, 0.4f, 0.3f, 1.2f, secondary);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.4f }, 5.6f, 0.12f, 2.2f, secondary);
            DrawCube((Vector3){ -0.6f, 0.7f, 2.3f }, 0.15f, 1.1f, 0.9f, primary);
            DrawCube((Vector3){  0.6f, 0.7f, 2.3f }, 0.15f, 1.1f, 0.9f, primary);
            DrawCube((Vector3){ 0.0f, 0.35f, -1.0f }, 0.65f, 0.4f, 1.8f, cockpit);
            DrawSphere((Vector3){ 0.0f, 0.0f, 3.4f }, 0.45f, ORANGE);
        }
        else if (cls == PLANE_BOMBER) {
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 2.2f, 1.3f, 6.5f, primary);
            DrawCube((Vector3){ 0.0f, 0.1f, 0.0f }, 10.5f, 0.25f, 2.4f, secondary);
            DrawCube((Vector3){ -3.0f, -0.35f, 0.2f }, 0.9f, 0.7f, 2.2f, BLACK);
            DrawCube((Vector3){  3.0f, -0.35f, 0.2f }, 0.9f, 0.7f, 2.2f, BLACK);
            DrawCube((Vector3){ -1.0f, 1.1f, 2.6f }, 0.25f, 1.4f, 1.2f, primary);
            DrawCube((Vector3){  1.0f, 1.1f, 2.6f }, 0.25f, 1.4f, 1.2f, primary);
            DrawCube((Vector3){ 0.0f, 0.6f, -1.6f }, 1.5f, 0.55f, 1.6f, cockpit);
            DrawSphere((Vector3){ -3.0f, -0.35f, 1.5f }, 0.4f, ORANGE);
            DrawSphere((Vector3){  3.0f, -0.35f, 1.5f }, 0.4f, ORANGE);
        }
        else {
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 1.2f, 0.8f, 5.0f, primary);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.2f }, 7.0f, 0.15f, 1.5f, secondary);
            DrawCube((Vector3){ 0.0f, 0.8f, 2.0f }, 0.2f, 1.2f, 1.0f, primary);
            DrawCube((Vector3){ 0.0f, 0.4f, -0.8f }, 0.8f, 0.5f, 1.5f, cockpit);
            DrawSphere((Vector3){ 0.0f, 0.0f, 2.7f }, 0.4f, ORANGE);
        }
    rlPopMatrix();
}