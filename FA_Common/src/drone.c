#include "drone.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef SERVER_BUILD
#include "rlgl.h"
static bool droneModelsChecked = false;
static bool hasAttackerDroneModel = false;
static bool hasDefenderDroneModel = false;
static Model attackerDroneModel;
static Model defenderDroneModel;

static Color GetDroneMatColor(const char *matName, bool is_attacker) {
    if (strcmp(matName, "mat23") == 0) return (Color){  30,  30,  38, 255 };
    if (strcmp(matName, "mat17") == 0) return (Color){  69,  89,  99, 255 };
    if (strcmp(matName, "mat16") == 0) return (Color){ 120, 143, 156, 255 };
    if (strcmp(matName, "mat22") == 0) return (Color){ 158, 158, 158, 255 };
    if (strcmp(matName, "mat25") == 0) return (Color){ 235, 242, 240, 255 };
    if (strcmp(matName, "mat15") == 0) return (Color){ 206, 217, 219, 255 };
    if (strcmp(matName, "mat21") == 0) return (Color){ 255, 255, 255, 255 };
    if (strcmp(matName, "mat3")  == 0) return (Color){ 128, 222, 235, 255 };
    if (strcmp(matName, "mat4")  == 0) return (Color){   0, 189, 212, 255 };
    if (strcmp(matName, "mat8")  == 0) return (Color){ 245,  66,  54, 255 };
    if (strcmp(matName, "mat14") == 0) return (Color){ 255,  87,  33, 255 };
    return is_attacker ? (Color){ 200, 60, 50, 255 } : (Color){ 70, 150, 210, 255 };
}

static void ParseObjFaceToken(const char *tok, int *outV, int *outVn) {
    *outV = atoi(tok);
    *outVn = 0;
    const char *s1 = strchr(tok, '/');
    if (s1) {
        const char *s2 = strchr(s1 + 1, '/');
        if (s2) *outVn = atoi(s2 + 1);
    }
}

