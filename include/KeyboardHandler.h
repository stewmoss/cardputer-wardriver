#ifndef KEYBOARD_HANDLER_H
#define KEYBOARD_HANDLER_H

#include "Config.h"

class KeyboardHandler
{
public:
    KeyboardHandler();
    KeyAction poll(AppState currentState, bool helpVisible, bool displayBlank);
};

#endif // KEYBOARD_HANDLER_H