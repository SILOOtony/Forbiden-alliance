#pragma once

#include "raylib.h"
#include <stdbool.h>

#define MAX_BUILDINGS 200

typedef struct {
    Vector3 position;
    Vector3 size;
    Vector3 min;
    Vector3 max;
    Color color;
    int hp;
    bool is_destroyed;
} Building;

typedef struct {
    Building buildings[MAX_BUILDINGS];
    int building_count;
    Vector2 ground_size;
    unsigned int seed; // La graine qui a généré cette carte
} GameMap;

void InitMap(GameMap *map, unsigned int seed);
void DrawMap(const GameMap *map);