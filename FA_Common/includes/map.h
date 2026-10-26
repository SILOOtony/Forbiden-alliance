#pragma once
#include "fa_types.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_BUILDINGS 200

typedef enum {
    CITY_PARIS    = 0,
    CITY_NEW_YORK = 1,
    CITY_TOKYO    = 2,
    CITY_DUBAI    = 3
} CityType;

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
    uint8_t city_type;
} GameMap;

void InitMap(GameMap *map, unsigned int seed);
const char *GetCityName(uint8_t city_type);
int GetCityRemainingBuildings(const GameMap *map);
float GetCityHpPercent(const GameMap *map);
void DrawMap(const GameMap *map);