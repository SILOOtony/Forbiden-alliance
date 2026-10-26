#include "playerMovment.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include "raymath.h"
#include "drone.h"

static bool modelsChecked = false;
static bool hasCustomModel[3] = { false, false, false };
static Model customModels[3];
static float customModelScales[3] = { 1.0f, 0.9f, 1.4f };
static bool hasAttDroneMdl = false;
static bool hasDefDroneMdl = false;
static Model attDroneMdl;
static Model defDroneMdl;

static void EnsureMtlFileForPlanes(void) {
    if (!FileExists("assets/materials.mtl")) {
        FILE *f = fopen("assets/materials.mtl", "w");
        if (f) {
            fprintf(f,
                "newmtl mat23\nKd 0.12 0.12 0.15\nd 1.0\n"
                "newmtl mat17\nKd 0.27 0.35 0.39\nd 1.0\n"
                "newmtl mat16\nKd 0.47 0.56 0.61\nd 1.0\n"
                "newmtl mat22\nKd 0.62 0.62 0.62\nd 1.0\n"
                "newmtl mat25\nKd 0.92 0.95 0.94\nd 1.0\n"
                "newmtl mat15\nKd 0.81 0.85 0.86\nd 1.0\n"
                "newmtl mat21\nKd 1.00 1.00 1.00\nd 1.0\n"
                "newmtl mat3\nKd 0.50 0.87 0.92\nd 1.0\n"
                "newmtl mat4\nKd 0.00 0.74 0.83\nd 1.0\n"
                "newmtl mat8\nKd 0.96 0.26 0.21\nd 1.0\n"
                "newmtl mat14\nKd 1.00 0.34 0.13\nd 1.0\n");
            fclose(f);
        }
    }
}

static void EnsurePlaneModelsLoaded(void) {
    if (modelsChecked) return;
    modelsChecked = true;
    EnsureMtlFileForPlanes();

    for (int i = 0; i < 3; i++) {
        const char *pathToLoad = NULL;
        if (i == PLANE_FIGHTER) {
            if (FileExists("assets/fighter.obj")) {
                pathToLoad = "assets/fighter.obj";
                customModelScales[i] = 1.0f;
            } else if (FileExists("assets/1397 fighter.obj")) {
                pathToLoad = "assets/1397 fighter.obj";
                customModelScales[i] = 0.085f;
            }
        } else if (i == PLANE_INTERCEPTOR && FileExists("assets/interceptor.obj")) {
            pathToLoad = "assets/interceptor.obj";
            customModelScales[i] = 0.9f;
        } else if (i == PLANE_BOMBER && FileExists("assets/bomber.obj")) {
            pathToLoad = "assets/bomber.obj";
            customModelScales[i] = 1.45f;
        }

        if (pathToLoad == NULL && FileExists("assets/interceptor.obj")) {
            pathToLoad = "assets/interceptor.obj";
            customModelScales[i] = (i == PLANE_BOMBER) ? 1.25f : 1.0f;
        }

        if (pathToLoad != NULL) {
            customModels[i] = LoadModel(pathToLoad);
            if (customModels[i].meshCount > 0) {
                for (int m = 0; m < customModels[i].materialCount; m++) {
                    customModels[i].materials[m].maps[MATERIAL_MAP_DIFFUSE].color.a = 255;
                }
                hasCustomModel[i] = true;
            }
        }
    }

    if (FileExists("assets/attacker_drone.obj")) {
        attDroneMdl = LoadSafeDroneModel("assets/attacker_drone.obj", true);
        hasAttDroneMdl = (attDroneMdl.meshCount > 0);
    }
    if (FileExists("assets/defender_drone.obj")) {
        defDroneMdl = LoadSafeDroneModel("assets/defender_drone.obj", false);
        hasDefDroneMdl = (defDroneMdl.meshCount > 0);
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
    // Spawns placés à z = +520 et z = -520 (hors de la grille d'immeubles -434..+434) à y = 110
    if (state->is_attacker) {
        state->position = (Vector3){ 0.0f, 110.0f, 520.0f };
        state->yaw = 0.0f;
    } else {
        state->position = (Vector3){ 0.0f, 110.0f, -520.0f };
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
    plane->speed = currentSpeed;

    Vector3 forward = {
        -cosf(plane->pitch) * sinf(plane->yaw),
        sinf(plane->pitch),
        -cosf(plane->pitch) * cosf(plane->yaw)
    };

    plane->position.x += forward.x * currentSpeed * dt;
    plane->position.y += forward.y * currentSpeed * dt;
    plane->position.z += forward.z * currentSpeed * dt;

    if (plane->position.x >  1000.0f) plane->position.x =  1000.0f;
    if (plane->position.x < -1000.0f) plane->position.x = -1000.0f;
    if (plane->position.z >  1000.0f) plane->position.z =  1000.0f;
    if (plane->position.z < -1000.0f) plane->position.z = -1000.0f;
    if (plane->position.y >   500.0f) plane->position.y =   500.0f;
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
            bool useAtt = state->is_attacker;
            if ((useAtt && hasAttDroneMdl) || (!useAtt && hasDefDroneMdl) || hasAttDroneMdl) {
                Model *dMdl = (useAtt && hasAttDroneMdl) ? &attDroneMdl : (hasDefDroneMdl ? &defDroneMdl : &attDroneMdl);
                rlPushMatrix();
                    rlRotatef(180.0f, 0.0f, 1.0f, 0.0f);
                    DrawModel(*dMdl, (Vector3){ 0.0f, 0.0f, 0.0f }, 3.8f, WHITE);
                rlPopMatrix();
            } else {
                DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, MAROON);
                DrawCubeWires((Vector3){ 0.0f, 0.0f, 0.0f }, 10.0f, 4.0f, 10.0f, GOLD);
            }
        }
        else if (hasCustomModel[cls]) {
            Model *mdl = &customModels[cls];
            if (mdl->materialCount >= 4) {
                mdl->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = primary;
                mdl->materials[1].maps[MATERIAL_MAP_DIFFUSE].color = primary;
                mdl->materials[2].maps[MATERIAL_MAP_DIFFUSE].color = cockpit;
                mdl->materials[3].maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 30, 30, 35, 255 };
                if (mdl->materialCount >= 5) {
                    mdl->materials[4].maps[MATERIAL_MAP_DIFFUSE].color = secondary;
                }
            } else {
                for (int m = 0; m < mdl->materialCount; m++) {
                    mdl->materials[m].maps[MATERIAL_MAP_DIFFUSE].color = (state->skin_id == 0) ? WHITE : primary;
                }
            }

            rlPushMatrix();
                rlRotatef(180.0f, 0.0f, 1.0f, 0.0f);
                DrawModel(*mdl, (Vector3){ 0.0f, 0.0f, 0.0f }, customModelScales[cls], WHITE);
            rlPopMatrix();

            DrawSphere((Vector3){ -0.7f, 0.08f, 4.5f }, 0.32f, ORANGE);
            DrawSphere((Vector3){  0.7f, 0.08f, 4.5f }, 0.32f, ORANGE);
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