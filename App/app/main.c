/* Copyright 2023 Dual Tachyon / Robzyl KA52
 * Licensed under the Apache License, Version 2.0
 */

#include <string.h>
#include "nav_invert.h"

#include "app/action.h"
#include "app/app.h"
#include "app/chFrScanner.h"
#include "app/common.h"
#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif
#include "app/generic.h"
#include "app/main.h"
#include "app/scanner.h"

#ifdef ENABLE_SPECTRUM
#include "app/spectrum.h"
#endif

#include "audio.h"
#include "board.h"
#include "driver/bk4819.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "ui/helper.h"

// ── Save current VFO frequency to first free memory channel ──────────────────
static void SaveFreqToFreeChannel(void)
{
    if (!IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
        return;

    uint32_t f = gTxVfo->pRX->Frequency;
    if (f < 1000000)
        return;

    // Проверяем — нет ли уже такой частоты
    for (uint16_t i = MR_CHANNEL_FIRST; i <= MR_CHANNEL_LAST; i++) {
        uint32_t chf = SETTINGS_FetchChannelFrequency(i);
        if (chf == f) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            gRequestDisplayScreen = DISPLAY_MAIN;
            return;
        }
    }

    // Ищем первый свободный канал (пустой = 0xFFFFFFFF или 0)
    int freeCh = -1;
    for (uint16_t i = MR_CHANNEL_FIRST; i <= MR_CHANNEL_LAST; i++) {
        uint32_t chf = SETTINGS_FetchChannelFrequency(i);
        if (chf == 0xFFFFFFFF || chf == 0) {
            freeCh = (int)i;
            break;
        }
    }

    // Попап: очищаем строки под текстом, пишем большим шрифтом
    if (freeCh >= 0) {
        SETTINGS_SaveChannel((uint16_t)freeCh, gEeprom.TX_VFO, gTxVfo, 2);
        MR_InvalidateChannelAttributesCache();
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        char chStr[16];
        sprintf(chStr, "SAVE CH:%d", freeCh + 1);
        // Очищаем строки 2-3 (UI_PrintString занимает 2 строки)
        memset(gFrameBuffer[2], 0, LCD_WIDTH);
        memset(gFrameBuffer[3], 0, LCD_WIDTH);
        UI_PrintString(chStr, 0, LCD_WIDTH - 1, 2, 8);
        ST7565_BlitLine(2);
        ST7565_BlitLine(3);
        SYSTEM_DelayMs(800);
    } else {
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        memset(gFrameBuffer[2], 0, LCD_WIDTH);
        memset(gFrameBuffer[3], 0, LCD_WIDTH);
        UI_PrintString("MEM FULL", 0, LCD_WIDTH - 1, 2, 8);
        ST7565_BlitLine(2);
        ST7565_BlitLine(3);
        SYSTEM_DelayMs(600);
    }
    gRequestDisplayScreen = DISPLAY_MAIN;
    gUpdateDisplay = true;
}
#include "ui/inputbox.h"
#include "ui/ui.h"
#include <stdlib.h>

// ── Scanlist toggle ──────────────────────────────────────────────────────────

static void toggle_chan_scanlist(void)
{
    if (SCANNER_IsScanning())
        return;

    if (!IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
#ifdef ENABLE_SCAN_RANGES
        CHFRSCANNER_ScanRange();
#endif
        return;
    }

    ChannelAttributes_t *att = MR_GetChannelAttributes(gTxVfo->CHANNEL_SAVE);

    if (att->exclude == true) {
        att->exclude = false;
        MR_SaveChannelAttributesToFlash(gTxVfo->CHANNEL_SAVE, att);
    } else {
        uint8_t scanlist = gTxVfo->SCANLIST_PARTICIPATION;
        scanlist++;
        if (scanlist > MR_CHANNELS_LIST + 1)
            scanlist = 0;
        gTxVfo->SCANLIST_PARTICIPATION = scanlist;
        SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true, true, true);
    }

    gVfoConfigureMode = VFO_CONFIGURE;
    gFlagResetVfos    = true;
}

