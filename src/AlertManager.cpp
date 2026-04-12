#include "AlertManager.h"
#include "Logger.h"
#include <M5Cardputer.h>
#include <FastLED.h>

// FastLED globals
static CRGB leds[NUM_LEDS];

AlertManager::AlertManager()
    : hwConfig(nullptr), alertActive(false), alertStartTime(0), initialized(false), soundMuted(false), toneSeqLen(0), toneSeqIdx(0), toneNoteStart(0), toneSeqPlaying(false)
{
}

void AlertManager::begin(HardwareConfig &config)
{
    hwConfig = &config;

    // Initialize FastLED for SK6812 on LED_PIN
    FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(hwConfig->led_brightness);

    this->clearLED();

    // Set screen brightness
    M5Cardputer.Display.setBrightness(hwConfig->screen_brightness);

    initialized = true;
    logger.debugPrintln("[AlertManager] Initialized");
}

void AlertManager::triggerAlert()
{
    if (!initialized || !hwConfig)
        return;

    this->setBuzzer(hwConfig->buzzer_freq, hwConfig->buzzer_duration_ms);
    this->redLED();

    alertActive = true;
    alertStartTime = millis();
}

void AlertManager::newAPBeep()
{
    if (!initialized || !hwConfig || !hwConfig->buzzer_enabled)
        return;
    if (!hwConfig->beep_on_new_ssid)
        return;
    this->setBuzzer(hwConfig->buzzer_freq, hwConfig->buzzer_duration_ms);
    alertActive = true;
    alertStartTime = millis();
}

void AlertManager::geoFencedBeep()
{
    if (!initialized || !hwConfig || !hwConfig->buzzer_enabled)
        return;
    if (!hwConfig->beep_on_blocked_ssid)
        return;
    this->setBuzzer(hwConfig->buzzer_freq / 2, hwConfig->buzzer_duration_ms);
    alertActive = true;
    alertStartTime = millis();
}

void AlertManager::geofenceAreaBeep()
{
    if (!initialized || !hwConfig || !hwConfig->buzzer_enabled)
        return;
    if (!hwConfig->beep_on_geofence)
        return;
    if (soundMuted)
        return;
    // Distinct low-frequency double-boop for geofence area warning
    static const ToneNote seq[] = {
        {BOOP_FREQ / 2, QUICK_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ / 2, QUICK_TONE_MS}};
    playSequence(seq, 3);
}

void AlertManager::update()
{
    // Drive tone sequence playback
    if (toneSeqPlaying)
    {
        if (millis() - toneNoteStart >= (unsigned long)toneSequence[toneSeqIdx].duration_ms)
        {
            toneSeqIdx++;
            if (toneSeqIdx >= toneSeqLen)
            {
                toneSeqPlaying = false;
            }
            else
            {
                toneNoteStart = millis();
                if (toneSequence[toneSeqIdx].freq > 0)
                {
                    M5Cardputer.Speaker.tone(toneSequence[toneSeqIdx].freq, toneSequence[toneSeqIdx].duration_ms);
                }
            }
        }
    }

    if (!alertActive)
        return;

    if (millis() - alertStartTime >= (unsigned long)hwConfig->buzzer_duration_ms)
    {
        alertActive = false;
    }
}

void AlertManager::beep()
{
    if (!initialized || !hwConfig || !hwConfig->buzzer_enabled)
        return;
    this->setBuzzer(hwConfig->buzzer_freq, hwConfig->buzzer_duration_ms);
}

void AlertManager::setBuzzer(int freq, int duration_ms)
{
    if (!initialized || !hwConfig || !hwConfig->buzzer_enabled)
        return;
    if (soundMuted)
        return;

    M5Cardputer.Speaker.tone(freq, duration_ms);
}

void AlertManager::toggleMute()
{
    soundMuted = !soundMuted;
    logger.debugPrintln(soundMuted ? "[AlertManager] Sound muted" : "[AlertManager] Sound unmuted");
}

