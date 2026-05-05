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

KeyAction KeyboardHandler::poll(AppState currentState,
                                bool helpVisible,
                                bool displayBlank,
                                DisplayView currentView,
                                bool shutdownUploadAvailable,
                                bool uploadActive)
{
    if (currentState == STATE_LOW_BATTERY_WARNING)
    {
        return KEY_ACTION_NONE;
    }

    if (currentState == STATE_UPLOAD_RUN || uploadActive)
    {
        if (M5Cardputer.BtnA.wasPressed())
        {
            return KEY_ACTION_CANCEL_UPLOAD;
        }
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
        {
            Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            // ESC key — Cardputer reports as 0x1B in word vector.
            for (auto key : keys.word)
            {
                if (key == 0x1B)
                {
                    return KEY_ACTION_CANCEL_UPLOAD;
                }
            }
        }
        // Note: when uploadActive is true (shutdown-driven upload), we still
        // fall through below if no cancel was pressed, so other shutdown
        // keys (G0 already handled in main) and the upload module's own
        // any-key path on COMPLETE/ERROR continue to work.
        if (currentState == STATE_UPLOAD_RUN)
        {
            return KEY_ACTION_NONE;
        }
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
            if (shutdownUploadAvailable && matchesKey(key, 'u'))
            {
                return KEY_ACTION_SHUTDOWN_UPLOAD;
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
        if (key == ' ')
        {
            return KEY_ACTION_SUB_VIEW;
        }
    }

    return KEY_ACTION_NONE;
}