// ── F-key functions ──────────────────────────────────────────────────────────
// KA50 назначения (beep=true = F+кнопка, beep=false = долгое нажатие):
// KEY_0        : FM радио
// KEY_1 !beep  : смена диапазона   / beep: 1Call
// KEY_2 !beep  : VFO A/B           / beep: —
// KEY_3 !beep  : VFO/MR            / beep: —
// KEY_4        : —
// KEY_5 !beep  : toggle scanlist   / beep: —
// KEY_6        : Power (и долгое и F+)
// KEY_7        : VOX (F+7)  — долгое 7 перехватывается в DIGITS
// KEY_8        : — (не используется)
// KEY_9        : Spectrum (F+9)  — долгое 9 перехватывается в DIGITS
// KEY_UP/DOWN  : squelch (F+)
// KEY_SIDE1/2  : step (F+)

static void processFKeyFunction(const KEY_Code_t Key, const bool beep)
{
    if (gScreenToDisplay == DISPLAY_MENU) {
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

    switch (Key) {
        case KEY_0:
#ifdef ENABLE_FMRADIO
            ACTION_FM();
#endif
            break;

        case KEY_1:
            if (!beep) {
                // долгое 1: смена диапазона
                const uint8_t Vfo1 = gEeprom.TX_VFO;
#ifdef ENABLE_WIDE_RX
                if (gTxVfo->Band == BAND7_470MHz && gTxVfo->pRX->Frequency < _1GHz_in_KHz) {
                    gTxVfo->pRX->Frequency = _1GHz_in_KHz;
                    break;
                }
#endif
                gTxVfo->Band += 1;
#ifdef ENABLE_350EN
                if (gTxVfo->Band == BAND5_350MHz && !gSetting_350EN)
                    gTxVfo->Band += 1;
                else
#endif
                if (gTxVfo->Band >= BAND_N_ELEM)
                    gTxVfo->Band = BAND1_50MHz;

                gEeprom.ScreenChannel[Vfo1] = FREQ_CHANNEL_FIRST + gTxVfo->Band;
                gEeprom.FreqChannel[Vfo1]   = FREQ_CHANNEL_FIRST + gTxVfo->Band;
                gRequestSaveVFO             = true;
                gVfoConfigureMode           = VFO_CONFIGURE_RELOAD;
                gRequestDisplayScreen       = DISPLAY_MAIN;
            } else {
                ACTION_1Call();
            }
            break;

        case KEY_2:
            if (!beep) {
                // долгое 2: переключение VFO A/B
#ifdef ENABLE_FEAT_F4HWN
                gVfoConfigureMode = VFO_CONFIGURE;
#endif
                COMMON_SwitchVFOs();
            } else {
                // F+2: сохранить текущую частоту в ближайший свободный канал
                SaveFreqToFreeChannel();
            }
            break;

        case KEY_3:
            if (!beep) {
#ifdef ENABLE_FEAT_F4HWN
                gVfoConfigureMode = VFO_CONFIGURE;
#endif
                COMMON_SwitchVFOMode();
            }
            break;

        case KEY_4:
            break;

        case KEY_5:
            if (!beep)
                toggle_chan_scanlist();
            break;

        case KEY_6:
            ACTION_Power();
            break;

        case KEY_7:
#ifdef ENABLE_SPECTRUM
            APP_RunSpectrumMode(1); // F+7 = ScanList режим
            gRequestDisplayScreen = DISPLAY_MAIN;
#endif
            break;

        case KEY_8:
#ifdef ENABLE_SPECTRUM
            APP_RunSpectrumMode(3); // F+8 = Band режим
            gRequestDisplayScreen = DISPLAY_MAIN;
#endif
            break;

        case KEY_9:
#ifdef ENABLE_SPECTRUM
            APP_RunSpectrumMode(2); // F+9 = Range режим
            gRequestDisplayScreen = DISPLAY_MAIN;
#endif
            break;

        case KEY_UP:
            gEeprom.SQUELCH_LEVEL = (gEeprom.SQUELCH_LEVEL < 9) ? gEeprom.SQUELCH_LEVEL + 1 : 9;
            gVfoConfigureMode     = VFO_CONFIGURE;
            gWasFKeyPressed       = false;
            break;

        case KEY_DOWN:
            gEeprom.SQUELCH_LEVEL = (gEeprom.SQUELCH_LEVEL > 0) ? gEeprom.SQUELCH_LEVEL - 1 : 0;
            gVfoConfigureMode     = VFO_CONFIGURE;
            gWasFKeyPressed       = false;
            break;

        case KEY_SIDE1: {
            uint8_t a = FREQUENCY_GetSortedIdxFromStepIdx(gTxVfo->STEP_SETTING);
            if (a < STEP_N_ELEM - 1)
                gTxVfo->STEP_SETTING = FREQUENCY_GetStepIdxFromSortedIdx(a + 1);
            if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
                gRequestSaveChannel = 1;
            gVfoConfigureMode = VFO_CONFIGURE;
            gWasFKeyPressed   = false;
            break;
        }

        case KEY_SIDE2: {
            uint8_t b = FREQUENCY_GetSortedIdxFromStepIdx(gTxVfo->STEP_SETTING);
            if (b > 0)
                gTxVfo->STEP_SETTING = FREQUENCY_GetStepIdxFromSortedIdx(b - 1);
            if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
                gRequestSaveChannel = 1;
            gVfoConfigureMode = VFO_CONFIGURE;
            gWasFKeyPressed   = false;
            break;
        }

        default:
            gUpdateStatus   = true;
            gWasFKeyPressed = false;
            if (beep)
                gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
            break;
    }
}

// ── Channel move ─────────────────────────────────────────────────────────────

void channelMove(uint16_t Channel)
{
    const uint8_t Vfo = gEeprom.TX_VFO;

    if (!RADIO_CheckValidChannel(Channel, false, 0)) {
        if (gKeyInputCountdown <= 1)
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    gBeepToPlay = BEEP_NONE;

    gEeprom.MrChannel[Vfo]     = (uint16_t)Channel;
    gEeprom.ScreenChannel[Vfo] = (uint16_t)Channel;
    gVfoConfigureMode           = VFO_CONFIGURE_RELOAD;

    RADIO_ConfigureChannel(gEeprom.TX_VFO, gVfoConfigureMode);
    SETTINGS_SaveVfoIndices();
}

void channelMoveSwitch(void)
{
    if (!IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE))
        return;

    uint16_t Channel = 0;
    for (uint8_t i = 0; i < gInputBoxIndex; i++)
        Channel = (Channel * 10) + gInputBox[i];

    if ((Channel == 0) && (gInputBoxIndex != 4))
        return;

    if (gInputBoxIndex == 4) {
        gInputBoxIndex     = 0;
        gKeyInputCountdown = 1;
    }

    channelMove(Channel - 1);
    SETTINGS_SaveVfoIndices();
}

// ── Digit keys ───────────────────────────────────────────────────────────────

static void MAIN_Key_DIGITS(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if (bKeyHeld) {
        if (bKeyPressed) {
            if (gScreenToDisplay == DISPLAY_MAIN) {
                if (gInputBoxIndex > 0) {
                    gInputBoxIndex        = 0;
                    gRequestDisplayScreen = DISPLAY_MAIN;
                }

                HideFKeyIcon();

                // Долгое 7: фонарик при RX
                if (Key == KEY_7) {
                    gEeprom.FlashlightOnRX = !gEeprom.FlashlightOnRX;
                    gRequestSaveSettings   = true;
                    gUpdateStatus          = true;
                    gRequestDisplayScreen  = DISPLAY_MAIN;
                    return;
                }
                // Долгое 9: подсветка
                if (Key == KEY_9) {
                    if (gBackLight)
                        ACTION_BackLight();
                    else
                        ACTION_BackLightOnDemand();
                    return;
                }
                // Долгое 0: модуляция
                if (Key == KEY_0) {
                    ACTION_SwitchDemodul();
                    gRequestDisplayScreen = DISPLAY_MAIN;
                    return;
                }
                // Долгое 1: смена диапазона
                if (Key == KEY_1) {
                    const uint8_t Vfo1 = gEeprom.TX_VFO;
#ifdef ENABLE_WIDE_RX
                    if (gTxVfo->Band == BAND7_470MHz && gTxVfo->pRX->Frequency < _1GHz_in_KHz) {
                        gTxVfo->pRX->Frequency = _1GHz_in_KHz;
                    } else {
#endif
                    gTxVfo->Band += 1;
#ifdef ENABLE_350EN
                    if (gTxVfo->Band == BAND5_350MHz && !gSetting_350EN)
                        gTxVfo->Band += 1;
                    else
#endif
                    if (gTxVfo->Band >= BAND_N_ELEM)
                        gTxVfo->Band = BAND1_50MHz;
#ifdef ENABLE_WIDE_RX
                    }
#endif
                    gEeprom.ScreenChannel[Vfo1] = FREQ_CHANNEL_FIRST + gTxVfo->Band;
                    gEeprom.FreqChannel[Vfo1]   = FREQ_CHANNEL_FIRST + gTxVfo->Band;
                    gRequestSaveVFO             = true;
                    gVfoConfigureMode           = VFO_CONFIGURE_RELOAD;
                    gRequestDisplayScreen       = DISPLAY_MAIN;
                    return;
                }
                // Долгое 2: переключение VFO A/B
                if (Key == KEY_2) {
#ifdef ENABLE_FEAT_F4HWN
                    gVfoConfigureMode = VFO_CONFIGURE;
#endif
                    COMMON_SwitchVFOs();
                    return;
                }
                // Долгое 3: переключение VFO/MR
                if (Key == KEY_3) {
#ifdef ENABLE_FEAT_F4HWN
                    gVfoConfigureMode = VFO_CONFIGURE;
#endif
                    COMMON_SwitchVFOMode();
                    return;
                }
                // Долгое 4: W/N
                if (Key == KEY_4) {
                    ACTION_Wn();
                    gRequestDisplayScreen = DISPLAY_MAIN;
                    return;
                }
                // Долгое 5: scanlist/шаг (KA50)
                if (Key == KEY_5) {
                    if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
                        toggle_chan_scanlist();
                        gRequestDisplayScreen = DISPLAY_MAIN;
                    } else {
                        uint8_t a = FREQUENCY_GetSortedIdxFromStepIdx(gTxVfo->STEP_SETTING);
                        a = (a < STEP_N_ELEM - 1) ? (a + 1) : 0;
                        gTxVfo->STEP_SETTING  = FREQUENCY_GetStepIdxFromSortedIdx(a);
                        gTxVfo->StepFrequency = gStepFrequencyTable[gTxVfo->STEP_SETTING];
                        if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
                            gRequestSaveChannel = 1;
                        gVfoConfigureMode     = VFO_CONFIGURE;
                        gUpdateStatus         = true;
                        gUpdateDisplay        = true;
                        gRequestDisplayScreen = DISPLAY_MAIN;
                    }
                    gWasFKeyPressed = false;
                    return;
                }

                // Долгое 8: ничего (спектр бендов только по F+8)
                if (Key == KEY_8) {
                    return;
                }

                // Остальные долгие (1,2,3,6,8) → processFKeyFunction(Key, true)
                // beep=true сигнализирует что это долгое нажатие
                processFKeyFunction(Key, true);
            }
        }
        return;
    }

    if (bKeyPressed) {
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        return;
    }

    if (!gWasFKeyPressed) {
        // Во время скана: двухцифровой ввод номера списка (оригинальная логика KA52)
        if (gScanStateDir != SCAN_OFF) {
            INPUTBOX_Append(Key);
            if (gInputBoxIndex < 2)
                return;

            gInputBoxIndex = 0;
            uint8_t value  = (uint8_t)((gInputBox[0] * 10) + gInputBox[1]);

            if (value == 0) {
                gEeprom.SCAN_LIST_DEFAULT = MR_CHANNELS_LIST + 1;
                gScanListChanged = true;
            } else if (value <= MR_CHANNELS_LIST) {
                gEeprom.SCAN_LIST_DEFAULT = value;
                gScanListChanged = true;
                if (!RADIO_CheckValidList(value)) {
                    gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                    RADIO_NextValidList(1);
                }
            }
#ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
            SETTINGS_WriteCurrentState();
#endif
            return;
        }

        const uint8_t Vfo = gEeprom.TX_VFO;
        INPUTBOX_Append(Key);
        gKeyInputCountdown = key_input_timeout_500ms;

        channelMoveSwitch();
        gRequestDisplayScreen = DISPLAY_MAIN;

        if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
            gKeyInputCountdown = key_input_timeout_500ms / 4;
            return;
        }

        if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
            uint8_t totalDigits = 6;
            if (gTxVfo->pRX->Frequency >= _1GHz_in_KHz)
                totalDigits = 7;

            if (gInputBoxIndex == 0)
                return;

            gKeyInputCountdown = (gInputBoxIndex >= totalDigits)
                ? (key_input_timeout_500ms / 16)
                : (key_input_timeout_500ms / 3);

            if (gInputBoxIndex > totalDigits) {
                gInputBoxIndex = totalDigits;
                return;
            }

            const char *inputStr  = INPUTBOX_GetAscii();
            uint8_t     inputLen  = gInputBoxIndex;
            uint32_t    inputFreq = StrToUL(inputStr);
            for (uint8_t i = 0; i < (totalDigits - inputLen); i++)
                inputFreq *= 10;

            uint32_t Frequency = inputFreq * 100;

            if (Frequency < frequencyBandTable[0].lower)
                Frequency = frequencyBandTable[0].lower;
            else if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower) {
                const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
                Frequency = (Frequency < center) ? BX4819_band1.upper : BX4819_band2.lower;
            } else if (Frequency > frequencyBandTable[BAND_N_ELEM - 1].upper)
                Frequency = frequencyBandTable[BAND_N_ELEM - 1].upper;

            const FREQUENCY_Band_t band = FREQUENCY_GetBand(Frequency);
            if (gTxVfo->Band != band) {
                gTxVfo->Band               = band;
                gEeprom.ScreenChannel[Vfo] = band + FREQ_CHANNEL_FIRST;
                gEeprom.FreqChannel[Vfo]   = band + FREQ_CHANNEL_FIRST;
                SETTINGS_SaveVfoIndices();
                RADIO_ConfigureChannel(Vfo, VFO_CONFIGURE_RELOAD);
            }

            Frequency = FREQUENCY_RoundToStep(Frequency, gTxVfo->StepFrequency);
            if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower) {
                const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
                Frequency = (Frequency < center)
                    ? BX4819_band1.upper - gTxVfo->StepFrequency
                    : BX4819_band2.lower;
            }
            gTxVfo->freq_config_RX.Frequency = Frequency;
            gRequestSaveChannel = 1;
            return;
        }

        gRequestDisplayScreen = DISPLAY_MAIN;
        gBeepToPlay           = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    // F + цифра
    HideFKeyIcon();
    processFKeyFunction(Key, true);  // beep=true = F-key press (not long press)
}

