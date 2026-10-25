#pragma once
#include "fa_types.h"
#include <stdbool.h>

#define MAX_BUILDINGS 200

typedef struct {
    Vector3 position;
    Vector3 size;
    Vector3 min;
    Vector3 max;
    Color color;
    int hp;
    int max_hp;
    bool is_destroyed;
} Building;

typedef struct {
    Building buildings[MAX_BUILDINGS];
    int building_count;
    int total_max_hp;
    Vector2 ground_size;
    unsigned int seed;
} GameMap;

void InitMap(GameMap *map, unsigned int seed);
int GetCityRemainingBuildings(const GameMap *map);
float GetCityHpPercent(const GameMap *map);
void DrawMap(const GameMap *map);