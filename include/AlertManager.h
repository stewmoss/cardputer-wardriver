#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>
#include "Config.h"

struct ToneNote
{
    int freq; // 0 = silent gap
    int duration_ms;
};

class AlertManager
{
public:
    AlertManager();
    void begin(HardwareConfig &config, bool superDebug = false);
    void triggerAlert();
    void newAPBeep();
    void geoFencedBeep();
    void geofenceAreaBeep();
    void update();
    bool isAlerting() { return alertActive; }
    void beep();
    void setBuzzer(int freq, int duration_ms);

    void toggleMute();
    bool isMuted() { return soundMuted; }

    // Tone sequence player (non-blocking)
    void playSequence(const ToneNote *notes, int count);

    // Named sound feedback methods
    void soundStop();          // beep boop
    void soundStart();         // boop beep
    void soundPause();         // boop boop
    void soundResume();        // beep beep
    void soundShutdown();      // boop boop boop
    void soundHelp();          // long boop (1s)
    void soundQuitHelp();      // long beep (1s)
    void soundBlankDisplay();  // three quick short boops
    void soundEnableDisplay(); // three quick short beeps
    void soundNextView();      // normal beep
    void soundConfigMode();    // two long boops (500ms each)

    // Named status LED states
    void setStatusSearching();
    void setStatusScanning();
    void setStatusReady();
    void setStatusError();
    void setStatusConfig();

    void clearLED();
    void redLED();
    void blueLED();
    void greenLED();
    void yellowLED();
    void purpleLED();
    void setLED(uint32_t color);

private:
    HardwareConfig *hwConfig;
    bool alertActive;
    unsigned long alertStartTime;
    bool initialized;
    bool soundMuted;
    bool superDebug;

    // Tone sequence state
    ToneNote toneSequence[MAX_TONE_SEQUENCE];
    int toneSeqLen;
    int toneSeqIdx;
    unsigned long toneNoteStart;
    bool toneSeqPlaying;
};

#endif // ALERT_MANAGER_H