// ── EXIT key ─────────────────────────────────────────────────────────────────

static void MAIN_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
    if (!bKeyHeld && bKeyPressed) {
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        return;
    }

    if (bKeyHeld) {
        if (bKeyPressed) {
            gInputBoxIndex        = 0;
            gRequestDisplayScreen = DISPLAY_MAIN;
        }
        return;
    }

    // released
#ifdef ENABLE_FMRADIO
    if (!gFmRadioMode)
#endif
    {
        if (gScanStateDir == SCAN_OFF) {
            if (gInputBoxIndex == 0)
                return;
            gInputBox[--gInputBoxIndex] = 10;
            gKeyInputCountdown = key_input_timeout_500ms;
#ifdef ENABLE_VOICE
            if (gInputBoxIndex == 0)
                gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
        } else {
            gScanKeepResult = false;
            gInputBoxIndex  = 0;
            CHFRSCANNER_Stop();
#ifdef ENABLE_VOICE
            gAnotherVoiceID = VOICE_ID_SCANNING_STOP;
#endif
        }
        gRequestDisplayScreen = DISPLAY_MAIN;
        return;
    }

#ifdef ENABLE_FMRADIO
    ACTION_FM();
#endif
}

// ── MENU key ─────────────────────────────────────────────────────────────────

