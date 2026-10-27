#include "camera_fx.h"
#include "raymath.h"
#include <math.h>

// Interpolation d'angle gérant proprement le passage entre -PI et +PI
static float LerpAngleShortest(float current, float target, float t) {
    float diff = target - current;
    while (diff >  PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    return current + diff * t;
}

static float AngleDiffShortest(float target, float current) {
    float diff = target - current;
    while (diff >  PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    return diff;
}

void InitPlaneCameraFX(PlaneCameraFX *fx, const PlayerState *plane) {
    fx->initialized    = true;
    fx->smoothedSpeed  = plane->speed;
    fx->smoothedPitch  = plane->pitch;
    fx->smoothedYaw    = plane->yaw;
    fx->smoothedRoll   = plane->roll;
    fx->verticalLag    = 0.0f;
    fx->lateralLag     = 0.0f;
    fx->currentFov     = 60.0f;
    fx->recoilBack     = 0.0f;
    fx->recoilPitch    = 0.0f;
    fx->shakeAmplitude = 0.0f;
    fx->shakeTimer     = 0.0f;

    Vector3 fwd = {
        -cosf(plane->pitch) * sinf(plane->yaw),
         sinf(plane->pitch),
        -cosf(plane->pitch) * cosf(plane->yaw)
    };
    fx->currentPos = (Vector3){
        plane->position.x - fwd.x * 16.0f,
        plane->position.y - fwd.y * 16.0f + 5.0f,
        plane->position.z - fwd.z * 16.0f
    };
    fx->currentTarget = Vector3Add(plane->position, Vector3Scale(fwd, 40.0f));
}

void TriggerCameraRecoil(PlaneCameraFX *fx, WeaponType weaponType) {
    if (weaponType == WEAPON_ROCKET) {
        fx->recoilBack     += 1.25f;
        fx->recoilPitch    += 0.018f;
        fx->shakeAmplitude  = 0.22f;
        fx->shakeTimer      = 0.14f;
    }
    else if (weaponType == WEAPON_AIR_TO_AIR) {
        fx->recoilBack     += 1.75f;
        fx->recoilPitch    += 0.026f;
        fx->shakeAmplitude  = 0.30f;
        fx->shakeTimer      = 0.18f;
    }
    else if (weaponType == WEAPON_AIR_TO_GROUND) {
        fx->recoilBack     += 2.40f;
        fx->recoilPitch    += 0.036f;
        fx->shakeAmplitude  = 0.42f;
        fx->shakeTimer      = 0.24f;
    }
    else if (weaponType == WEAPON_DRONE_BLASTER) {
        fx->recoilBack     += 0.65f;
        fx->recoilPitch    += 0.010f;
        fx->shakeAmplitude  = 0.12f;
        fx->shakeTimer      = 0.09f;
    }
    else if (weaponType == WEAPON_FLARES) {
        fx->recoilBack     += 0.45f;
        fx->shakeAmplitude  = 0.10f;
        fx->shakeTimer      = 0.10f;
    }

    // Plafonds de sécurité en cas de tir très rapide (spam-clic)
    if (fx->recoilBack  > 3.6f)  fx->recoilBack  = 3.6f;
    if (fx->recoilPitch > 0.06f) fx->recoilPitch = 0.06f;
}

void UpdatePlaneCameraFX(Camera3D *camera, PlaneCameraFX *fx, const PlayerState *plane, float dt) {
    if (!fx->initialized) {
        InitPlaneCameraFX(fx, plane);
    }

    // 1. Retard à l'accélération : smoothedSpeed rattrape plane->speed avec inertie
    fx->smoothedSpeed += (plane->speed - fx->smoothedSpeed) * (3.2f * dt);
    float accelDelta = plane->speed - fx->smoothedSpeed; // Positif quand on accélère, négatif quand on freine

    // 2. Retard angulaire : la caméra suit les changements de cap et d'assiette avec souplesse
    fx->smoothedPitch += (plane->pitch - fx->smoothedPitch) * (6.5f * dt);
    fx->smoothedYaw    = LerpAngleShortest(fx->smoothedYaw, plane->yaw, 6.5f * dt);
    fx->smoothedRoll  += (plane->roll - fx->smoothedRoll) * (7.5f * dt);

    float pitchDiff = plane->pitch - fx->smoothedPitch;
    float yawDiff   = AngleDiffShortest(plane->yaw, fx->smoothedYaw);

    // La caméra monte/descend légèrement lors des ressources et piqués
    float targetVertLag = -pitchDiff * 9.5f - sinf(plane->pitch) * 1.35f;
    fx->verticalLag += (targetVertLag - fx->verticalLag) * (7.0f * dt);

    // La caméra se décale légèrement sur le côté dans les virages et tonneaux
    float targetLatLag = yawDiff * 7.5f - sinf(fx->smoothedRoll) * 2.1f;
    fx->lateralLag += (targetLatLag - fx->lateralLag) * (6.5f * dt);

    // 3. Amortissement du recul (Recoil) et des vibrations de tir
    fx->recoilBack  -= fx->recoilBack  * (14.0f * dt);
    fx->recoilPitch -= fx->recoilPitch * (12.0f * dt);
    if (fx->shakeTimer > 0.0f) {
        fx->shakeTimer -= dt;
        if (fx->shakeTimer < 0.0f) fx->shakeTimer = 0.0f;
    }

    // Vecteurs de base à partir des angles lissés de la caméra
    Vector3 camFwd = {
        -cosf(fx->smoothedPitch) * sinf(fx->smoothedYaw),
         sinf(fx->smoothedPitch),
        -cosf(fx->smoothedPitch) * cosf(fx->smoothedYaw)
    };
    Vector3 planeFwd = {
        -cosf(plane->pitch) * sinf(plane->yaw),
         sinf(plane->pitch),
        -cosf(plane->pitch) * cosf(plane->yaw)
    };
    Vector3 camRight = { cosf(fx->smoothedYaw), 0.0f, -sinf(fx->smoothedYaw) };
    Vector3 worldUp  = { 0.0f, 1.0f, 0.0f };

    // 4. Distance dynamique : recule quand on accélère ou qu'on tire, se rapproche quand on décélère
    float baseDist  = plane->is_drone ? 12.5f : 15.8f;
    float speedPull = (plane->speed - 45.0f) * 0.065f;
    float accelLag  = accelDelta * 0.22f; // L'avion part devant, la caméra traîne un peu derrière avant de rattraper
    float totalDist = baseDist + speedPull + accelLag + fx->recoilBack;
    if (totalDist < 10.5f) totalDist = 10.5f;
    if (totalDist > 24.5f) totalDist = 24.5f;

    float baseHeight = plane->is_drone ? 3.6f : 4.8f;

    // Micro-secousse haute fréquence pendant un tir
    float shakeX = 0.0f, shakeY = 0.0f;
    if (fx->shakeTimer > 0.0f) {
        float intensity = fx->shakeAmplitude * (fx->shakeTimer / 0.18f);
        shakeX = sinf(fx->shakeTimer * 95.0f) * intensity;
        shakeY = cosf(fx->shakeTimer * 120.0f) * intensity;
    }

    Vector3 desiredPos = {
        plane->position.x - camFwd.x * totalDist + camRight.x * (fx->lateralLag + shakeX),
        plane->position.y - camFwd.y * totalDist + baseHeight + fx->verticalLag + shakeY,
        plane->position.z - camFwd.z * totalDist + camRight.z * (fx->lateralLag + shakeX)
    };

    // Suivi élastique de la position caméra
    fx->currentPos = Vector3Lerp(fx->currentPos, desiredPos, 14.0f * dt);

    // Point visé par la caméra : aligné sur la convergence des tirs à 240m
    Vector3 desiredTarget = {
        plane->position.x + planeFwd.x * 240.0f,
        plane->position.y + planeFwd.y * 240.0f + fx->recoilPitch * 110.0f + shakeY * 4.0f,
        plane->position.z + planeFwd.z * 240.0f
    };
    fx->currentTarget = Vector3Lerp(fx->currentTarget, desiredTarget, 24.0f * dt);

    // 5. Inclinaison de l'horizon (Banking) : la caméra penche à 38% du roulis de l'avion
    float camRoll = fx->smoothedRoll * 0.38f;
    Vector3 tiltedUp = {
        worldUp.x * cosf(camRoll) + camRight.x * sinf(camRoll),
        worldUp.y * cosf(camRoll) + camRight.y * sinf(camRoll),
        worldUp.z * cosf(camRoll) + camRight.z * sinf(camRoll)
    };

    // 6. FOV dynamique qui s'ouvre avec la vitesse et l'accélération
    float targetFov = 58.0f + (plane->speed / 120.0f) * 11.0f + (accelDelta > 0.0f ? accelDelta * 0.15f : 0.0f);
    if (targetFov < 56.0f) targetFov = 56.0f;
    if (targetFov > 74.0f) targetFov = 74.0f;
    fx->currentFov += (targetFov - fx->currentFov) * (6.0f * dt);

    camera->position = fx->currentPos;
    camera->target   = fx->currentTarget;
    camera->up       = Vector3Normalize(tiltedUp);
    camera->fovy     = fx->currentFov;
}