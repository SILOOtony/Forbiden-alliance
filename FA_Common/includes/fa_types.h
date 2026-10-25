#pragma once

#ifdef SERVER_BUILD
    #ifndef PI
        #define PI 3.14159265358979323846f
    #endif
    #ifndef DEG2RAD
        #define DEG2RAD (PI / 180.0f)
    #endif
    #ifndef RAD2DEG
        #define RAD2DEG (180.0f / PI)
    #endif

    typedef struct Vector2 {
        float x;
        float y;
    } Vector2;

    typedef struct Vector3 {
        float x;
        float y;
        float z;
    } Vector3;

    typedef struct Color {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    } Color;

    typedef struct BoundingBox {
        Vector3 min;
        Vector3 max;
    } BoundingBox;
#else
    #include "raylib.h"
#endif