/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/chFrScanner.h"
#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif
#include "app/scanner.h"
#include "bitmaps.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"

#ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER
#ifndef ENABLE_FEAT_F4HWN_DEBUG
static void convertTime(uint8_t *line, uint8_t type) 
{
    uint16_t t = (type == 0) ? (gTxTimerCountdown_500ms / 2) : (3600 - gRxTimerCountdown_500ms / 2);
    uint8_t m = t / 60;
    uint8_t s = t - (m * 60);

    char str[6];
    sprintf(str, "%02u:%02u", m, s);
    UI_PrintStringSmallBufferBold(str, line);

    gUpdateStatus = true;
}
#endif
#endif

void UI_DisplayStatus()
{
    char str[12] = "";
    gUpdateStatus = false;
    memset(gStatusLine, 0, sizeof(gStatusLine));

    // ТВОИ КООРДИНАТЫ (X)
     uint8_t POS_MOD  = 1;   // DW, XB, MO (глифы)
    const uint8_t POS_LOCK = 83;   // Замок (глиф)
    const uint8_t POS_F    = 83;   // Буква F (глиф)

   

    // 2. РЕЖИМЫ (DW, DWR, HL, MO)
    if (!SCANNER_IsScanning()) {
        uint8_t dw = (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) + (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) * 2;
        if (dw == 1 || dw == 3) {
            if (gDualWatchActive) {
                if (dw == 1) { // DW
                    gStatusLine[POS_MOD++] = 0x7F;
                    gStatusLine[POS_MOD++] = 0x6B;
                    gStatusLine[POS_MOD++] = 0x49;
                    gStatusLine[POS_MOD++] = 0x08;
                    gStatusLine[POS_MOD++] = 0x49;
                    gStatusLine[POS_MOD++] = 0x6B;
                    gStatusLine[POS_MOD++] = 0x7F;
                } else { // DWR
                    gStatusLine[POS_MOD++] = 0x7F;
                    gStatusLine[POS_MOD++] = 0x6B;
                    gStatusLine[POS_MOD++] = 0x4D;
                    gStatusLine[POS_MOD++] = 0x0E;
                    gStatusLine[POS_MOD++] = 0x4D;
                    gStatusLine[POS_MOD++] = 0x6B;
                    gStatusLine[POS_MOD++] = 0x7F;
                }
            } else { // HL (HOLD)
                gStatusLine[POS_MOD++] = 0x7F;
                gStatusLine[POS_MOD++] = 0x41;
                gStatusLine[POS_MOD++] = 0x77;
                gStatusLine[POS_MOD++] = 0x77;
                gStatusLine[POS_MOD++] = 0x77;
                gStatusLine[POS_MOD++] = 0x41;
                gStatusLine[POS_MOD++] = 0x7F;
            }
        } else { // MO
            gStatusLine[POS_MOD++] = 0x7F;
            gStatusLine[POS_MOD++] = 0x51;
            gStatusLine[POS_MOD++] = 0x71;
            gStatusLine[POS_MOD++] = 0x51;
            gStatusLine[POS_MOD++] = 0x71;
            gStatusLine[POS_MOD++] = 0x51;
            gStatusLine[POS_MOD++] = 0x7F;
        }
    }


         // 1. СКАНЕР И СПИСКИ
    if (gScanStateDir != SCAN_OFF || SCANNER_IsScanning()) {
        POS_MOD += 3;
        if (IS_MR_CHANNEL(gNextMrChannel) && !SCANNER_IsScanning()) {
            {
                char sl_str[4] = {0};
                uint8_t sl = gEeprom.SCAN_LIST_DEFAULT;
                if (sl == 0 || sl > MR_CHANNELS_LIST + 1) {
                    // неверное значение — показываем 0
                    sl_str[0] = '0'; sl_str[1] = 0;
                    GUI_DisplaySmallestDark(sl_str, POS_MOD, 1, true, false); POS_MOD += 5;
                } else if (sl == MR_CHANNELS_LIST + 1) {
                    // ALL — показываем A
                    sl_str[0] = 'A'; sl_str[1] = 0;
                    GUI_DisplaySmallestDark(sl_str, POS_MOD, 1, true, false); POS_MOD += 5;
                } else if (sl >= 10) {
                    sl_str[0] = '0' + sl/10; sl_str[1] = '0' + sl%10; sl_str[2] = 0;
                    GUI_DisplaySmallestDark(sl_str, POS_MOD, 1, true, false); POS_MOD += 9;
                } else {
                    sl_str[0] = '0' + sl; sl_str[1] = 0;
                    GUI_DisplaySmallestDark(sl_str, POS_MOD, 1, true, false); POS_MOD += 5;
                }
            }
        } else {
            //memcpy(gStatusLine + POS_MOD, gFontS, sizeof(gFontS));
                gStatusLine[POS_MOD ++] |= 0x41;
                gStatusLine[POS_MOD ++] |= 0x63;
                gStatusLine[POS_MOD ++] |= 0x36;
                gStatusLine[POS_MOD ++] |= 0x1C;
                gStatusLine[POS_MOD ++] |= 0x08;
                gStatusLine[POS_MOD ++] |= 0x41;
                gStatusLine[POS_MOD ++] |= 0x63;
                gStatusLine[POS_MOD ++] |= 0x36;
                gStatusLine[POS_MOD ++] |= 0x1C;
                gStatusLine[POS_MOD ++] |= 0x08;
        }
    }


    // 3. ТАЙМЕР
#ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER
    if (gSetting_set_tmr) {      
        if (gCurrentFunction == FUNCTION_TRANSMIT){
            POS_MOD += 2;
            convertTime(gStatusLine + POS_MOD, 0);
            POS_MOD += 35;
        }
        else if (FUNCTION_IsRx()) {
            POS_MOD += 2;
            convertTime(gStatusLine + POS_MOD, 1);
            POS_MOD += 35;
        }
    }
#endif

    // 4. VOX - В СТОЛБИК
#ifdef ENABLE_VOX
    if (gEeprom.VOX_SWITCH) {
        POS_MOD += 3;
        gStatusLine[POS_MOD++] = 0x7F;
        gStatusLine[POS_MOD++] = 0x41;
        gStatusLine[POS_MOD++] = 0x1C;
        gStatusLine[POS_MOD++] = 0x7C;
        gStatusLine[POS_MOD++] = 0x1C;
        gStatusLine[POS_MOD++] = 0x41;
        gStatusLine[POS_MOD++] = 0x7F;
    }
#endif

    // 6. ФОНАРИК (FlashlightOnRX) - мигание при RX включено
    if (gEeprom.FlashlightOnRX) {
        POS_MOD += 3;
        gStatusLine[POS_MOD++] |= 0x70;
        gStatusLine[POS_MOD++] |= 0x7E;
        gStatusLine[POS_MOD++] |= 0x61;
        gStatusLine[POS_MOD++] |= 0x61;
        gStatusLine[POS_MOD++] |= 0x61;
        gStatusLine[POS_MOD++] |= 0x7E;
        gStatusLine[POS_MOD++] |= 0x70;
    }

    // 7. ПОДСВЕТКА (B) - В СТОЛБИК, сдвигается правее если фонарик тоже включён
    if (gBackLight) {
        POS_MOD += 3;
        gStatusLine[POS_MOD++] |= 0x0C;
        gStatusLine[POS_MOD++] |= 0x12;
        gStatusLine[POS_MOD++] |= 0x65;
        gStatusLine[POS_MOD++] |= 0x79;
        gStatusLine[POS_MOD++] |= 0x65;
        gStatusLine[POS_MOD++] |= 0x12;
        gStatusLine[POS_MOD++] |= 0x0C;
    }


    // 7. F-KEY И ЗАМОК - РАЗДЕЛЬНО
    if (gWasFKeyPressed) {
        gStatusLine[POS_F + 0] = 0x7F;
        gStatusLine[POS_F + 1] = 0x41;
        gStatusLine[POS_F + 2] = 0x75;
        gStatusLine[POS_F + 3] = 0x75;
        gStatusLine[POS_F + 4] = 0x75;
        gStatusLine[POS_F + 5] = 0x7D;
        gStatusLine[POS_F + 6] = 0x7F;
    }
    if (gEeprom.KEY_LOCK) {
        gStatusLine[POS_LOCK + 0] = 0x7C;
        gStatusLine[POS_LOCK + 1] = 0x7A;
        gStatusLine[POS_LOCK + 2] = 0x79;
        gStatusLine[POS_LOCK + 3] = 0x49;
        gStatusLine[POS_LOCK + 4] = 0x79;
        gStatusLine[POS_LOCK + 5] = 0x7A;
        gStatusLine[POS_LOCK + 6] = 0x7C;
    }




    #ifdef ENABLE_FEAT_F4HWN
    if (gMute) {
        // Вывод Mute (например на позиции 100, чтоб не мешал)
        memcpy(gStatusLine + 100, gFontMute, sizeof(gFontMute));
    }
    #endif

    // 8. БАТАРЕЯ (Твой оригинал)
    if (gSetting_battery_text == 0);
    else if (gSetting_battery_text == 1) {
        sprintf(str, "%u.%02uV", gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100);
    } else if (gSetting_battery_text == 2) {
        sprintf(str, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
    }
    
    uint8_t battPos = 127 - (strlen(str) * 7);
    UI_PrintStringSmallBufferBold(str, gStatusLine + battPos);

    ST7565_BlitStatusLine();
}