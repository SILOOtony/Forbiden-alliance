#include "map.h"

static unsigned int fa_rng_state = 1;

static void FA_SetSeed(unsigned int seed) {
    fa_rng_state = seed;
}

static int FA_GetRandomValue(int min, int max) {
    if (min > max) { int tmp = max; max = min; min = tmp; }
    fa_rng_state = fa_rng_state * 1103515245u + 12345u;
    unsigned int rand_val = (fa_rng_state / 65536u) % 32768u;
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

void InitMap(GameMap *map, unsigned int seed) {
    map->seed = seed;
    FA_SetSeed(seed);

    map->building_count = 0;
    map->ground_size = (Vector2){ 2000.0f, 2000.0f };

    int grid_size = 14;
    float spacing = 60.0f;
    float start_offset = -(grid_size * spacing) / 2.0f;

    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            if (map->building_count >= MAX_BUILDINGS) break;
            if (FA_GetRandomValue(0, 10) < 2) continue;

            Building *b = &map->buildings[map->building_count];

            float width = (float)FA_GetRandomValue(15, 30);
            float length = (float)FA_GetRandomValue(15, 30);
            float height = (float)FA_GetRandomValue(20, 120);

            b->size = (Vector3){ width, height, length };
            b->position = (Vector3){
                start_offset + x * spacing + (float)FA_GetRandomValue(-5, 5),
                height / 2.0f,
                start_offset + z * spacing + (float)FA_GetRandomValue(-5, 5)
            };

            b->min = (Vector3){
                b->position.x - b->size.x / 2.0f,
                b->position.y - b->size.y / 2.0f,
                b->position.z - b->size.z / 2.0f
            };
            b->max = (Vector3){
                b->position.x + b->size.x / 2.0f,
                b->position.y + b->size.y / 2.0f,
                b->position.z + b->size.z / 2.0f
            };

            int tint = FA_GetRandomValue(100, 180);
            b->color = (Color){ tint, tint, tint + 15, 255 };
            b->hp = (int)(height * 2);
            b->is_destroyed = ((int)height % 3 == 0);

            map->building_count++;
        }
    }
}

#ifndef SERVER_BUILD
void DrawMap(const GameMap *map) {
    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, map->ground_size, (Color){ 35, 50, 40, 255 });
    DrawGrid(100, 20.0f);

    for (int i = 0; i < map->building_count; i++) {
        Building b = map->buildings[i];
        if (!b.is_destroyed) {
            DrawCubeV(b.position, b.size, b.color);
            DrawCubeWiresV(b.position, b.size, DARKGRAY);
        } else {
            Vector3 rubble_pos = { b.position.x, 2.0f, b.position.z };
            Vector3 rubble_size = { b.size.x, 4.0f, b.size.z };
            DrawCubeV(rubble_pos, rubble_size, DARKBROWN);
        }
    }
}
#endif