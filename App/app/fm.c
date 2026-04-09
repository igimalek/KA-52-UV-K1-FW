/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * OURO_KA52: simple FM, auto-seek only, no memory, no band switching
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifdef ENABLE_FMRADIO

#include <string.h>
#include "nav_invert.h"

#include "app/action.h"
#include "app/fm.h"
#include "app/generic.h"
#include "audio.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
#include "driver/py25q16.h"
#include "driver/system.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"

#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

// Fixed band: 87.5–108.0 MHz
#define FM_BAND  0

// ---- Globals (kept for settings.c / app.c / action.c) ----
uint16_t          gFM_Channels[20];
bool              gFmRadioMode;
uint8_t           gFmRadioCountdown_500ms;
volatile uint16_t gFmPlayCountdown_10ms;
volatile int8_t   gFM_ScanState;
bool              gFM_AutoScan;
uint8_t           gFM_ChannelPosition;
bool              gFM_FoundFrequency;
uint16_t          gFM_RestoreCountdown_10ms;
bool              gFM_ManualMode = false;
bool              gFM_Mute       = false;

const uint8_t BUTTON_STATE_PRESSED = 1 << 0;
const uint8_t BUTTON_STATE_HELD    = 1 << 1;
const uint8_t BUTTON_EVENT_PRESSED = BUTTON_STATE_PRESSED;
const uint8_t BUTTON_EVENT_HELD    = BUTTON_STATE_PRESSED | BUTTON_STATE_HELD;
const uint8_t BUTTON_EVENT_SHORT   = 0;
const uint8_t BUTTON_EVENT_LONG    = BUTTON_STATE_HELD;

// ---- Stubs for external callers ----

bool FM_CheckValidChannel(uint8_t Channel)
{
    return Channel < ARRAY_SIZE(gFM_Channels) &&
           gFM_Channels[Channel] >= BK1080_GetFreqLoLimit(FM_BAND) &&
           gFM_Channels[Channel] <  BK1080_GetFreqHiLimit(FM_BAND);
}

uint8_t FM_FindNextChannel(uint8_t Channel, uint8_t Direction)
{
    (void)Channel; (void)Direction;
    return 0xFF;
}

int FM_ConfigureChannelState(void)
{
    gEeprom.FM_IsMrMode         = false;
    gEeprom.FM_FrequencyPlaying = gEeprom.FM_SelectedFrequency;
    return 0;
}

void FM_EraseChannels(void)
{
    PY25Q16_SectorErase(0x003000);
    memset(gFM_Channels, 0xFF, sizeof(gFM_Channels));
}

// Stubs — not used, kept for app.c/scheduler
void FM_Tune(uint16_t Frequency, int8_t Step, bool bFlag)
{
    (void)Frequency; (void)Step; (void)bFlag;
}

void FM_PlayAndUpdate(void)
{
    gFM_ScanState = FM_SCAN_OFF;
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
}

int FM_CheckFrequencyLock(uint16_t Frequency, uint16_t LowerLimit)
{
    int ret = -1;
    const uint16_t Test2     = BK1080_ReadRegister(BK1080_REG_07);
    const uint16_t Deviation = BK1080_REG_07_GET_FREQD(Test2);

    if (BK1080_REG_07_GET_SNR(Test2) <= 2) {
        BK1080_FrequencyDeviation = Deviation;
        BK1080_BaseFrequency      = Frequency;
        return ret;
    }
    const uint16_t Status = BK1080_ReadRegister(BK1080_REG_10);
    if ((Status & BK1080_REG_10_MASK_AFCRL) != BK1080_REG_10_AFCRL_NOT_RAILED ||
        BK1080_REG_10_GET_RSSI(Status) < 10) {
        BK1080_FrequencyDeviation = Deviation;
        BK1080_BaseFrequency      = Frequency;
        return ret;
    }
    if (Deviation >= 280 && Deviation <= 3815) {
        BK1080_FrequencyDeviation = Deviation;
        BK1080_BaseFrequency      = Frequency;
        return ret;
    }
    if (Frequency > LowerLimit && (Frequency - BK1080_BaseFrequency) == 1) {
        if (BK1080_FrequencyDeviation & 0x800 || BK1080_FrequencyDeviation < 20) {
            BK1080_FrequencyDeviation = Deviation;
            BK1080_BaseFrequency      = Frequency;
            return ret;
        }
    }
    if (Frequency >= LowerLimit && (BK1080_BaseFrequency - Frequency) == 1) {
        if ((BK1080_FrequencyDeviation & 0x800) == 0 || BK1080_FrequencyDeviation > 4075) {
            BK1080_FrequencyDeviation = Deviation;
            BK1080_BaseFrequency      = Frequency;
            return ret;
        }
    }
    ret = 0;
    BK1080_FrequencyDeviation = Deviation;
    BK1080_BaseFrequency      = Frequency;
    return ret;
}

void FM_Play(void)
{
    GUI_SelectNextDisplay(DISPLAY_FM);
}

// ---- Synchronous seek: step through frequencies until station found ----

