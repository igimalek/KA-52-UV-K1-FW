/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * OURO_KA52 FM UI
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifdef ENABLE_FMRADIO

#include <string.h>

#include "app/fm.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/ui.h"

void UI_DisplayFM(void)
{
    char String[16];

    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    // "FM" большим шрифтом слева
    strcpy(String, "FM");
    UI_PrintString(String, 24, 10, 2, 10);

    // Частота большим диджитал шрифтом
    memset(String, 0, sizeof(String));
    sprintf(String, "%3d.%d",
            gEeprom.FM_FrequencyPlaying / 10,
            gEeprom.FM_FrequencyPlaying % 10);
    UI_DisplayFrequency(String, 56, 2, true);

    // Пунктирные линии (step=2)
    const uint8_t step = 2;

    // Горизонтальная верхняя (Y=16)
    for (uint8_t x = 3; x < 125; x += 1) {
        uint8_t y = 15;
        gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
    }

    // Горизонтальная нижняя-2 (Y=58)
    for (uint8_t x = 3; x < 126; x += 1) {
        uint8_t y = 54;
        gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
    }
    // Вертикальная левая (X=14, Y 10..52)
    for (uint8_t y = 15; y <= 53; y += 1) {
        uint8_t x = 3;
        gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
    }
    // Вертикальная левая (X=14, Y 10..52)
    for (uint8_t y = 15; y <= 53; y += 2) {
        uint8_t x = 5;
        gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
    }
    // Вертикальная правая (X=120, Y 20..50)
    for (uint8_t y = 15; y <= 53; y += 1) {
        uint8_t x = 125;
        gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
    }
    // Вертикальная правая (X=120, Y 20..50)
    for (uint8_t y = 15; y <= 53; y += 2) {
        uint8_t x = 123;
        gFrameBuffer[(y - 8) >> 3][x] |= (1u << ((y - 8) & 7));
    }

 //GUI_DisplaySmallestDark("FIND:", 1, 6, false, true);
    // Режим поиска: AUTO или MANU — сразу после BROADCAST
    if (gFM_ManualMode)
        GUI_DisplaySmallestDark("STEP", 21, 5, false, true);
    else
        GUI_DisplaySmallestDark("AUTO", 21, 5, false, true);

//MUTE / ON AIR
    if (gFM_Mute) {
        // Если звук выключен
        GUI_DisplaySmallestDark("SILENT", 69, 5, false, true);
    } else {
        // Если звук включен (активный эфир)
        GUI_DisplaySmallestDark("ON AIR", 69, 5, false, true);
    }

    GUI_DisplaySmallestDark("KA-52 STATION", 26, 44, false, true);

    ST7565_BlitFullScreen();
}

#endif
