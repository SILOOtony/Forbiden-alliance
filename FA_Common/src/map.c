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
    map->drone_rng = 98765u ^ seed;
    map->city_type = (uint8_t)(seed % 4);
    FA_SetSeed(seed);

    map->building_count = 0;
    map->total_max_hp = 0;
    map->ground_size = (Vector2){ 2000.0f, 2000.0f };

    // 1. Monuments ancrés au sol
    if (map->city_type == CITY_PARIS) {
        Color eiffelBase = (Color){ 155, 110, 65, 255 };
        Color eiffelTop  = (Color){ 175, 130, 85, 255 };
        AddBuilding(map, (Vector3){ -70.0f, 25.0f, 0.0f }, (Vector3){ 34.0f,  50.0f, 34.0f }, eiffelBase, 240);
        AddBuilding(map, (Vector3){ -70.0f, 52.5f, 0.0f }, (Vector3){ 20.0f, 105.0f, 20.0f }, eiffelTop,  240);
        AddBuilding(map, (Vector3){ -70.0f, 85.0f, 0.0f }, (Vector3){  8.0f, 170.0f,  8.0f }, eiffelBase, 240);
        AddBuilding(map, (Vector3){ 120.0f, 65.0f, 180.0f }, (Vector3){ 28.0f, 130.0f, 20.0f }, (Color){ 45, 50, 65, 255 }, 240);
    }
    else if (map->city_type == CITY_NEW_YORK) {
        Color nyCol = (Color){ 150, 165, 180, 255 };
        AddBuilding(map, (Vector3){ -140.0f, 70.0f, -60.0f }, (Vector3){ 32.0f, 140.0f, 32.0f }, nyCol, 240);
        AddBuilding(map, (Vector3){ -140.0f, 95.0f, -60.0f }, (Vector3){ 12.0f, 190.0f, 12.0f }, (Color){ 210, 220, 230, 255 }, 240);
        AddBuilding(map, (Vector3){  150.0f, 90.0f,  80.0f }, (Vector3){ 30.0f, 180.0f, 30.0f }, (Color){ 120, 175, 215, 255 }, 240);
    }
    else if (map->city_type == CITY_TOKYO) {
        AddBuilding(map, (Vector3){ 0.0f, 35.0f, 0.0f }, (Vector3){ 30.0f,  70.0f, 30.0f }, (Color){ 220, 45, 35, 255 }, 240);
        AddBuilding(map, (Vector3){ 0.0f, 60.0f, 0.0f }, (Vector3){ 16.0f, 120.0f, 16.0f }, (Color){ 240, 240, 245, 255 }, 240);
        AddBuilding(map, (Vector3){ 0.0f, 85.0f, 0.0f }, (Vector3){  7.0f, 170.0f,  7.0f }, (Color){ 220, 45, 35, 255 }, 240);
    }
    else if (map->city_type == CITY_DUBAI) {
        Color dubaiCol = (Color){ 170, 210, 235, 255 };
        AddBuilding(map, (Vector3){ 0.0f,  45.0f, 0.0f }, (Vector3){ 36.0f,  90.0f, 36.0f }, dubaiCol, 240);
        AddBuilding(map, (Vector3){ 0.0f,  75.0f, 0.0f }, (Vector3){ 22.0f, 150.0f, 22.0f }, dubaiCol, 240);
        AddBuilding(map, (Vector3){ 0.0f, 110.0f, 0.0f }, (Vector3){ 10.0f, 220.0f, 10.0f }, (Color){ 225, 240, 255, 255 }, 240);
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

            if (wx > -52.0f && wx < 52.0f && wz > 55.0f && wz < 160.0f) continue;

            if (map->city_type == CITY_PARIS && (wx > -45.0f && wx < 45.0f)) continue;
            if (map->city_type == CITY_PARIS && (wx > -105.0f && wx < -35.0f && wz > -35.0f && wz < 35.0f)) continue;
            if (map->city_type == CITY_NEW_YORK && (wx > -110.0f && wx < 110.0f && wz > -160.0f && wz < 160.0f)) continue;
            if ((map->city_type == CITY_TOKYO || map->city_type == CITY_DUBAI) &&
                (wx > -55.0f && wx < 55.0f && wz > -55.0f && wz < 55.0f)) continue;

            if (FA_GetRandomValue(0, 10) < 2) continue;

            float width  = (float)FA_GetRandomValue(16, 30);
            float length = (float)FA_GetRandomValue(16, 30);
            float height = 40.0f;
            Color col    = (Color){ 140, 140, 155, 255 };

            if (map->city_type == CITY_PARIS) {
                height = (float)FA_GetRandomValue(22, 48);
                int stone = FA_GetRandomValue(185, 220);
                col = (Color){ (unsigned char)stone, (unsigned char)(stone - 10), (unsigned char)(stone - 30), 255 };
            }
            else if (map->city_type == CITY_NEW_YORK) {
                height = (float)FA_GetRandomValue(55, 120);
                int glass = FA_GetRandomValue(95, 160);
                col = (Color){ (unsigned char)glass, (unsigned char)(glass + 15), (unsigned char)(glass + 35), 255 };
            }
            else if (map->city_type == CITY_TOKYO) {
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
static Color ScaleColorBrightness(Color c, float factor) {
    int r = (int)((float)c.r * factor);
    int g = (int)((float)c.g * factor);
    int b = (int)((float)c.b * factor);
    if (r > 255) r = 255; if (r < 0) r = 0;
    if (g > 255) g = 255; if (g < 0) g = 0;
    if (b > 255) b = 255; if (b < 0) b = 0;
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}

void DrawMap(const GameMap *map) {
    // 1. Sol naturel extérieur + centre urbain en asphalte (sans grille de debug !)
    Color outerGround = (Color){ 42, 68, 45, 255 };
    Color asphaltCol  = (Color){ 36, 38, 44, 255 };
    Color sidewalkCol = (Color){ 115, 118, 124, 255 };

    if (map->city_type == CITY_PARIS) {
        outerGround = (Color){ 52, 74, 48, 255 };
        asphaltCol  = (Color){ 46, 48, 54, 255 };
    } else if (map->city_type == CITY_NEW_YORK) {
        outerGround = (Color){ 38, 52, 42, 255 };
        asphaltCol  = (Color){ 34, 37, 44, 255 };
    } else if (map->city_type == CITY_TOKYO) {
        outerGround = (Color){ 18, 20, 32, 255 };
        asphaltCol  = (Color){ 22, 24, 34, 255 };
        sidewalkCol = (Color){ 65, 70, 88, 255 };
    } else if (map->city_type == CITY_DUBAI) {
        outerGround = (Color){ 195, 165, 110, 255 };
        asphaltCol  = (Color){ 52, 50, 55, 255 };
        sidewalkCol = (Color){ 170, 155, 130, 255 };
    }

    // Terrain extérieur (2000x2000m) + Dalle urbaine centrale (980x980m)
    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, map->ground_size, outerGround);
    DrawCube((Vector3){ 0.0f, 0.05f, 0.0f }, 980.0f, 0.1f, 980.0f, asphaltCol);

    // 2. Réseau d'avenues et marquages routiers
    int grid_size = 14;
    float spacing = 62.0f;
    float start_offset = -(grid_size * spacing) / 2.0f;
    Color laneCol = (map->city_type == CITY_TOKYO) ? (Color){ 0, 190, 235, 140 } : (Color){ 215, 195, 110, 140 };

    for (int g = 0; g <= grid_size; g++) {
        float roadCoord = start_offset + g * spacing - spacing * 0.5f;
        // Avenues Nord-Sud et Est-Ouest
        DrawCube((Vector3){ roadCoord, 0.09f, 0.0f }, 14.0f, 0.04f, 940.0f, (Color){ 26, 28, 32, 255 });
        DrawCube((Vector3){ 0.0f, 0.09f, roadCoord }, 940.0f, 0.04f, 14.0f, (Color){ 26, 28, 32, 255 });
        // Ligne centrale de voie
        DrawCube((Vector3){ roadCoord, 0.12f, 0.0f }, 0.8f, 0.04f, 920.0f, laneCol);
        DrawCube((Vector3){ 0.0f, 0.12f, roadCoord }, 920.0f, 0.04f, 0.8f, laneCol);
    }

    // 3. Fleuves, Parcs, Ponts et Arbres propres à chaque ville
    if (map->city_type == CITY_PARIS) {
        // La Seine + Quais en pierre + 5 Ponts parisiens + Champ-de-Mars
        DrawCube((Vector3){ 0.0f, 0.14f, 0.0f }, 86.0f, 0.18f, 2000.0f, (Color){ 165, 155, 135, 255 });
        DrawCube((Vector3){ 0.0f, 0.18f, 0.0f }, 72.0f, 0.22f, 2000.0f, (Color){ 28, 105, 175, 255 });
        DrawCube((Vector3){ -70.0f, 0.15f, 0.0f }, 75.0f, 0.15f, 120.0f, (Color){ 58, 125, 55, 255 });
        for (int b = -2; b <= 2; b++) {
            float bz = (float)b * 185.0f;
            DrawCube((Vector3){ 0.0f, 1.4f, bz }, 88.0f, 1.8f, 18.0f, (Color){ 185, 175, 155, 255 });
            DrawCubeWires((Vector3){ 0.0f, 2.2f, bz }, 88.0f, 2.2f, 18.0f, DARKGRAY);
        }
    }
    else if (map->city_type == CITY_NEW_YORK) {
        // Central Park avec lac, allées et arbres 3D
        DrawCube((Vector3){ 0.0f, 0.16f, 0.0f }, 218.0f, 0.22f, 318.0f, (Color){ 140, 145, 150, 255 });
        DrawCube((Vector3){ 0.0f, 0.22f, 0.0f }, 206.0f, 0.26f, 306.0f, (Color){ 42, 130, 52, 255 });
        DrawCube((Vector3){ 15.0f, 0.28f, -20.0f }, 75.0f, 0.28f, 95.0f, (Color){ 32, 115, 180, 255 });
        for (int tx = -80; tx <= 80; tx += 32) {
            for (int tz = -125; tz <= 125; tz += 35) {
                if (tx > -30 && tx < 60 && tz > -75 && tz < 35) continue;
                DrawCylinder((Vector3){ (float)tx, 0.2f, (float)tz }, 1.2f, 1.2f, 5.0f, 6, BROWN);
                DrawSphere((Vector3){ (float)tx, 7.5f, (float)tz }, 5.5f, (Color){ 30, 105, 40, 255 });
            }
        }
    }
    else if (map->city_type == CITY_DUBAI) {
        // Canal de Dubaï + Plages de sable + Ponts modernes
        DrawCube((Vector3){ 220.0f, 0.14f, 0.0f }, 182.0f, 0.18f, 2000.0f, (Color){ 225, 200, 145, 255 });
        DrawCube((Vector3){ 220.0f, 0.18f, 0.0f }, 156.0f, 0.22f, 2000.0f, (Color){ 18, 150, 195, 255 });
        for (int b = -1; b <= 1; b++) {
            DrawCube((Vector3){ 220.0f, 1.8f, (float)b * 240.0f }, 175.0f, 2.0f, 20.0f, WHITE);
        }
    }

    // 4. Montagnes / Collines en bordure de carte (ferme l'horizon à 950m)
    Color hillCol = ScaleColorBrightness(outerGround, 0.78f);
    for (int h = 0; h < 16; h++) {
        float pos = -900.0f + (float)h * 120.0f;
        float h1 = 55.0f + (float)((h * 37) % 65);
        float h2 = 60.0f + (float)((h * 53) % 75);
        DrawCube((Vector3){ pos, h1 * 0.5f, -960.0f }, 135.0f, h1, 90.0f, hillCol);
        DrawCube((Vector3){ pos, h2 * 0.5f,  960.0f }, 135.0f, h2, 90.0f, hillCol);
        DrawCube((Vector3){ -960.0f, h2 * 0.5f, pos }, 90.0f, h2, 135.0f, hillCol);
        DrawCube((Vector3){  960.0f, h1 * 0.5f, pos }, 90.0f, h1, 135.0f, hillCol);
    }

    // 5. Rendu architectural détaillé des bâtiments
    for (int i = 0; i < map->building_count; i++) {
        Building b = map->buildings[i];

        // Trottoir en béton au pied de chaque immeuble
        DrawCube((Vector3){ b.position.x, 0.35f, b.position.z }, b.size.x + 6.0f, 0.6f, b.size.z + 6.0f, sidewalkCol);

        if (!b.is_destroyed) {
            float hpRatio = (b.max_hp > 0) ? ((float)b.hp / (float)b.max_hp) : 1.0f;
            // L'immeuble s'assombrit progressivement quand il subit des dégâts
            Color mainCol = ScaleColorBrightness(b.color, 0.55f + 0.45f * hpRatio);
            Color darkTrim = ScaleColorBrightness(mainCol, 0.68f);
            Color windowBand = (map->city_type == CITY_TOKYO)
                ? (Color){ 110, 230, 255, 255 }
                : ScaleColorBrightness(mainCol, 1.25f);

            // A. Rez-de-chaussée / Socle renforcé
            float baseH = (b.size.y > 20.0f) ? 5.0f : (b.size.y * 0.2f);
            DrawCube((Vector3){ b.position.x, baseH * 0.5f, b.position.z },
                     b.size.x + 1.6f, baseH, b.size.z + 1.6f, darkTrim);

            // B. Corps principal de l'immeuble
            DrawCubeV(b.position, b.size, mainCol);
            DrawCubeWiresV(b.position, b.size, (map->city_type == CITY_TOKYO) ? SKYBLUE : DARKGRAY);

            // C. Bandeaux d'étages / Vitres architecturales tous les 12m de hauteur
            for (float fy = 10.0f; fy < b.size.y - 4.0f; fy += 11.0f) {
                DrawCube((Vector3){ b.position.x, fy, b.position.z },
                         b.size.x + 0.45f, 1.8f, b.size.z + 0.45f, windowBand);
            }

            // D. Corniche de toit + Superstructure (bloc clim / héliport / antenne)
            float roofY = b.position.y + b.size.y * 0.5f;
            DrawCube((Vector3){ b.position.x, roofY + 0.6f, b.position.z },
                     b.size.x + 1.4f, 1.2f, b.size.z + 1.4f, darkTrim);

            if (i >= 4) {
                DrawCube((Vector3){ b.position.x, roofY + 2.5f, b.position.z },
                         b.size.x * 0.42f, 3.2f, b.size.z * 0.42f, darkTrim);
                if ((i % 3) == 0 && b.size.y > 45.0f) {
                    // Antenne radio sur les grands immeubles
                    DrawCylinder((Vector3){ b.position.x, roofY + 4.0f, b.position.z },
                                 0.35f, 0.35f, 9.0f, 5, LIGHTGRAY);
                    DrawSphere((Vector3){ b.position.x, roofY + 13.2f, b.position.z }, 0.7f, RED);
                }
            }

            // E. Indicateur visuel de dégâts si l'immeuble est endommagé
            if (b.hp < b.max_hp) {
                Color dmgCol = (hpRatio > 0.5f) ? GOLD : RED;
                DrawCube((Vector3){ b.position.x, roofY + 6.5f, b.position.z },
                         b.size.x * hpRatio, 1.4f, 1.4f, dmgCol);
            }
        } else {
            // Décombres détaillés quand le bâtiment est détruit
            DrawCube((Vector3){ b.position.x, 1.8f, b.position.z }, b.size.x, 3.6f, b.size.z, DARKBROWN);
            DrawCube((Vector3){ b.position.x - 2.0f, 3.8f, b.position.z + 1.5f },
                     b.size.x * 0.45f, 2.8f, b.size.z * 0.45f, (Color){ 65, 55, 50, 255 });
            DrawCubeWires((Vector3){ b.position.x, 1.8f, b.position.z }, b.size.x, 3.6f, b.size.z, BLACK);
        }
    }
}
#endif