Model LoadSafeDroneModel(const char *objPath, bool is_attacker) {
    Model empty = { 0 };
    FILE *f = fopen(objPath, "rb");
    if (!f) return empty;

    size_t vCap = 4096, vCount = 0;
    size_t vnCap = 4096, vnCount = 0;
    size_t outCap = 16384, outVerts = 0;

    Vector3 *vList  = (Vector3 *)malloc(vCap * sizeof(Vector3));
    Vector3 *vnList = (Vector3 *)malloc(vnCap * sizeof(Vector3));
    float *outPos   = (float *)malloc(outCap * 3 * sizeof(float));
    float *outNorm  = (float *)malloc(outCap * 3 * sizeof(float));
    unsigned char *outCol = (unsigned char *)malloc(outCap * 4 * sizeof(unsigned char));

    Color curCol = is_attacker ? (Color){ 210, 65, 55, 255 } : (Color){ 80, 165, 220, 255 };
    float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
    float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) {
            if (vCount >= vCap) {
                vCap *= 2;
                vList = (Vector3 *)realloc(vList, vCap * sizeof(Vector3));
            }
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (sscanf(p + 2, "%f %f %f", &x, &y, &z) == 3) {
                vList[vCount++] = (Vector3){ x, y, z };
                if (x < minX) minX = x; if (x > maxX) maxX = x;
                if (y < minY) minY = y; if (y > maxY) maxY = y;
                if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
            }
        }
        else if (p[0] == 'v' && p[1] == 'n' && (p[2] == ' ' || p[2] == '\t')) {
            if (vnCount >= vnCap) {
                vnCap *= 2;
                vnList = (Vector3 *)realloc(vnList, vnCap * sizeof(Vector3));
            }
            float nx = 0.0f, ny = 1.0f, nz = 0.0f;
            if (sscanf(p + 3, "%f %f %f", &nx, &ny, &nz) == 3) {
                vnList[vnCount++] = (Vector3){ nx, ny, nz };
            }
        }
        else if (strncmp(p, "usemtl", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
            char mName[64] = { 0 };
            sscanf(p + 7, "%63s", mName);
            curCol = GetDroneMatColor(mName, is_attacker);
        }
        else if (p[0] == 'f' && (p[1] == ' ' || p[1] == '\t')) {
            int fv[16], fvn[16];
            int cornerCount = 0;
            char *tok = strtok(p + 2, " \t\r\n");
            while (tok && cornerCount < 16) {
                ParseObjFaceToken(tok, &fv[cornerCount], &fvn[cornerCount]);
                cornerCount++;
                tok = strtok(NULL, " \t\r\n");
            }
            for (int t = 1; t + 1 < cornerCount; t++) {
                if (outVerts + 3 >= outCap) {
                    outCap *= 2;
                    outPos  = (float *)realloc(outPos, outCap * 3 * sizeof(float));
                    outNorm = (float *)realloc(outNorm, outCap * 3 * sizeof(float));
                    outCol  = (unsigned char *)realloc(outCol, outCap * 4 * sizeof(unsigned char));
                }
                int idx[3]  = { fv[0], fv[t], fv[t + 1] };
                int nidx[3] = { fvn[0], fvn[t], fvn[t + 1] };

                for (int k = 0; k < 3; k++) {
                    int vi = (idx[k] > 0) ? (idx[k] - 1) : ((int)vCount + idx[k]);
                    int ni = (nidx[k] > 0) ? (nidx[k] - 1) : ((int)vnCount + nidx[k]);
                    Vector3 pos = (vi >= 0 && vi < (int)vCount) ? vList[vi] : (Vector3){ 0.0f, 0.0f, 0.0f };
                    Vector3 nrm = (ni >= 0 && ni < (int)vnCount) ? vnList[ni] : (Vector3){ 0.0f, 1.0f, 0.0f };

                    outPos[outVerts * 3 + 0] = pos.x;
                    outPos[outVerts * 3 + 1] = pos.y;
                    outPos[outVerts * 3 + 2] = pos.z;

                    outNorm[outVerts * 3 + 0] = nrm.x;
                    outNorm[outVerts * 3 + 1] = nrm.y;
                    outNorm[outVerts * 3 + 2] = nrm.z;

                    outCol[outVerts * 4 + 0] = curCol.r;
                    outCol[outVerts * 4 + 1] = curCol.g;
                    outCol[outVerts * 4 + 2] = curCol.b;
                    outCol[outVerts * 4 + 3] = 255;
                    outVerts++;
                }
            }
        }
    }
    fclose(f);
    free(vList);
    free(vnList);

    if (outVerts == 0) {
        free(outPos); free(outNorm); free(outCol);
        return empty;
    }

    // Centrage et normalisation automatique de la taille du drone (~2.5 unités)
    float cx = (minX + maxX) * 0.5f;
    float cy = (minY + maxY) * 0.5f;
    float cz = (minZ + maxZ) * 0.5f;
    float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
    float maxDim = dx;
    if (dy > maxDim) maxDim = dy;
    if (dz > maxDim) maxDim = dz;
    float normScale = (maxDim > 0.001f) ? (2.5f / maxDim) : 1.0f;

    for (size_t i = 0; i < outVerts; i++) {
        outPos[i * 3 + 0] = (outPos[i * 3 + 0] - cx) * normScale;
        outPos[i * 3 + 1] = (outPos[i * 3 + 1] - cy) * normScale;
        outPos[i * 3 + 2] = (outPos[i * 3 + 2] - cz) * normScale;
    }

    Mesh mesh = { 0 };
    mesh.vertexCount = (int)outVerts;
    mesh.triangleCount = (int)(outVerts / 3);
    mesh.vertices = outPos;
    mesh.normals  = outNorm;
    mesh.colors   = outCol;

    UploadMesh(&mesh, false);
    Model mdl = LoadModelFromMesh(mesh);
    printf("[3D Drone OK] %s charge (%d triangles)\n", objPath, mesh.triangleCount);
    fflush(stdout);
    return mdl;
}
#endif

static unsigned int drone_init_rng = 98765u;

static int Drone_NextRand(unsigned int *state, int min, int max) {
    if (min > max) { int tmp = max; max = min; min = tmp; }
    *state = (*state) * 1103515245u + 12345u;
    unsigned int rand_val = ((*state) / 65536u) % 32768u;
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

static bool Drone_CheckSpheres(Vector3 a, float ra, Vector3 b, float rb) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    float r = ra + rb;
    return (dx * dx + dy * dy + dz * dz) <= (r * r);
}