// ── Tone Sequence Player ────────────────────────────────────────────────────
void AlertManager::playSequence(const ToneNote *notes, int count)
{
    if (!initialized || !hwConfig || !hwConfig->buzzer_enabled)
        return;
    if (soundMuted)
        return;
    if (count <= 0 || count > MAX_TONE_SEQUENCE)
        return;

    for (int i = 0; i < count; i++)
    {
        toneSequence[i] = notes[i];
    }
    toneSeqLen = count;
    toneSeqIdx = 0;
    toneNoteStart = millis();
    toneSeqPlaying = true;

    // Play first note immediately
    if (toneSequence[0].freq > 0)
    {
        M5Cardputer.Speaker.tone(toneSequence[0].freq, toneSequence[0].duration_ms);
    }
}

// ── Named Sound Methods ─────────────────────────────────────────────────────

void AlertManager::soundStop()
{ // beep boop
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BEEP_FREQ, SHORT_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ, SHORT_TONE_MS}};
    playSequence(seq, 3);
}

void AlertManager::soundStart()
{ // boop beep
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BOOP_FREQ, SHORT_TONE_MS}, {0, TONE_GAP_MS}, {BEEP_FREQ, SHORT_TONE_MS}};
    playSequence(seq, 3);
}

void AlertManager::soundPause()
{ // boop boop
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BOOP_FREQ, SHORT_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ, SHORT_TONE_MS}};
    playSequence(seq, 3);
}

void AlertManager::soundResume()
{ // beep beep
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BEEP_FREQ, SHORT_TONE_MS}, {0, TONE_GAP_MS}, {BEEP_FREQ, SHORT_TONE_MS}};
    playSequence(seq, 3);
}

void AlertManager::soundShutdown()
{ // boop boop boop
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BOOP_FREQ, SHORT_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ, SHORT_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ, SHORT_TONE_MS}};
    playSequence(seq, 5);
}

void AlertManager::soundHelp()
{ // long boop (1s)
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {{BOOP_FREQ, LONG_TONE_MS}};
    playSequence(seq, 1);
}

void AlertManager::soundQuitHelp()
{ // long beep (1s)
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {{BEEP_FREQ, LONG_TONE_MS}};
    playSequence(seq, 1);
}

void AlertManager::soundBlankDisplay()
{ // three quick short boops
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BOOP_FREQ, QUICK_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ, QUICK_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ, QUICK_TONE_MS}};
    playSequence(seq, 5);
}

void AlertManager::soundEnableDisplay()
{ // three quick short beeps
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BEEP_FREQ, QUICK_TONE_MS}, {0, TONE_GAP_MS}, {BEEP_FREQ, QUICK_TONE_MS}, {0, TONE_GAP_MS}, {BEEP_FREQ, QUICK_TONE_MS}};
    playSequence(seq, 5);
}

void AlertManager::soundNextView()
{ // normal beep
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {{BEEP_FREQ, SHORT_TONE_MS}};
    playSequence(seq, 1);
}

void AlertManager::soundConfigMode()
{ // two long boops (500ms each)
    if (!hwConfig || !hwConfig->keyboard_sounds)
        return;
    static const ToneNote seq[] = {
        {BOOP_FREQ, LONG_CONFIG_TONE_MS}, {0, TONE_GAP_MS}, {BOOP_FREQ, LONG_CONFIG_TONE_MS}};
    playSequence(seq, 3);
}

void AlertManager::setStatusSearching()
{
    this->yellowLED();
}

void AlertManager::setStatusScanning()
{
    this->blueLED();
}

void AlertManager::setStatusReady()
{
    this->greenLED();
}

void AlertManager::setStatusError()
{
    this->redLED();
}

void AlertManager::setStatusConfig()
{
    this->purpleLED();
}

void AlertManager::setLED(uint32_t color)
{
    if (!initialized)
        return;
    leds[0] = CRGB(color);
    FastLED.show();
}

void AlertManager::clearLED()
{
    this->setLED(CRGB::Black);
}

void AlertManager::redLED()
{
    this->setLED(CRGB::Red);
}

void AlertManager::blueLED()
{
    this->setLED(CRGB::Blue);
}

void AlertManager::greenLED()
{
    this->setLED(CRGB::Green);
}

void AlertManager::yellowLED()
{
    this->setLED(CRGB::Yellow);
}

void AlertManager::purpleLED()
{
    this->setLED(CRGB::Purple);
}