static void FM_SeekNext(int8_t direction)
{
    const uint16_t lo   = BK1080_GetFreqLoLimit(FM_BAND);
    const uint16_t hi   = BK1080_GetFreqHiLimit(FM_BAND);
    uint16_t       freq = gEeprom.FM_FrequencyPlaying;
    const uint16_t start = freq;

    // Mute audio while seeking
    AUDIO_AudioPathOff();
    gEnableSpeaker = false;

    do {
        freq += direction;
        if (freq < lo) freq = hi;
        if (freq > hi) freq = lo;

        BK1080_SetFrequency(freq, FM_BAND);
        SYSTEM_DelayMs(100);

        if (FM_CheckFrequencyLock(freq, lo) == 0) {
            // Station found — restore audio
            gEeprom.FM_FrequencyPlaying  = freq;
            gEeprom.FM_SelectedFrequency = freq;
            gRequestSaveFM = true;
            gRequestDisplayScreen = DISPLAY_FM;
            AUDIO_AudioPathOn();
            gEnableSpeaker = true;
            return;
        }

        // Full circle — nothing found, restore original
    } while (freq != start);

    // No station found — restore frequency, keep muted
    BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, FM_BAND);
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
}

// ---- Manual frequency step: 0.1 MHz ----

static void FM_StepFreq(int8_t direction)
{
    const uint16_t lo = BK1080_GetFreqLoLimit(FM_BAND);
    const uint16_t hi = BK1080_GetFreqHiLimit(FM_BAND);
    uint16_t freq = gEeprom.FM_FrequencyPlaying + direction;
    if (freq < lo) freq = hi;
    if (freq > hi) freq = lo;

    gEeprom.FM_FrequencyPlaying  = freq;
    gEeprom.FM_SelectedFrequency = freq;
    BK1080_SetFrequency(freq, FM_BAND);
    gRequestSaveFM = true;
    gRequestDisplayScreen = DISPLAY_FM;
}

// ---- Key handling ----

void FM_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    uint8_t state = bKeyPressed + 2 * bKeyHeld;

    switch (Key) {
        case KEY_UP:
            if (state == BUTTON_EVENT_SHORT) {
                int8_t dir = NAV_DIR(-1); // UP direction (inverted if ENABLE_INVERT_NAV)
                if (gFM_ManualMode)
                    FM_StepFreq(dir);
                else
                    FM_SeekNext(dir);
            }
            break;
        case KEY_DOWN:
            if (state == BUTTON_EVENT_SHORT) {
                int8_t dir = NAV_DIR(1);  // DOWN direction (inverted if ENABLE_INVERT_NAV)
                if (gFM_ManualMode)
                    FM_StepFreq(dir);
                else
                    FM_SeekNext(dir);
            }
            break;
        case KEY_STAR:
            if (state == BUTTON_EVENT_HELD) {
                // Долгое нажатие — переключить mute
                gFM_Mute = !gFM_Mute;
                gRequestDisplayScreen = DISPLAY_FM;
            } else if (state == BUTTON_EVENT_SHORT) {
                // Короткое нажатие — переключить auto/manual
                gFM_ManualMode = !gFM_ManualMode;
                gRequestDisplayScreen = DISPLAY_FM;
            }
            break;
        case KEY_EXIT:
            if (state == BUTTON_EVENT_SHORT)
                ACTION_FM();
            break;
        case KEY_F:
            GENERIC_Key_F(bKeyPressed, bKeyHeld);
            break;
        case KEY_PTT:
            GENERIC_Key_PTT(bKeyPressed);
            break;
        default:
            if (!bKeyHeld && bKeyPressed)
                gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            break;
    }
}

// ---- FM_TurnOff / FM_Start ----

void FM_TurnOff(void)
{
    gFmRadioMode              = false;
    gFM_ScanState             = FM_SCAN_OFF;
    gFM_RestoreCountdown_10ms = 0;
    gFM_Mute                  = false;
    gFM_ManualMode            = false;

    BK1080_Init0();
    BK4819_PickRXFilterPathBasedOnFrequency(gRxVfo->freq_config_RX.Frequency);

    AUDIO_AudioPathOn();
    gEnableSpeaker = true;

    gUpdateStatus = true;

    #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        gEeprom.CURRENT_STATE = 0;
        SETTINGS_WriteCurrentState();
    #endif
}

void FM_Start(void)
{
    gDualWatchActive          = false;
    gFmRadioMode              = true;
    gFM_ScanState             = FM_SCAN_OFF;
    gFM_RestoreCountdown_10ms = 0;
    gFM_AutoScan              = false;
    gFM_ChannelPosition       = 0;
    gFM_ManualMode            = false;
    gFM_Mute                  = false;

    gEeprom.FM_Band     = FM_BAND;
    gEeprom.FM_IsMrMode = false;

    BK1080_Init(gEeprom.FM_FrequencyPlaying, FM_BAND);
    BK4819_PickRXFilterPathBasedOnFrequency(10320000);

    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
    gUpdateStatus  = true;

    #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        gEeprom.CURRENT_STATE = 3;
        SETTINGS_WriteCurrentState();
    #endif
}

#endif
