#pragma once
#include "raylib.h"
#include "player.h"
#include "weaponSlot.h"

typedef struct {
    bool initialized;

    // Inertie de vitesse et d'angles (système ressort / amortisseur)
    float smoothedSpeed;
    float smoothedPitch;
    float smoothedYaw;
    float smoothedRoll;
    float verticalLag;
    float lateralLag;

    // Position et cible lissées de la caméra
    Vector3 currentPos;
    Vector3 currentTarget;
    float currentFov;

    // Effet de recul (Recoil) et secousse au tir
    float recoilBack;
    float recoilPitch;
    float shakeAmplitude;
    float shakeTimer;
} PlaneCameraFX;

// Initialise ou réinitialise la caméra dynamique sur l'avion
void InitPlaneCameraFX(PlaneCameraFX *fx, const PlayerState *plane);

// Déclenche une impulsion de recul selon le calibre de l'arme tirée
void TriggerCameraRecoil(PlaneCameraFX *fx, WeaponType weaponType);

// Met à jour la caméra 3D à 60 Hz (style Fortnite Saison 7)
void UpdatePlaneCameraFX(Camera3D *camera, PlaneCameraFX *fx, const PlayerState *plane, float dt);