static bool Drone_CheckBoxSphere(Vector3 bmin, Vector3 bmax, Vector3 center, float radius) {
    float cx = (center.x < bmin.x) ? bmin.x : ((center.x > bmax.x) ? bmax.x : center.x);
    float cy = (center.y < bmin.y) ? bmin.y : ((center.y > bmax.y) ? bmax.y : center.y);
    float cz = (center.z < bmin.z) ? bmin.z : ((center.z > bmax.z) ? bmax.z : center.z);
    float dx = center.x - cx;
    float dy = center.y - cy;
    float dz = center.z - cz;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

void InitDrone(Drone *drone, bool is_attacker) {
    drone->hp = 100;
    drone->ammo = 30;
    drone->max_ammo = 30;
    drone->reload_timer = 0.0f;
    drone->speed = (float)Drone_NextRand(&drone_init_rng, 25, 45);

    drone->pos = (Vector3){
        (float)Drone_NextRand(&drone_init_rng, -450, 450),
        (float)Drone_NextRand(&drone_init_rng, 45, 135),
        (float)Drone_NextRand(&drone_init_rng, -450, 450)
    };

    float randomAngle = (float)Drone_NextRand(&drone_init_rng, 0, 360) * DEG2RAD;
    drone->direction = (Vector3){ cosf(randomAngle), 0.0f, sinf(randomAngle) };
    drone->is_alive = true;
    drone->is_attacker = is_attacker;

    drone->shoot_timer = (float)Drone_NextRand(&drone_init_rng, 15, 50) / 10.0f;
    drone->bullet_pos = drone->pos;
    drone->bullet_dir = (Vector3){ 0.0f, -1.0f, 0.0f };
    drone->bullet_speed = 240.0f;
    drone->bullet_lifetime = 0.0f;
    drone->bullet_active = false;
}

void SpawnDrones(Drone drones[], int max_drone_count) {
    drone_init_rng = 98765u;
    for (int i = 0; i < max_drone_count; i++) {
        bool is_attacker = (i >= DEFENDER_DRONE_COUNT);
        InitDrone(&drones[i], is_attacker);
    }
}

void UpdateDrones(
    Drone drones[],
    int max_drone_count,
    GameMap *map,
    uint16_t *out_buildings_destroyed,
    uint32_t *out_city_damage,
    float dt
) {
    unsigned int local_rng = (map != NULL) ? map->drone_rng : 98765u;

    for (int i = 0; i < max_drone_count; i++) {
        Drone *d = &drones[i];

        if (d->is_alive) {
            d->pos.x += d->direction.x * d->speed * dt;
            d->pos.z += d->direction.z * d->speed * dt;

            if (d->pos.x > 500.0f || d->pos.x < -500.0f) d->direction.x = -d->direction.x;
            if (d->pos.z > 500.0f || d->pos.z < -500.0f) d->direction.z = -d->direction.z;

            d->shoot_timer -= dt;
            if (d->shoot_timer <= 0.0f && !d->bullet_active) {
                d->shoot_timer = (float)Drone_NextRand(&local_rng, 20, 55) / 10.0f;
                Vector3 targetPos = { 0 };
                bool foundTarget = false;

                if (d->is_attacker && map != NULL && map->building_count > 0) {
                    for (int tries = 0; tries < 15; tries++) {
                        int bIdx = Drone_NextRand(&local_rng, 0, map->building_count - 1);
                        if (!map->buildings[bIdx].is_destroyed && map->buildings[bIdx].hp > 0) {
                            targetPos = map->buildings[bIdx].position;
                            foundTarget = true;
                            break;
                        }
                    }
                } else if (!d->is_attacker) {
                    for (int tries = 0; tries < 15; tries++) {
                        int tIdx = Drone_NextRand(&local_rng, DEFENDER_DRONE_COUNT, max_drone_count - 1);
                        if (drones[tIdx].is_alive) {
                            targetPos = drones[tIdx].pos;
                            foundTarget = true;
                            break;
                        }
                    }
                }

                if (foundTarget) {
                    targetPos.x += (float)Drone_NextRand(&local_rng, -20, 20);
                    targetPos.y += (float)Drone_NextRand(&local_rng, -10, 10);
                    targetPos.z += (float)Drone_NextRand(&local_rng, -20, 20);

                    float dx = targetPos.x - d->pos.x;
                    float dy = targetPos.y - d->pos.y;
                    float dz = targetPos.z - d->pos.z;
                    float len = sqrtf(dx * dx + dy * dy + dz * dz);

                    if (len > 0.001f) {
                        d->bullet_dir = (Vector3){ dx / len, dy / len, dz / len };
                        d->bullet_pos = d->pos;
                        d->bullet_speed = 250.0f;
                        d->bullet_lifetime = 3.5f;
                        d->bullet_active = true;
                    }
                }
            }
        }

        if (d->bullet_active) {
            d->bullet_lifetime -= dt;
            d->bullet_pos.x += d->bullet_dir.x * d->bullet_speed * dt;
            d->bullet_pos.y += d->bullet_dir.y * d->bullet_speed * dt;
            d->bullet_pos.z += d->bullet_dir.z * d->bullet_speed * dt;

            if (d->bullet_lifetime <= 0.0f || d->bullet_pos.y <= 0.0f) {
                d->bullet_active = false;
                continue;
            }

            if (d->is_attacker && map != NULL) {
                for (int b = 0; b < map->building_count; b++) {
                    Building *bld = &map->buildings[b];
                    if (bld->is_destroyed || bld->hp <= 0) continue;

                    if (Drone_CheckBoxSphere(bld->min, bld->max, d->bullet_pos, 0.8f)) {
                        d->bullet_active = false;
                        int dmg = (bld->max_hp + 14) / 15;
                        if (dmg < 1) dmg = 1;
                        if (dmg > bld->hp) dmg = bld->hp;

                        bld->hp -= dmg;
                        if (out_city_damage != NULL) {
                            *out_city_damage += (uint32_t)dmg;
                        }
                        if (bld->hp <= 0) {
                            bld->hp = 0;
                            bld->is_destroyed = true;
                            if (out_buildings_destroyed != NULL) {
                                (*out_buildings_destroyed)++;
                            }
                        }
                        break;
                    }
                }
            } else if (!d->is_attacker) {
                for (int t = DEFENDER_DRONE_COUNT; t < max_drone_count; t++) {
                    if (!drones[t].is_alive) continue;

                    if (Drone_CheckSpheres(d->bullet_pos, 1.0f, drones[t].pos, 6.0f)) {
                        d->bullet_active = false;
                        drones[t].hp -= 10;
                        if (drones[t].hp <= 0) {
                            drones[t].hp = 0;
                            drones[t].is_alive = false;
                            drones[t].bullet_active = false;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (map != NULL) {
        map->drone_rng = local_rng;
    }
}

#ifndef SERVER_BUILD
void DrawDrones(const Drone drones[], int max_drone_count) {
    if (!droneModelsChecked) {
        droneModelsChecked = true;

        if (FileExists("assets/attacker_drone.obj")) {
            attackerDroneModel = LoadSafeDroneModel("assets/attacker_drone.obj", true);
            hasAttackerDroneModel = (attackerDroneModel.meshCount > 0);
        }
        if (FileExists("assets/defender_drone.obj")) {
            defenderDroneModel = LoadSafeDroneModel("assets/defender_drone.obj", false);
            hasDefenderDroneModel = (defenderDroneModel.meshCount > 0);
        }
    }

    for (int i = 0; i < max_drone_count; i++) {
        if (drones[i].is_alive) {
            bool isAtt = drones[i].is_attacker;
            bool canDraw3D = isAtt ? (hasAttackerDroneModel || hasDefenderDroneModel)
                                   : (hasDefenderDroneModel || hasAttackerDroneModel);

            if (canDraw3D) {
                Model *mdl = isAtt ? (hasAttackerDroneModel ? &attackerDroneModel : &defenderDroneModel)
                                   : (hasDefenderDroneModel ? &defenderDroneModel : &attackerDroneModel);
                float yawDeg = atan2f(drones[i].direction.x, drones[i].direction.z) * RAD2DEG;
                rlPushMatrix();
                    rlTranslatef(drones[i].pos.x, drones[i].pos.y, drones[i].pos.z);
                    rlRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
                    DrawModel(*mdl, (Vector3){ 0.0f, 0.0f, 0.0f }, 3.8f, WHITE);
                rlPopMatrix();
            } else {
                Color bodyColor = isAtt ? MAROON : DARKBLUE;
                Color wireColor = isAtt ? RED : SKYBLUE;
                DrawCube(drones[i].pos, 10.0f, 4.0f, 10.0f, bodyColor);
                DrawCubeWires(drones[i].pos, 10.0f, 4.0f, 10.0f, wireColor);
            }
        }

        if (drones[i].bullet_active) {
            Color bulletColor = drones[i].is_attacker ? ORANGE : SKYBLUE;
            DrawSphere(drones[i].bullet_pos, 0.55f, bulletColor);
        }
    }
}
#endif