static void MAIN_Key_MENU(bool bKeyPressed, bool bKeyHeld)
{
    if (bKeyPressed && !bKeyHeld)
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

    if (bKeyHeld) {
        if (bKeyPressed) {
#ifdef ENABLE_FEAT_F4HWN
            if (gScanStateDir != SCAN_OFF) {
                if (FUNCTION_IsRx() || gScanPauseDelayIn_10ms > 9) {
                    ChannelAttributes_t *att = MR_GetChannelAttributes(lastFoundFrqOrChan);
                    att->exclude = true;
                    MR_SaveChannelAttributesToFlash(lastFoundFrqOrChan, att);
                    gVfoConfigureMode  = VFO_CONFIGURE;
                    gFlagResetVfos     = true;
                    lastFoundFrqOrChan = lastFoundFrqOrChanOld;
                    CHFRSCANNER_ContinueScanning();
                }
                return;
            }
#endif
            gWasFKeyPressed = false;
            if (gScreenToDisplay == DISPLAY_MAIN) {
                if (gInputBoxIndex > 0) {
                    gInputBoxIndex        = 0;
                    gRequestDisplayScreen = DISPLAY_MAIN;
                }
                gUpdateStatus = true;
                ACTION_Handle(KEY_MENU, bKeyPressed, bKeyHeld);
            }
        }
        return;
    }

    if (!bKeyPressed) {
        gKeyInputCountdown = 1;
        channelMoveSwitch();
        const bool bFlag = !gInputBoxIndex;
        gInputBoxIndex   = 0;

        if (bFlag) {
            if (gScanStateDir != SCAN_OFF) {
                CHFRSCANNER_Stop();
                return;
            }
            gFlagRefreshSetting   = true;
            gRequestDisplayScreen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
            gAnotherVoiceID = VOICE_ID_MENU;
#endif
        } else {
            gRequestDisplayScreen = DISPLAY_MAIN;
        }
    }
}

