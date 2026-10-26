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

const char *GetCityName(uint8_t city_type) {
    switch (city_type % 4) {
        case CITY_PARIS:    return "PARIS";
        case CITY_NEW_YORK: return "NEW YORK";
        case CITY_TOKYO:    return "TOKYO";
        case CITY_DUBAI:    return "DUBAI";
        default:            return "PARIS";
    }
}

static void AddBuilding(GameMap *map, Vector3 pos, Vector3 size, Color col, int raw_hp) {
    if (map->building_count >= MAX_BUILDINGS) return;
    Building *b = &map->buildings[map->building_count];
    b->size = size;
    b->position = pos;
    b->min = (Vector3){ pos.x - size.x / 2.0f, pos.y - size.y / 2.0f, pos.z - size.z / 2.0f };
    b->max = (Vector3){ pos.x + size.x / 2.0f, pos.y + size.y / 2.0f, pos.z + size.z / 2.0f };
    b->color = col;
    b->hp = ((raw_hp + 14) / 15) * 15;
    b->max_hp = b->hp;
    b->is_destroyed = false;

    map->total_max_hp += b->max_hp;
    map->building_count++;
}

void InitMap(GameMap *map, unsigned int seed) {
    map->seed = seed;
    map->city_type = (uint8_t)(seed % 4);
    FA_SetSeed(seed);

    map->building_count = 0;
    map->total_max_hp = 0;
    map->ground_size = (Vector2){ 2000.0f, 2000.0f };

    // 1. Monuments emblématiques au centre selon la ville réelle choisie
    if (map->city_type == CITY_PARIS) {
        // Tour Eiffel (3 étages pyramidaux au bord de la Seine)
        Color eiffelCol = (Color){ 165, 120, 75, 255 };
        AddBuilding(map, (Vector3){ -70.0f,  25.0f, 0.0f }, (Vector3){ 34.0f,  50.0f, 34.0f }, eiffelCol, 240);
        AddBuilding(map, (Vector3){ -70.0f,  75.0f, 0.0f }, (Vector3){ 20.0f,  50.0f, 20.0f }, eiffelCol, 240);
        AddBuilding(map, (Vector3){ -70.0f, 135.0f, 0.0f }, (Vector3){  8.0f,  70.0f,  8.0f }, eiffelCol, 240);
        // Tour Montparnasse au sud
        AddBuilding(map, (Vector3){ 120.0f, 65.0f, 180.0f }, (Vector3){ 28.0f, 130.0f, 20.0f }, (Color){ 45, 50, 65, 255 }, 240);
    }
    else if (map->city_type == CITY_NEW_YORK) {
        // Empire State Building
        Color nyCol = (Color){ 150, 165, 180, 255 };
        AddBuilding(map, (Vector3){ -140.0f,  70.0f, -60.0f }, (Vector3){ 32.0f, 140.0f, 32.0f }, nyCol, 240);
        AddBuilding(map, (Vector3){ -140.0f, 165.0f, -60.0f }, (Vector3){ 10.0f,  50.0f, 10.0f }, (Color){ 210, 220, 230, 255 }, 240);
        // One World Trade Center
        AddBuilding(map, (Vector3){  150.0f,  90.0f,  80.0f }, (Vector3){ 30.0f, 180.0f, 30.0f }, (Color){ 120, 175, 215, 255 }, 240);
    }
    else if (map->city_type == CITY_TOKYO) {
        // Tokyo Tower (Rouge et Blanche)
        AddBuilding(map, (Vector3){ 0.0f,  35.0f, 0.0f }, (Vector3){ 30.0f, 70.0f, 30.0f }, (Color){ 220, 45, 35, 255 }, 240);
        AddBuilding(map, (Vector3){ 0.0f,  95.0f, 0.0f }, (Vector3){ 16.0f, 50.0f, 16.0f }, (Color){ 240, 240, 245, 255 }, 240);
        AddBuilding(map, (Vector3){ 0.0f, 145.0f, 0.0f }, (Vector3){  7.0f, 50.0f,  7.0f }, (Color){ 220, 45, 35, 255 }, 240);
    }
    else if (map->city_type == CITY_DUBAI) {
        // Burj Khalifa (Immense tour à étages)
        Color dubaiCol = (Color){ 170, 210, 235, 255 };
        AddBuilding(map, (Vector3){ 0.0f,  45.0f, 0.0f }, (Vector3){ 36.0f,  90.0f, 36.0f }, dubaiCol, 240);
        AddBuilding(map, (Vector3){ 0.0f, 120.0f, 0.0f }, (Vector3){ 22.0f,  60.0f, 22.0f }, dubaiCol, 240);
        AddBuilding(map, (Vector3){ 0.0f, 185.0f, 0.0f }, (Vector3){ 10.0f,  70.0f, 10.0f }, (Color){ 225, 240, 255, 255 }, 240);
    }

    // 2. Génération des quartiers propres à chaque ville
    int grid_size = 14;
    float spacing = 62.0f;
    float start_offset = -(grid_size * spacing) / 2.0f;

    for (int x = 0; x < grid_size; x++) {
        for (int z = 0; z < grid_size; z++) {
            if (map->building_count >= MAX_BUILDINGS) break;

            float wx = start_offset + x * spacing + (float)FA_GetRandomValue(-5, 5);
            float wz = start_offset + z * spacing + (float)FA_GetRandomValue(-5, 5);

            // PARIS : La Seine coule au centre (pas de bâtiments dans le lit du fleuve)
            if (map->city_type == CITY_PARIS && (wx > -45.0f && wx < 45.0f)) continue;
            if (map->city_type == CITY_PARIS && (wx > -105.0f && wx < -35.0f && wz > -35.0f && wz < 35.0f)) continue;

            // NEW YORK : Central Park au milieu (grand rectangle vert dégagé)
            if (map->city_type == CITY_NEW_YORK && (wx > -110.0f && wx < 110.0f && wz > -160.0f && wz < 160.0f)) continue;

            // TOKYO / DUBAI : Place dégagée autour de la tour centrale
            if ((map->city_type == CITY_TOKYO || map->city_type == CITY_DUBAI) &&
                (wx > -55.0f && wx < 55.0f && wz > -55.0f && wz < 55.0f)) continue;

            if (FA_GetRandomValue(0, 10) < 2) continue;

            float width  = (float)FA_GetRandomValue(16, 30);
            float length = (float)FA_GetRandomValue(16, 30);
            float height = 40.0f;
            Color col    = (Color){ 140, 140, 155, 255 };

            if (map->city_type == CITY_PARIS) {
                // Immeubles haussmanniens bas et réguliers couleur pierre crème
                height = (float)FA_GetRandomValue(22, 48);
                int stone = FA_GetRandomValue(185, 220);
                col = (Color){ (unsigned char)stone, (unsigned char)(stone - 10), (unsigned char)(stone - 30), 255 };
            }
            else if (map->city_type == CITY_NEW_YORK) {
                // Gratte-ciels très hauts de Manhattan
                height = (float)FA_GetRandomValue(55, 120);
                int glass = FA_GetRandomValue(95, 160);
                col = (Color){ (unsigned char)glass, (unsigned char)(glass + 15), (unsigned char)(glass + 35), 255 };
            }
            else if (map->city_type == CITY_TOKYO) {
                // Tours néon cyber-urbaines
                height = (float)FA_GetRandomValue(35, 110);
                int pick = FA_GetRandomValue(0, 5);
                if (pick == 0) col = (Color){ 190, 40, 140, 255 };
                else if (pick == 1) col = (Color){ 35, 175, 210, 255 };
                else {
                    int dark = FA_GetRandomValue(50, 90);
                    col = (Color){ (unsigned char)dark, (unsigned char)dark, (unsigned char)(dark + 20), 255 };
                }
            }
            else {
                // Dubaï : Tours luxueuses dorées et blanches
                height = (float)FA_GetRandomValue(40, 115);
                int sand = FA_GetRandomValue(195, 235);
                col = (Color){ (unsigned char)sand, (unsigned char)(sand - 10), (unsigned char)(sand - 35), 255 };
            }

            AddBuilding(map, (Vector3){ wx, height / 2.0f, wz }, (Vector3){ width, height, length }, col, (int)(height * 2));
        }
    }
}

