/* Copyright 2023 fagci
 * https://github.com/fagci
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

#ifndef SPECTRUM_H
#define SPECTRUM_H

#include "../bitmaps.h"
#include "../board.h"
#include "../driver/bk4819-regs.h"
#include "../driver/bk4819.h"
#include "../driver/gpio.h"
#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../external/printf/printf.h"
#include "../font.h"
#include "../frequencies.h"
#include "../helper/battery.h"
#include "../misc.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/helper.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define ADRESS_STATE   0xC000
#define ADRESS_VERSION 0xC010
#define ADRESS_PARAMS  0xC020
#define ADRESS_HISTORY 0xC200


typedef enum {
    AUTOLOCK_OFF,
    AUTOLOCK_10S,
    AUTOLOCK_20S,
    AUTOLOCK_30S
} AUTOLOCK_t;

static const uint8_t DrawingEndY = 50;
static const uint8_t U8RssiMap[] = {
    121, 115, 109, 103, 97, 91, 85, 79, 73, 63,
};
static const char* const scanStepNames[] = {
  "10Hz","50Hz","100Hz","250Hz","500Hz",
  "1kHz","1k25Hz","2k5Hz","5kHz","6k25Hz",
  "8k33Hz","9kHz","10kHz","12k5Hz","15kHz",
  "20kHz","25kHz","30kHz","50kHz","100kHz",
  "125kHz","200kHz","250kHz","500kHz"};

static const uint16_t scanStepValues[] = {
    1, 5, 10, 25, 50,
    100, 125, 250, 500, 625,
    833, 900, 1000, 1250, 1500,
    2000, 2500, 3000, 5000, 10000,
    12500, 20000, 25000, 50000
};

static const uint16_t jumpSizes[] = {
    1, 5, 10, 25, 50,           
    3000, 2500, 2500, 2500, 2500,
    2500, 2700, 3000, 2500, 3000,
    4000, 2500, 3000, 5000, 10000,
    12500, 20000, 25000, 50000
};

static const uint16_t scanStepBWRegValues[] = {
    //     RX  RXw TX  BW
    // 0b0 000 000 001 01 1000
    // 1
    0b0000000001011000, // 6.25
    // 10
    0b0000000001011000, // 6.25
    // 50
    0b0000000001011000, // 6.25
    // 100
    0b0000000001011000, // 6.25
    // 250
    0b0000000001011000, // 6.25
    // 500
    0b0010010001011000, // 6.25
    // 625
    0b0100100001011000, // 6.25
    // 833
    0b0110110001001000, // 6.25
    // 1000
    0b0110110001001000, // 6.25
    // 1250
    0b0111111100001000, // 6.25
    // 2500
    0b0011011000101000, // 25
    // 10000
    0b0011011000101000, // 25
};

// static const uint16_t listenBWRegValues[] = {
//     0b0011011000101000, // 25
//     0b0111111100001000, // 12.5
//     0b0100100001011000, // 6.25
// };


typedef enum State {
  SPECTRUM,
  FREQ_INPUT,
  STILL,
  BAND_LIST_SELECT,
  SCANLIST_SELECT,
  PARAMETERS_SELECT,
#ifdef ENABLE_CPU_STATS
  RAM_VIEW,           // diagnostyczny widok pamięci RAM
  MEM_BUFFERS,        // lista głównych buforów/tablic
  MEM_VIEWER,         // podgląd hex+ascii/bin/info wybranego bufora
  CPU_VIEW,           // informacje o CPU i bieżąca temperatura
#endif
  #ifdef ENABLE_SCANLIST_SHOW_DETAIL
  SCANLIST_CHANNELS,  // NOWY STAN
  #endif
} State;


typedef enum Mode {
  FREQUENCY_MODE,
  CHANNEL_MODE,
  SCAN_RANGE_MODE,
  SCAN_BAND_MODE,
} Mode;

typedef enum StepsCount {
  STEPS_128,
  STEPS_64,
  STEPS_32,
  STEPS_16,
} StepsCount;

typedef enum ScanList {
  S_SCAN_LIST_1,
  S_SCAN_LIST_2,
  S_SCAN_LIST_3,
  S_SCAN_LIST_4,
  S_SCAN_LIST_5,
  S_SCAN_LIST_6,
  S_SCAN_LIST_7,
  S_SCAN_LIST_8,
  S_SCAN_LIST_9,
  S_SCAN_LIST_10,
  S_SCAN_LIST_11,
  S_SCAN_LIST_12,
  S_SCAN_LIST_13,
  S_SCAN_LIST_14,
  S_SCAN_LIST_15,
  S_SCAN_LIST_ALL
} ScanList;

typedef struct bandparameters { 
  char BandName[12];
  uint32_t Startfrequency; // Start frequency in MHz /100
  uint32_t Stopfrequency; // Stop frequency in MHz /100
  STEP_Setting_t scanStep;
  ModulationMode_t modulationType;
} bandparameters;

#define MAX_BANDS 24

typedef struct SpectrumSettings {
  uint32_t frequencyChangeStep;  
  StepsCount stepsCount;
  STEP_Setting_t scanStepIndex;
  uint16_t scanDelay;
  uint16_t rssiTriggerLevelUp;
  BK4819_FilterBandwidth_t bw;
  BK4819_FilterBandwidth_t listenBw;
  int16_t dbMin;
  int16_t dbMax;  
  ModulationMode_t modulationType;
  int scanList;
  bool scanListEnabled[MR_CHANNELS_LIST];
  bool bandEnabled[MAX_BANDS];
} SpectrumSettings;

typedef struct KeyboardState{
  KEY_Code_t current;
  KEY_Code_t prev;
  uint8_t counter;
  bool fKeyPressed;
} KeyboardState;

typedef struct ScanInfo {
  uint16_t rssi, rssiMin, rssiMax;
  uint32_t f;
  uint16_t scanStep,i;
} ScanInfo;

typedef struct PeakInfo {
  uint16_t t;
  uint16_t rssi;
  uint32_t f;
  uint16_t i;
} PeakInfo;

typedef struct ChannelInfo_t {
    uint32_t frequency;
    uint32_t offset;
} __attribute__((packed)) ChannelInfo_t;

extern bool gComeBack;
void APP_RunSpectrum(void);
void ClearSettings(void);
void LoadSettings();


#endif 
