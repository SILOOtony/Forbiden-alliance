#include "map.h"

void InitMap(GameMap *map, unsigned int seed) {
    map->seed = seed;
    SetRandomSeed(seed); // Force la même suite de nombres aléatoires partout !

    map->building_count = 0;
    map->ground_size = (Vector2){ 2000.0f, 2000.0f };

    int grid_size = 14;
    float spacing = 60.0f;
    float start_offset = -(grid_size * spacing) / 2.0f;

    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            if (map->building_count >= MAX_BUILDINGS) break;

            if (GetRandomValue(0, 10) < 2) continue;

            Building *b = &map->buildings[map->building_count];

            float width = (float)GetRandomValue(15, 30);
            float length = (float)GetRandomValue(15, 30);
            float height = (float)GetRandomValue(20, 120);

            b->size = (Vector3){ width, height, length };

            b->position = (Vector3){
                start_offset + x * spacing + (float)GetRandomValue(-5, 5),
                height / 2.0f,
                start_offset + z * spacing + (float)GetRandomValue(-5, 5)
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

            int tint = GetRandomValue(100, 180);
            b->color = (Color){ tint, tint, tint + 15, 255 };

            b->hp = (int)(height * 2);
            if ((int)height % 3 == 0) {
                b->is_destroyed = true;
            } else {
                b->is_destroyed = false;
            }

            map->building_count++;
        }
    }
}

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