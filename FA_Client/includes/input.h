#pragma once
#include "raylib.h"

typedef enum {
    ACTION_UP,
    ACTION_DOWN,
    ACTION_LEFT,
    ACTION_RIGHT,
    ACTION_ATTACK,
    ACTION_SLOT_1,
    ACTION_SLOT_2,
    ACTION_SLOT_3,
    ACTION_COUNT 
} GameAction;     

typedef struct {
    int bindings[ACTION_COUNT];
} KeyBindings;

KeyBindings InitKeyBinds(void);