// ── STAR key ─────────────────────────────────────────────────────────────────
// КА50: короткое * = ACTION_Scan (сканирование по спискам)
//        долгое * = SCANNER_Start(false) (сканер частот/тонов)
//        F+*      = SCANNER_Start(true)  (сканер CTCSS/DCS)

static void MAIN_Key_STAR(bool bKeyPressed, bool bKeyHeld)
{
    if (gCurrentFunction == FUNCTION_TRANSMIT)
        return;

    if (gInputBoxIndex) {
        if (!bKeyHeld && bKeyPressed)
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    if (!bKeyHeld && bKeyPressed) {
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        return;
    }

    if (bKeyHeld && !gWasFKeyPressed) {
        if (!bKeyPressed)
            return;
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        gBackup_CROSS_BAND_RX_TX  = gEeprom.CROSS_BAND_RX_TX;
        gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
        gWasFKeyPressed = false;
        gUpdateStatus   = true;
        SCANNER_Start(false);
        gRequestDisplayScreen = DISPLAY_SCANNER;
        return;
    }

    if (!gWasFKeyPressed) {
        // Короткое * = сканирование по спискам
        ACTION_Scan(false);
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    } else {
        // F+* = сканер тонов
        gWasFKeyPressed = false;
#ifdef ENABLE_NOAA
        if (IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }
#endif
        gBackup_CROSS_BAND_RX_TX  = gEeprom.CROSS_BAND_RX_TX;
        gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
        SCANNER_Start(true);
        gRequestDisplayScreen = DISPLAY_SCANNER;
    }

    gUpdateStatus = true;
}

// ── UP/DOWN keys ─────────────────────────────────────────────────────────────

static void MAIN_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
    if (!gEeprom.SET_NAV)
        Direction = -Direction;

#ifdef ENABLE_FEAT_F4HWN
    if (gWasFKeyPressed) {
        processFKeyFunction(Direction == 1 ? KEY_UP : KEY_DOWN, true);
        return;
    }
#endif

    uint16_t Channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];

    if (bKeyHeld || !bKeyPressed) {
        if (gInputBoxIndex > 0)
            return;
        if (!bKeyPressed) {
            if (!bKeyHeld || IS_FREQ_CHANNEL(Channel))
                return;
#ifdef ENABLE_VOICE
            AUDIO_SetDigitVoice(0, gTxVfo->CHANNEL_SAVE + 1);
            gAnotherVoiceID = (VOICE_ID_t)0xFE;
#endif
            return;
        }
    } else {
        if (gInputBoxIndex > 0) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    }

    if (gScanStateDir == SCAN_OFF) {
#ifdef ENABLE_NOAA
        if (!IS_NOAA_CHANNEL(Channel))
#endif
        {
            if (IS_FREQ_CHANNEL(Channel)) {
                const uint32_t frequency = APP_SetFrequencyByStep(gTxVfo, Direction);
                if (RX_freq_check(frequency) < 0) {
                    gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                    return;
                }
                gTxVfo->freq_config_RX.Frequency = frequency;
                BK4819_SetFrequency(frequency);
                BK4819_RX_TurnOn();
                gRequestSaveChannel = 1;
                return;
            }

            uint16_t Next = RADIO_FindNextChannel(Channel + Direction, Direction, false, 0);
            if (Next == 0xFFFF)
                return;
            if (Channel == Next)
                return;
            gEeprom.MrChannel[gEeprom.TX_VFO]    = Next;
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = Next;
            if (!bKeyHeld) {
#ifdef ENABLE_VOICE
                AUDIO_SetDigitVoice(0, Next + 1);
                gAnotherVoiceID = (VOICE_ID_t)0xFE;
#endif
            }
        }
#ifdef ENABLE_NOAA
        else {
            Channel = NOAA_CHANNEL_FIRST + NUMBER_AddWithWraparound(
                gEeprom.ScreenChannel[gEeprom.TX_VFO] - NOAA_CHANNEL_FIRST, Direction, 0, 9);
            gEeprom.NoaaChannel[gEeprom.TX_VFO]   = Channel;
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = Channel;
        }
#endif
        gRequestSaveVFO   = true;
        gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
        return;
    }

    CHFRSCANNER_Start(false, Direction);
    gScanPauseDelayIn_10ms = 1;
    gScheduleScanListen    = false;
    gPttWasReleased        = true;
}

// ── Main dispatcher ──────────────────────────────────────────────────────────

void MAIN_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
#ifdef ENABLE_FMRADIO
    if (gFmRadioMode && Key != KEY_PTT && Key != KEY_EXIT) {
        if (!bKeyHeld && bKeyPressed)
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }
#endif

    switch (Key) {
#ifdef ENABLE_FEAT_F4HWN
        case KEY_SIDE1:
        case KEY_SIDE2:
#endif
        case KEY_0 ... KEY_9:
            MAIN_Key_DIGITS(Key, bKeyPressed, bKeyHeld);
            break;
        case KEY_MENU:
            MAIN_Key_MENU(bKeyPressed, bKeyHeld);
            break;
        case KEY_UP:
        case KEY_DOWN:
            MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, NAV_DIR(Key == KEY_UP ? 1 : -1));
            break;
        case KEY_EXIT:
            MAIN_Key_EXIT(bKeyPressed, bKeyHeld);
            break;
        case KEY_STAR:
            MAIN_Key_STAR(bKeyPressed, bKeyHeld);
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
