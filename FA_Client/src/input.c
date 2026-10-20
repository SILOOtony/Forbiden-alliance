#include "input.h"

KeyBindings InitKeyBinds(void) {
    KeyBindings control;
    control.bindings[ACTION_UP]     = KEY_W;
    control.bindings[ACTION_DOWN]   = KEY_S;
    control.bindings[ACTION_LEFT]   = KEY_A;
    control.bindings[ACTION_RIGHT]  = KEY_D;
    control.bindings[ACTION_ATTACK] = KEY_LEFT_SHIFT;
    control.bindings[ACTION_SLOT_1] = KEY_ONE;
    control.bindings[ACTION_SLOT_2] = KEY_TWO;
    control.bindings[ACTION_SLOT_3] = KEY_THREE;
    return control;
}