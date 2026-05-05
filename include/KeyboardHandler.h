#ifndef KEYBOARD_HANDLER_H
#define KEYBOARD_HANDLER_H

#include "Config.h"

class KeyboardHandler
{
public:
    KeyboardHandler();
    KeyAction poll(AppState currentState,
                   bool helpVisible,
                   bool displayBlank,
                   DisplayView currentView = VIEW_SEARCHING_SATS,
                   bool shutdownUploadAvailable = false,
                   bool uploadActive = false);
};

#endif // KEYBOARD_HANDLER_H