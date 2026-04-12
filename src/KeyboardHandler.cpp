#include <M5Cardputer.h>

#include "KeyboardHandler.h"

namespace
{
bool matchesKey(char key, char lowerKey)
{
    return key == lowerKey || key == (lowerKey - ('a' - 'A'));
}
}

KeyboardHandler::KeyboardHandler()
{
}

KeyAction KeyboardHandler::poll(AppState currentState, bool helpVisible, bool displayBlank)
{
    if (currentState == STATE_LOW_BATTERY_WARNING)
    {
        return KEY_ACTION_NONE;
    }

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed())
    {
        return KEY_ACTION_NONE;
    }

    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

    if (currentState == STATE_SHUTDOWN)
    {
        for (auto key : keys.word)
        {
            if (matchesKey(key, 'b'))
            {
                return KEY_ACTION_TOGGLE_BLANK;
            }
        }
        return KEY_ACTION_NONE;
    }

    if (helpVisible)
    {
        for (auto key : keys.word)
        {
            if (matchesKey(key, 'h'))
            {
                return KEY_ACTION_DISMISS_HELP;
            }
        }
        return KEY_ACTION_NONE;
    }

    // Blank screen: only S (toggle mute) and B (unblank)
    if (displayBlank)
    {
        for (auto key : keys.word)
        {
            if (matchesKey(key, 's'))
            {
                return KEY_ACTION_TOGGLE_MUTE;
            }
            if (matchesKey(key, 'b'))
            {
                return KEY_ACTION_TOGGLE_BLANK;
            }
        }
        return KEY_ACTION_NONE;
    }

    // Satellite search screen: only S, B, Q, C
    if (currentState == STATE_SEARCHING_SATS)
    {
        for (auto key : keys.word)
        {
            if (matchesKey(key, 's'))
            {
                return KEY_ACTION_TOGGLE_MUTE;
            }
            if (matchesKey(key, 'b'))
            {
                return KEY_ACTION_TOGGLE_BLANK;
            }
            if (matchesKey(key, 'q'))
            {
                return KEY_ACTION_SHUTDOWN;
            }
            if (matchesKey(key, 'c'))
            {
                return KEY_ACTION_ENTER_CONFIG;
            }
        }
        return KEY_ACTION_NONE;
    }

    if (keys.enter)
    {
        return KEY_ACTION_NEXT_VIEW;
    }

    for (auto key : keys.word)
    {
        if (matchesKey(key, 'b'))
        {
            return KEY_ACTION_TOGGLE_BLANK;
        }
        if (matchesKey(key, 'c'))
        {
            return KEY_ACTION_ENTER_CONFIG;
        }
        if (matchesKey(key, 'q'))
        {
            return KEY_ACTION_SHUTDOWN;
        }
        if (matchesKey(key, 's'))
        {
            return KEY_ACTION_TOGGLE_MUTE;
        }
        if (matchesKey(key, 'h'))
        {
            return KEY_ACTION_SHOW_HELP;
        }
        if (matchesKey(key, 'x'))
        {
            return KEY_ACTION_TOGGLE_SCAN;
        }
        if (matchesKey(key, 'p'))
        {
            return KEY_ACTION_TOGGLE_PAUSE;
        }
    }

    return KEY_ACTION_NONE;
}