int GetCityRemainingBuildings(const GameMap *map) {
    int count = 0;
    for (int i = 0; i < map->building_count; i++) {
        if (!map->buildings[i].is_destroyed && map->buildings[i].hp > 0) count++;
    }
    return count;
}

float GetCityHpPercent(const GameMap *map) {
    if (map->total_max_hp <= 0) return 0.0f;
    int current_hp = 0;
    for (int i = 0; i < map->building_count; i++) {
        if (!map->buildings[i].is_destroyed && map->buildings[i].hp > 0) {
            current_hp += map->buildings[i].hp;
        }
    }
    return ((float)current_hp * 100.0f) / (float)map->total_max_hp;
}

#ifndef SERVER_BUILD
void DrawMap(const GameMap *map) {
    Color groundCol = (Color){ 35, 50, 40, 255 };
    if (map->city_type == CITY_PARIS)    groundCol = (Color){ 48, 55, 48, 255 };
    if (map->city_type == CITY_NEW_YORK) groundCol = (Color){ 42, 45, 52, 255 };
    if (map->city_type == CITY_TOKYO)    groundCol = (Color){ 25, 25, 38, 255 };
    if (map->city_type == CITY_DUBAI)    groundCol = (Color){ 195, 165, 110, 255 };

    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, map->ground_size, groundCol);

    // Décors au sol spécifiques (La Seine à Paris, Central Park à New York, Lagune à Dubaï)
    if (map->city_type == CITY_PARIS) {
        DrawCube((Vector3){ 0.0f, 0.1f, 0.0f }, 75.0f, 0.2f, 2000.0f, (Color){ 30, 105, 175, 255 });
    } else if (map->city_type == CITY_NEW_YORK) {
        DrawCube((Vector3){ 0.0f, 0.1f, 0.0f }, 210.0f, 0.2f, 310.0f, (Color){ 35, 125, 45, 255 });
    } else if (map->city_type == CITY_DUBAI) {
        DrawCube((Vector3){ 220.0f, 0.1f, 0.0f }, 160.0f, 0.2f, 2000.0f, (Color){ 20, 145, 185, 255 });
    }

    DrawGrid(100, 20.0f);

    for (int i = 0; i < map->building_count; i++) {
        Building b = map->buildings[i];
        if (!b.is_destroyed) {
            DrawCubeV(b.position, b.size, b.color);
            DrawCubeWiresV(b.position, b.size, (map->city_type == CITY_TOKYO) ? SKYBLUE : DARKGRAY);
        } else {
            Vector3 rubble_pos = { b.position.x, 2.0f, b.position.z };
            Vector3 rubble_size = { b.size.x, 4.0f, b.size.z };
            DrawCubeV(rubble_pos, rubble_size, DARKBROWN);
        }
    }
}
#endif