/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef FREQUENCIES_H
#define FREQUENCIES_H

#include <stdint.h>
#include <frequencies.h>

#define _1GHz_in_KHz 100000000
#define DEFAULT_FREQ 43450000 // Use for Reset and Aircopy

typedef struct {
    const uint32_t lower;
    const uint32_t upper;
} freq_band_table_t;

extern const freq_band_table_t BX4819_band1;
extern const freq_band_table_t BX4819_band2;

typedef enum  {
    BAND_NONE = -1,
    BAND1_50MHz = 0,
    BAND2_108MHz,
    BAND3_137MHz,
    BAND4_174MHz,
    BAND5_350MHz,
    BAND6_400MHz,
    BAND7_470MHz,
    BAND_N_ELEM
} FREQUENCY_Band_t;

extern const freq_band_table_t frequencyBandTable[7];

typedef enum {
    // Ordre croissant de fréquence
    STEP_0_01kHz,   // 1
    STEP_0_05kHz,   // 5
    STEP_0_1kHz,    // 10
    STEP_0_25kHz,   // 25
    STEP_0_5kHz,    // 50
    STEP_1kHz,      // 100
    STEP_1_25kHz,   // 125
    STEP_2_5kHz,    // 250
    STEP_5kHz,      // 500
    STEP_6_25kHz,   // 625
    STEP_8_33kHz,   // 833
    STEP_9kHz,      // 900
    STEP_10kHz,     // 1000
    STEP_12_5kHz,   // 1250
    STEP_15kHz,     // 1500
    STEP_20kHz,     // 2000
    STEP_25kHz,     // 2500
    STEP_30kHz,     // 3000
    STEP_50kHz,     // 5000
    STEP_100kHz,    // 10000
    STEP_125kHz,    // 12500
    STEP_200kHz,    // 20000
    STEP_250kHz,    // 25000
    STEP_500kHz,    // 50000
    STEP_N_ELEM     // Nombre d'éléments (24)
} STEP_Setting_t;


extern const uint16_t gStepFrequencyTable[];

#ifdef ENABLE_NOAA
    extern const uint32_t NoaaFrequencyTable[10];
#endif

FREQUENCY_Band_t FREQUENCY_GetBand(uint32_t Frequency);
uint8_t          FREQUENCY_CalculateOutputPower(uint8_t TxpLow, uint8_t TxpMid, uint8_t TxpHigh, int32_t LowerLimit, int32_t Middle, int32_t UpperLimit, int32_t Frequency);
uint32_t         FREQUENCY_RoundToStep(uint32_t freq, uint16_t step);

STEP_Setting_t   FREQUENCY_GetStepIdxFromSortedIdx(uint8_t sortedIdx);
uint32_t         FREQUENCY_GetSortedIdxFromStepIdx(uint8_t step);

int32_t          TX_freq_check(uint32_t Frequency);
int32_t          RX_freq_check(uint32_t Frequency);

#endif
