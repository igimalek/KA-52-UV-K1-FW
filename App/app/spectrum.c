//K1 Spectrum

// ============================================================
// SECTION: Includes
// ============================================================
#include "app/spectrum.h"
#include "nav_invert.h"
#include "scanner.h"
#include "driver/backlight.h"
#include "driver/eeprom.h"
#include "ui/helper.h"
#include "common.h"
#include "action.h"
#include "ui/main.h"
#ifdef ENABLE_CPU_STATS
    #include "app/mem_stats.h"
#endif
#ifdef ENABLE_CPU_TEMP
    #include "driver/cpu_temp.h"
#endif
#include "audio.h"
#include "misc.h"
#include "driver/py25q16.h"
#include "version.h"
#ifdef ENABLE_DEV
#include "debugging.h"
#endif
#include <stdlib.h>
#ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
    #include "screenshot.h"
#endif
// ============================================================
// SECTION: Compile-time configuration
// ============================================================
#define MAX_VISIBLE_LINES 6
#define NoisLvl 45
#define NoiseHysteresis 15
          /////////////////////////DEBUG//////////////////////////
//char str[64] = "";sprintf(str, "%d\r\n", Spectrum_state );//LogUart(str);

// ============================================================
// SECTION: State variables
// ============================================================
static volatile bool gSpectrumChangeRequested = false;
static volatile uint8_t gRequestedSpectrumState = 0;

#ifdef ENABLE_USB
    #define HISTORY_SIZE 100
#else
    #define HISTORY_SIZE 200
#endif

#define MONITOR_SIZE 20
static uint8_t cachedValidScanListCount = 0;
static uint8_t cachedEnabledScanListCount = 0;
static uint16_t historyListIndex = 0;
static uint16_t indexFs = 0;
static int16_t historyScrollOffset = 0;
static uint8_t MonitorIndex = 0;

/* ---- packed bool flags (was 15 separate bytes) ---- */
static struct {
    uint16_t scanListCountsDirty    : 1;
    uint16_t gHistoryScan           : 1;
    uint16_t gHistorySortLongPressDone : 1;
    uint16_t SettingsLoaded         : 1;
    uint16_t SPECTRUM_PAUSED        : 1;
    uint16_t gIsPeak                : 1;
    uint16_t historyListActive      : 1;
    uint16_t gForceModulation       : 1;
    uint16_t classic                : 1;
    uint16_t Key_1_pressed          : 1;
    uint16_t isKnownChannel         : 1;
    uint16_t isInitialized          : 1;
    uint16_t isListening            : 1;
    uint16_t newScanStart           : 1;
    uint16_t audioState             : 1;
    uint16_t refreshScanListName    : 1;
} bf = {
    .scanListCountsDirty  = 1,
    .SettingsLoaded       = 0,
    .SPECTRUM_PAUSED      = 0,
    .gIsPeak              = 0,
    .historyListActive    = 0,
    .gForceModulation     = 0,
    .classic              = 1,
    .Key_1_pressed        = 0,
    .isKnownChannel       = 0,
    .isInitialized        = 0,
    .isListening          = 1,
    .newScanStart         = 1,
    .audioState           = 1,
    .refreshScanListName  = 1,
    .gHistoryScan         = 0,
    .gHistorySortLongPressDone = 0,
};
/* convenience macros so all existing code compiles unchanged */
#define scanListCountsDirty     bf.scanListCountsDirty
#define gHistoryScan            bf.gHistoryScan
#define gHistorySortLongPressDone bf.gHistorySortLongPressDone
#define SettingsLoaded          bf.SettingsLoaded
#define SPECTRUM_PAUSED         bf.SPECTRUM_PAUSED
#define gIsPeak                 bf.gIsPeak
#define historyListActive       bf.historyListActive
#define gForceModulation        bf.gForceModulation
#define classic                 bf.classic
#define Key_1_pressed           bf.Key_1_pressed
#define isKnownChannel          bf.isKnownChannel
#define isInitialized           bf.isInitialized
#define isListening             bf.isListening
#define newScanStart            bf.newScanStart
#define audioState              bf.audioState
#define refreshScanListName     bf.refreshScanListName

/////////////////////////////Parameters://///////////////////////////
//SEE parametersSelectedIndex
// see GetParametersText
static uint8_t DelayRssi = 3;                // case 0       
static uint16_t SpectrumDelay = 0;           // case 1      
static uint16_t MaxListenTime = 0;           // case 2
static uint32_t gScanRangeStart = 1400000;   // case 3      
static uint32_t gScanRangeStop = 13000000;   // case 4
//Step                                       // case 5      
//ListenBW                                   // case 6      
//Modulation                                 // case 7      
static bool Backlight_On_Rx = 0;             // case 8        
static uint16_t SpectrumSleepMs = 0;         // case 9
static uint8_t Noislvl_OFF = NoisLvl;        // case 10
static uint8_t Noislvl_ON = NoisLvl - NoiseHysteresis;
static uint16_t osdPopupSetting = 500;       // case 11
static uint16_t UOO_trigger = 15;            // case 12
static uint8_t AUTO_KEYLOCK = AUTOLOCK_OFF;  // case 13
static uint8_t GlitchMax = 20;               // case 14 
static bool    SoundBoost = 0;               // case 15
static uint8_t PttEmission = 0;              // case 16
static bool gMonitorScan = true;             // case 17
//ClearHistory All                           // case 18
//ClearHistory BL                            // case 19
//ClearHistory Not BL                        // case 20
//ClearSettings                              // case 21
   
#define PARAMETER_COUNT 22
////////////////////////////////////////////////////////////////////
#ifdef ENABLE_BENCH
    static uint32_t benchTickMs = 0;      
    static uint16_t benchStepsThisSec = 0;
    static uint16_t benchRatePerSec = 0;  
    static uint32_t benchLapMs = 0;       
    static uint32_t benchLastLapMs = 0;   
    static bool benchLapDone = false;
#endif
bool Cleared = 0;
uint8_t  gKeylockCountdown = 0;
bool     gIsKeylocked = false;
static uint16_t osdPopupTimer = 0;
static uint32_t Fmax = 0;
static uint32_t spectrumElapsedCount = 0;
static uint16_t SpectrumPauseCount = 0;
static uint8_t IndexMaxLT = 0;
static const char *labels[] = {"OFF","3s","6s","10s","20s", "1m", "5m", "10m", "20m", "30m"};
static const uint16_t listenSteps[] = {0, 3, 6, 10, 20, 60, 300, 600, 1200, 1800}; //in s
#define LISTEN_STEP_COUNT 9

static uint8_t IndexPS = 0;
static const char *labelsPS[] = {"OFF","200ms","500ms", "1s", "2s", "5s"};
static const uint16_t PS_Steps[] = {0, 20, 50, 100, 200, 500}; //in 10 ms
#define PS_STEP_COUNT 5


static uint32_t lastReceivingFreq = 0;
static uint8_t SpectrumMonitor = 0;
static uint8_t prevSpectrumMonitor = 0;
static uint16_t WaitSpectrum = 0; 
#define SQUELCH_OFF_DELAY 10;

static uint8_t ArrowLine = 1;

static void ToggleRX(bool on);
static void NextScanStep();
static void BuildValidScanListIndices();
static void RenderHistoryList();
static void RenderScanListSelect();
static void RenderParametersSelect();
//static bool StorePtt_Toggle_Mode = 0;
static void UpdateScan();
static uint8_t bandListSelectedIndex = 0;
static int16_t bandListScrollOffset = 0;
static void RenderBandSelect();
static void ClearHistory(uint8_t mode);
static void DrawMeter(int);
static void SortHistoryByFrequencyAscending(void); //nowe
static uint8_t scanListSelectedIndex = 0;
static uint8_t scanListNavIndex = 0;  // индекс при листании UP/DOWN
static uint8_t scanListScrollOffset = 0;
static uint8_t parametersSelectedIndex = 0;
static uint8_t parametersScrollOffset = 0;
static uint8_t validScanListCount = 0;
static KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0,0};
struct FrequencyBandInfo {
    uint32_t lower;
    uint32_t upper;
    uint32_t middle;
};
static uint32_t cdcssFreq;
static uint16_t ctcssFreq;
//static uint8_t refresh = 0; // СУБТОНО ЗАПРОС ВСЕГДА
#define F_MAX frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper
#define Bottom_print 51 //Robby69
static Mode appMode;
#define UHF_NOISE_FLOOR 5

static uint16_t scanChannelsCount;
static uint8_t monitorChannelsCount;
static void ToggleScanList();
static void SaveSettings();
static const uint16_t RSSI_MAX_VALUE = 255;
static uint16_t R30, R37, R3D, R43, R47, R48, R7E, R02, R3F, R7B, R12, R11, R14, R54, R55, R75;
static char String[48];
static uint16_t  gChannel;
static char channelName[12];
ModulationMode_t  channelModulation;
static BK4819_FilterBandwidth_t channelBandwidth;
static uint8_t bl;
static State currentState = SPECTRUM, previousState = SPECTRUM;
static uint8_t Spectrum_state = 1; 
static PeakInfo peak;
static ScanInfo scanInfo;
static bool IsBlacklisted(uint32_t f);
static void SetState(State state);

typedef struct {
    char left[17];
    char right[14];
    bool enabled;
} ListRow;

typedef void (*GetListRowFn)(uint16_t index, ListRow *row);

/***************************BIG RAM******************************************/
static uint32_t         *ScanFrequencies = NULL;
static bandparameters   *BParams = NULL;
static uint32_t         *HFreqs = NULL;        // was static array, now heap
static bool             *HBlacklisted = NULL;  // was static array, now heap
static uint32_t         MonitorFreqs[MONITOR_SIZE];
/****************************************************************************/

SpectrumSettings settings = {stepsCount: STEPS_128,
                             scanStepIndex: STEP_500kHz,
                             frequencyChangeStep: 80000,
                             rssiTriggerLevelUp: 20,
                             bw: BK4819_FILTER_BW_WIDE,
                             listenBw: BK4819_FILTER_BW_WIDE,
                             modulationType: false,
                             dbMin: -120,
                             dbMax: -60,
                             scanList: S_SCAN_LIST_ALL,
                             scanListEnabled: {0},
                             bandEnabled: {0}
                            };

static uint32_t currentFreq, tempFreq;
static uint8_t rssiHistory[128];
static uint8_t ShowLines = 1;
static uint8_t freqInputIndex = 0;
static uint8_t freqInputDotIndex = 0;
static KEY_Code_t freqInputArr[10];
char freqInputString[11];
static uint8_t nextBandToScanIndex = 0;
static void LookupChannelModulation();

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
  static uint16_t scanListChannels[MR_CHANNEL_LAST+1]; // Array to store Channel indices for selected scanlist
  static uint16_t scanListChannelsCount = 0; // Number of Channels in selected scanlist
  static uint16_t scanListChannelsSelectedIndex = 0;
  static uint16_t scanListChannelsScrollOffset = 0;
  static uint16_t selectedScanListIndex = 0; // Which scanlist we're viewing Channels for
  static void BuildScanListChannels(uint8_t scanListIndex);
  static void RenderScanListChannels();
 // static void RenderScanListChannelsDoubleLines(const char* title, uint8_t numItems, uint8_t selectedIndex, uint8_t scrollOffset);
#endif
static uint8_t validScanListIndices[MR_CHANNELS_LIST];
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//                                              K1 SPECIFIC
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
bool gComeBack = 0;
static void LoadActiveBands(void);
uint16_t BOARD_gMR_fetchChannel(const uint32_t freq);
static void LoadActiveScanFrequencies(void);
static void LoadSingleScanListFrequencies(uint8_t listIndex);  // load one list by validScanListIndices index
#ifdef ENABLE_CPU_STATS
    static void RenderRAMView();
    static void OnKeyDownRAMView(uint8_t key);
    static void RenderMemBuffers();
    static void OnKeyDownMemBuffers(uint8_t key);
    static void RenderMemViewer();
    static void OnKeyDownMemViewer(uint8_t key);
    static uint8_t memBufSelectedIndex = 0;   /* selected entry in MEM_BUFFERS */
    static uint8_t memBufScrollOffset  = 0;   /* scroll offset for MEM_BUFFERS list */
    static uint16_t memViewOffset      = 0;   /* byte offset into buffer in MEM_VIEWER */
    
    typedef enum {
        MEM_VIEW_HEX_ASCII = 0, /* wide HEX+ASCII, 6 bytes per row (default) */
        MEM_VIEW_BIN,           /* BIN detail for one byte                    */
        MEM_VIEW_INFO,          /* buffer metadata / INFO screen              */
        MEM_VIEW_MODE_COUNT
    } MemViewMode_t;
    
    static MemViewMode_t memViewMode = MEM_VIEW_HEX_ASCII;
    static void OnKeyDownCPUView(uint8_t key);
#endif
static uint16_t bandCount;
STEP_Setting_t channelStep;
int Rssi2DBm(const uint16_t rssi) {return (rssi >> 1) - 160;}

static int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

static void UpdateDBMaxAuto() { //Zoom
  static uint8_t z = 2;
  int newDbMax;
    if (scanInfo.rssiMax > 0) {
        newDbMax = clamp(Rssi2DBm(scanInfo.rssiMax), -60, 0);
        newDbMax = Rssi2DBm(scanInfo.rssiMax);

        if (newDbMax > settings.dbMax + z) {
            settings.dbMax = settings.dbMax + z;   // montée limitée
        } else if (newDbMax < settings.dbMax - z) {
            settings.dbMax = settings.dbMax - z;   // descente limitée
        } else {
            settings.dbMax = newDbMax;              // suivi normal
        }
    }

    if (scanInfo.rssiMin > 0) {
        settings.dbMin = clamp(Rssi2DBm(scanInfo.rssiMin), -160, -120);
        settings.dbMin = Rssi2DBm(scanInfo.rssiMin);
    }
}


BK4819_FilterBandwidth_t ACTION_NextBandwidth(BK4819_FilterBandwidth_t currentBandwidth, const bool dynamic, bool increase)
{
    BK4819_FilterBandwidth_t nextBandwidth =
        (increase && currentBandwidth == BK4819_FILTER_BW_NARROWER) ? BK4819_FILTER_BW_WIDE :
        (!increase && currentBandwidth == BK4819_FILTER_BW_WIDE)     ? BK4819_FILTER_BW_NARROWER :
        (increase ? currentBandwidth + 1 : currentBandwidth - 1);

    BK4819_SetFilterBandwidth(nextBandwidth, dynamic);
    gRequestSaveChannel = 1;
    return nextBandwidth;
}

const char *bwNames[5] = {"25k", "12.5k", "8.33k", "6.25k", "5k"};

int16_t BK4819_GetAFCValue() { //from Hawk5
  int16_t signedAfc = (int16_t)BK4819_ReadRegister(0x6D);
  return (signedAfc * 10) / 3;
}

#ifdef ENABLE_CPU_TEMP
int16_t temp_dc;

static void RenderCPUTemp(void) {
    char buf[32];
    /* ambient_est = cpu_temp - CPU_AMBIENT_OFFSET (default 8.0 degC) */
#define CPU_AMBIENT_OFFSET_DC  80   /* 8.0 degC in deci-Celsius */
    if (gNextTimeslice_60s) {
        gNextTimeslice_60s = 0;
        temp_dc = CpuTemp_ReadDeciCelsius();
    }
    int16_t amb_dc = temp_dc - CPU_AMBIENT_OFFSET_DC;
    int16_t a_int  = amb_dc / 10;
    int16_t a_frac = (int16_t)abs(amb_dc % 10);
    snprintf(buf, sizeof(buf), "%d.%d C", (int)a_int, (int)a_frac);
    UI_PrintStringSmallbackground(buf, 1, 1, 0, 0);
}
#endif
typedef struct
{
	uint8_t      sLevel;      // S-level value
	uint8_t      over;        // over S9 value
	int          dBmRssi;     // RSSI in dBm
	bool         overSquelch; // determines whether signal is over squelch open threshold
}  __attribute__((packed))  sLevelAttributes;

#define HF_FREQUENCY 3000000

sLevelAttributes GetSLevelAttributes(const int16_t rssi, const uint32_t frequency)
{
	sLevelAttributes att;
	// S0 .. base level
	int16_t      s0_dBm       = -130;
	// all S1 on max gain, no antenna
	const int8_t dBmCorrTable[7] = {
		-5,  // band 1
		-38, // band 2
		-37, // band 3
		-20, // band 4
		-23, // band 5
		-23, // band 6
		-16  // band 7
	};

	if(frequency > HF_FREQUENCY)
	s0_dBm-=20;
	att.dBmRssi = Rssi2DBm(rssi)+dBmCorrTable[FREQUENCY_GetBand(frequency)];
	att.sLevel  = MIN(MAX((att.dBmRssi - s0_dBm) / 6, 0), 9);
	att.over    = MIN(MAX(att.dBmRssi - (s0_dBm + 9*6), 0), 99);
	att.overSquelch = att.sLevel > 5;

	return att;
}

#ifdef ENABLE_CPU_STATS
/* ---- buffer watch registry -------------------------------------------- */

/*
 * RamWatchEntry_t — describes one monitored buffer for the memory diagnostics
 * UI (MEM_BUFFERS / MEM_VIEWER screens).
 *
 * For static arrays  : base = array start, dyn = NULL.
 * For dynamic buffers: base = NULL, dyn = &the_pointer_variable
 *   so the viewer can dereference at render time and handle NULL safely.
 *
 * elem_count and elem_size are informational (shown in INFO mode); set to 0
 * when not applicable.
 */
typedef struct {
    const char  *name;        /* display name (up to ~9 chars)            */
    const void  *base;        /* static: data ptr; dynamic: NULL          */
    const void **dyn;         /* dynamic: &ptr_var; static: NULL          */
    uint32_t     bytes;       /* total size in bytes                      */
    uint16_t     elem_count;  /* number of elements (0 = n/a)             */
    uint8_t      elem_size;   /* size of one element in bytes (0 = n/a)   */
    const char  *section;     /* ".bss", "heap", ".data" …                */
} RamWatchEntry_t;

/* Forward helpers */
static const uint8_t *RamWatch_GetPtr(uint8_t idx);
static uint32_t       RamWatch_GetSize(uint8_t idx);

/* The watched-buffer table. */
static const RamWatchEntry_t ram_watch[] = {
    /* name        base             dyn                            bytes                              count        esize  section */
    { "rssiHist",  rssiHistory,     NULL,                          sizeof(rssiHistory),               128,         1,     ".bss"  },
    { "settings",  &settings,       NULL,                          sizeof(settings),                  1,           sizeof(settings), ".bss" },
    { "frameBuf",  gFrameBuffer,    NULL,                          sizeof(gFrameBuffer),              7,           128,   ".bss"  },
    { "ScanFreq",  NULL,            (const void **)&ScanFrequencies,
                                                                    (MR_CHANNEL_LAST + 1)*sizeof(uint32_t), (MR_CHANNEL_LAST + 1), 4, "heap" },
    { "HFreqs",    NULL,            (const void **)&HFreqs,        HISTORY_SIZE*sizeof(uint32_t),     HISTORY_SIZE, 4,   "heap"  },
    { "HBlkList",  NULL,            (const void **)&HBlacklisted,  HISTORY_SIZE*sizeof(bool),         HISTORY_SIZE, 1,   "heap"  },
};
#define RAM_WATCH_COUNT ((uint8_t)(sizeof(ram_watch) / sizeof(ram_watch[0])))

static const uint8_t *RamWatch_GetPtr(uint8_t idx)
{
    if (idx >= RAM_WATCH_COUNT) return NULL;
    if (ram_watch[idx].dyn != NULL)
        return (const uint8_t *)*ram_watch[idx].dyn;
    return (const uint8_t *)ram_watch[idx].base;
}

static uint32_t RamWatch_GetSize(uint8_t idx)
{
    if (idx >= RAM_WATCH_COUNT) return 0u;
    return ram_watch[idx].bytes;
}

/* ---- MEM_BUFFERS screen ----------------------------------------------- */

#define MEM_BUF_ROWS 6   /* visible buffer entries per screen */

static void RenderMemBuffers(void)
{
    char buf[32];

    /* inverted header */
    for (uint8_t px = 0; px < 128; ++px)
        for (uint8_t py = 0; py < 7; ++py)
            PutPixel(px, py, true);
    GUI_DisplaySmallest("MEM BUFS MENU=view", 1, 1, false, false);

    /* clamp scroll */
    if (memBufSelectedIndex >= RAM_WATCH_COUNT)
        memBufSelectedIndex = RAM_WATCH_COUNT - 1u;
    if (memBufSelectedIndex < memBufScrollOffset)
        memBufScrollOffset = memBufSelectedIndex;
    if (memBufSelectedIndex >= memBufScrollOffset + MEM_BUF_ROWS)
        memBufScrollOffset = memBufSelectedIndex - MEM_BUF_ROWS + 1u;

    uint8_t y = 9;
    for (uint8_t i = 0; i < MEM_BUF_ROWS; ++i) {
        uint8_t idx = memBufScrollOffset + i;
        if (idx >= RAM_WATCH_COUNT) break;

        const uint8_t *ptr  = RamWatch_GetPtr(idx);
        uint32_t       size = RamWatch_GetSize(idx);

        if (ptr != NULL) {
            snprintf(buf, sizeof(buf), "%-9s%4lu %s",
                     ram_watch[idx].name, (unsigned long)size,
                     ram_watch[idx].section);
        } else {
            snprintf(buf, sizeof(buf), "%-9s  -- %s",
                     ram_watch[idx].name, ram_watch[idx].section);
        }

        bool selected = (idx == memBufSelectedIndex);
        if (selected) {
            for (uint8_t px = 0; px < 128; ++px)
                for (uint8_t py = y - 1; py < y + 7; ++py)
                    PutPixel(px, py, true);
            GUI_DisplaySmallest(buf, 1, y, false, false);
        } else {
            GUI_DisplaySmallest(buf, 1, y, false, true);
        }
        y += 8;
    }
}

static void OnKeyDownMemBuffers(uint8_t key)
{
    BACKLIGHT_TurnOn();
    switch (key) {
    case KEY_UP:
        if (memBufSelectedIndex > 0)
            --memBufSelectedIndex;
        else
            memBufSelectedIndex = RAM_WATCH_COUNT - 1u;
        break;
    case KEY_DOWN:
        if (memBufSelectedIndex < RAM_WATCH_COUNT - 1u)
            ++memBufSelectedIndex;
        else
            memBufSelectedIndex = 0;
        break;
    case KEY_MENU:
        /* Enter viewer for the selected buffer (only if data is available). */
        memViewOffset = 0;
        memViewMode   = MEM_VIEW_HEX_ASCII;
        SetState(MEM_VIEWER);
        break;
    case KEY_EXIT:
        SetState(RAM_VIEW);
        break;
    default:
        break;
    }
}

/* ---- MEM_VIEWER screen ------------------------------------------------- */

#define MEM_VIEW_COLS  6   /* bytes per hex+ascii row */
#define MEM_VIEW_ROWS  6   /* visible rows in hex+ascii mode */

static void RenderMemViewer(void)
{
    char buf[40];
    uint8_t y = 9;

    const uint8_t           *data = RamWatch_GetPtr(memBufSelectedIndex);
    const RamWatchEntry_t   *ent  = &ram_watch[memBufSelectedIndex];
    uint32_t                 size = RamWatch_GetSize(memBufSelectedIndex);

    /* ---- inverted header ---- */
    for (uint8_t px = 0; px < 128; ++px)
        for (uint8_t py = 0; py < 7; ++py)
            PutPixel(px, py, true);

    const char *mode_str =
        (memViewMode == MEM_VIEW_BIN)  ? "BIN"  :
        (memViewMode == MEM_VIEW_INFO) ? "INFO" : "HEX+ASC";
    snprintf(buf, sizeof(buf), "%.9s %s", ent->name, mode_str);
    GUI_DisplaySmallest(buf, 1, 1, false, false);

    /* ---- NULL / unallocated guard ---- */
    if (data == NULL || size == 0u) {
        GUI_DisplaySmallest("not allocated", 1, y, false, true); y += 8;
        GUI_DisplaySmallest("MENU=mode EXIT=back", 1, y, false, true);
        return;
    }

    /* clamp offset */
    if (memViewOffset >= (uint16_t)size)
        memViewOffset = (uint16_t)(size - 1u);

    /* ---- HEX+ASCII mode ---- */
    if (memViewMode == MEM_VIEW_HEX_ASCII) {
        for (uint8_t row = 0; row < MEM_VIEW_ROWS; ++row) {
            uint16_t off = memViewOffset + (uint16_t)(row * MEM_VIEW_COLS);
            if (off >= (uint16_t)size) break;

            /* Build "OOOO: HH HH HH HH HH HH  AAAAAA" */
            uint8_t n = (uint8_t)snprintf(buf, sizeof(buf), "%04X:", (unsigned)off);
            if (n >= sizeof(buf)) n = (uint8_t)(sizeof(buf) - 1u);
            uint8_t ascii_col[MEM_VIEW_COLS];
            uint8_t valid = 0;
            for (uint8_t col = 0; col < MEM_VIEW_COLS; ++col) {
                uint16_t bo = off + col;
                if (n + 4u < sizeof(buf)) {
                    if (bo < (uint16_t)size) {
                        uint8_t b = data[bo];
                        int r = snprintf(buf + n, sizeof(buf) - n, " %02X", (unsigned)b);
                        if (r > 0) n = (uint8_t)(n + (uint8_t)r);
                        ascii_col[col] = (b >= 0x20u && b <= 0x7Eu) ? b : (uint8_t)'.';
                        ++valid;
                    } else {
                        buf[n++] = ' '; buf[n++] = ' '; buf[n++] = ' ';
                        ascii_col[col] = ' ';
                    }
                }
            }
            /* two-space separator then ASCII */
            if (n + 2u < sizeof(buf)) {
                buf[n++] = ' '; buf[n++] = ' ';
            }
            for (uint8_t col = 0; col < valid && n + 1u < sizeof(buf); ++col)
                buf[n++] = (char)ascii_col[col];
            buf[n] = '\0';

            GUI_DisplaySmallest(buf, 1, y, false, true);
            y += 8;
        }

    /* ---- BIN detail mode ---- */
    } else if (memViewMode == MEM_VIEW_BIN) {
        uint8_t bv = data[memViewOffset];

        snprintf(buf, sizeof(buf), "ofs:%u", (unsigned)memViewOffset);
        GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

        snprintf(buf, sizeof(buf), "hex:%02X  dec:%u", (unsigned)bv, (unsigned)bv);
        GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

        char ac = (bv >= 0x20u && bv <= 0x7Eu) ? (char)bv : '.';
        snprintf(buf, sizeof(buf), "asc:%c", ac);
        GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

        snprintf(buf, sizeof(buf), "bin:%u%u%u%u %u%u%u%u",
                 (bv >> 7) & 1, (bv >> 6) & 1,
                 (bv >> 5) & 1, (bv >> 4) & 1,
                 (bv >> 3) & 1, (bv >> 2) & 1,
                 (bv >> 1) & 1, (bv >> 0) & 1);
        GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

        snprintf(buf, sizeof(buf), "%u/%lu B", (unsigned)memViewOffset,
                 (unsigned long)size);
        GUI_DisplaySmallest(buf, 1, y, false, true);

    /* ---- INFO mode ---- */
    } else {
        snprintf(buf, sizeof(buf), "src:%s", ent->section);
        GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

        snprintf(buf, sizeof(buf), "size:%lu B", (unsigned long)size);
        GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

        if (ent->elem_count > 0u && ent->elem_size > 0u) {
            snprintf(buf, sizeof(buf), "elem:%ux%u",
                     (unsigned)ent->elem_count, (unsigned)ent->elem_size);
            GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;
        }

        /* show pointer as hex address */
        snprintf(buf, sizeof(buf), "ptr:%08lX",
                 (unsigned long)(uintptr_t)data);
        GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

        snprintf(buf, sizeof(buf), "state:%s",
                 ent->dyn ? ((*ent->dyn != NULL) ? "alloc" : "empty") : "static");
        GUI_DisplaySmallest(buf, 1, y, false, true);
    }
}

static void OnKeyDownMemViewer(uint8_t key)
{
    BACKLIGHT_TurnOn();
    const uint32_t size = RamWatch_GetSize(memBufSelectedIndex);

    if (size == 0u || RamWatch_GetPtr(memBufSelectedIndex) == NULL) {
        switch (key) {
        case KEY_MENU:
            memViewMode = (MemViewMode_t)((memViewMode + 1u) % MEM_VIEW_MODE_COUNT);
            break;
        case KEY_EXIT:
            SetState(MEM_BUFFERS);
            break;
        default:
            break;
        }
        return;
    }

    uint16_t step = (memViewMode == MEM_VIEW_HEX_ASCII)
                    ? (uint16_t)MEM_VIEW_COLS
                    : 1u;

    switch (key) {
    case KEY_UP:
        if (memViewMode == MEM_VIEW_INFO) break; /* no scroll in INFO */
        if (memViewOffset >= step)
            memViewOffset -= step;
        else
            memViewOffset = 0;
        break;
    case KEY_DOWN:
        if (memViewMode == MEM_VIEW_INFO) break;
        if ((uint32_t)(memViewOffset + step) < size)
            memViewOffset += step;
        else
            memViewOffset = (uint16_t)(size > step ? size - step : 0u);
        break;
    case KEY_MENU:
        memViewMode = (MemViewMode_t)((memViewMode + 1u) % MEM_VIEW_MODE_COUNT);
        /* keep offset in-bounds after mode switch */
        if (memViewOffset >= (uint16_t)size)
            memViewOffset = (uint16_t)(size - 1u);
        break;
    case KEY_EXIT:
        SetState(MEM_BUFFERS);
        break;
    default:
        break;
    }
}

/* ----------------------------------------------------------------------- */

static void RenderRAMView(void)
{
    char buf[32];
    uint8_t y;

    /* ---- header row (inverted) ---- */
    for (uint8_t px = 0; px < 128; ++px)
        for (uint8_t py = 0; py < 7; ++py)
            PutPixel(px, py, true);
    GUI_DisplaySmallest("RAM STATS [MENU=bufs]", 1, 1, false, false);

    /* ---- data rows ---- */
    y = 9;
    uint32_t stat  = MemStats_GetStaticRAM();
    uint32_t hused = MemStats_GetHeapUsed();
    uint32_t hpeak = MemStats_GetHeapPeak();
    uint32_t hfree = MemStats_GetFreeGap();
    uint32_t total = MemStats_GetRAMTotal();

    snprintf(buf, sizeof(buf), ".data+.bss: %5lu B", (unsigned long)stat);
    GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

    snprintf(buf, sizeof(buf), "Heap now:   %5lu B", (unsigned long)hused);
    GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

    snprintf(buf, sizeof(buf), "Heap peak:  %5lu B", (unsigned long)hpeak);
    GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

    snprintf(buf, sizeof(buf), "Free gap:   %5lu B", (unsigned long)hfree);
    GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

    /* RAM usage percentage (integer arithmetic, no floats) */
    uint32_t used_total = stat + hpeak;
    uint16_t pct = (used_total <= total) ? (uint16_t)((used_total * 100u) / total) : 100u;
    snprintf(buf, sizeof(buf), "RAM total: %5lu B", (unsigned long)total);
    GUI_DisplaySmallest(buf, 1, y, false, true); y += 8;

    snprintf(buf, sizeof(buf), "Used: %lu+%lu=%u%%", (unsigned long)stat,
             (unsigned long)hpeak, (unsigned)pct);
    GUI_DisplaySmallest(buf, 1, y, false, true);
}

static void OnKeyDownRAMView(uint8_t key)
{
    BACKLIGHT_TurnOn();
    switch (key) {
    case KEY_MENU:
        /* Navigate forward to the buffer list screen. */
        memBufSelectedIndex = 0;
        memBufScrollOffset  = 0;
        SetState(MEM_BUFFERS);
        break;
    case KEY_8:
        /* Cycle forward to the CPU info screen. */
        SetState(CPU_VIEW);
        break;
    case KEY_EXIT:
        /* Return to the spectrum view that was active before RAM_VIEW. */
        SetState(SPECTRUM);
        break;
    default:
        break;
    }
}

/* ----------------------------------------------------------------------- */

static void RenderCPUView(void)
{
    char buf[32];

    /* ---- header row (inverted) ---- */
    for (uint8_t px = 0; px < 128; ++px)
        for (uint8_t py = 0; py < 7; ++py)
            PutPixel(px, py, true);
    GUI_DisplaySmallest("CPU INFO [8=VOICE]", 1, 1, false, false);

    /* ---- CPU temperature (large font) ---- */
    int16_t temp_dc = CpuTemp_ReadDeciCelsius();
    int16_t t_int  = temp_dc / 10;
    int16_t t_frac = (int16_t)abs(temp_dc % 10);
    snprintf(buf, sizeof(buf), "%d.%d C", (int)t_int, (int)t_frac);
    /* UI_PrintString: centred between x=0..127, line index 1 (~row 8) */
    UI_PrintString(buf, 0, 127, 1, 8);

    /* ---- Ambient temperature estimate ---- */
    /* ambient_est = cpu_temp - CPU_AMBIENT_OFFSET (default 8.0 degC) */
#define CPU_AMBIENT_OFFSET_DC  80   /* 8.0 degC in deci-Celsius */
    int16_t amb_dc = temp_dc - CPU_AMBIENT_OFFSET_DC;
    int16_t a_int  = amb_dc / 10;
    int16_t a_frac = (int16_t)abs(amb_dc % 10);
    snprintf(buf, sizeof(buf), "Amb~%d.%d C", (int)a_int, (int)a_frac);
    /* Large font (UI_PrintString) occupies line 1, which is ~8px tall with 8px char height.
     * Line 1 ends at approximately pixel row 8+8+8=24; leave a small gap -> row 33. */
    GUI_DisplaySmallest(buf, 1, 33, false, true);
}

static void OnKeyDownCPUView(uint8_t key)
{
    BACKLIGHT_TurnOn();
    switch (key) {
    case KEY_8:
        /* Cycle back to RAM diagnostics. */
        SetState(RAM_VIEW);
        break;
    case KEY_EXIT:
        /* Return to the spectrum view. */
        SetState(SPECTRUM);
        break;
    default:
        break;
  }
}
#endif // ENABLE_CPU_STATS

ChannelInfo_t FetchChannelFrequency(const uint16_t Channel) {
    ChannelInfo_t info;
    PY25Q16_ReadBuffer(0x0000 + (uint32_t)Channel * 16, &info, sizeof(info));
    if (info.frequency == 0xFFFFFFFF) {
        ChannelInfo_t empty = {0, 0};
        return empty;
    }
    return info;
}

uint16_t BOARD_gMR_fetchChannel(const uint32_t freq) {
		for (uint16_t i = MR_CHANNEL_FIRST; i <= MR_CHANNEL_LAST; i++) {
            ChannelInfo_t freqcmp = FetchChannelFrequency(i);
            if (freqcmp.frequency == freq) return i;
		}
		return 0xFFFF;
}

uint16_t RADIO_ValidMemoryChannelsCount(bool bCheckScanList, uint8_t CurrentScanList)
{
	uint16_t count=0;
	for (uint16_t i = MR_CHANNEL_FIRST; i<=MR_CHANNEL_LAST; ++i) {
			if(RADIO_CheckValidChannel(i, bCheckScanList, CurrentScanList)) count++;
		}
	return count;
}

static void LoadActiveBands(void) {
    memset(BParams, 0, (MAX_BANDS) * sizeof(bandparameters));
    bandCount = 0;
    for (uint16_t bd = 0; bd < MAX_BANDS; bd++)
    {
        gChannel = bd + 999;
        LookupChannelModulation(); //Fill BParams modulation and step
        BParams[bd].modulationType = channelModulation;
        BParams[bd].scanStep =  channelStep;
//char str[64] = "";
//sprintf(str, "%d %d %d\r\n", bd, channelModulation, channelStep);LogUart(str);
        ChannelInfo_t freqs = FetchChannelFrequency(gChannel);
        if(!freqs.frequency) return;
        BParams[bd].Startfrequency = freqs.frequency;
        BParams[bd].Stopfrequency  = freqs.offset;
        PY25Q16_ReadBuffer(0x004000 + (gChannel * 16), BParams[bd].BandName, 10);
        bandCount++;
//char str[64] = "";
//sprintf(str, "%d %d %d %d %s\r\n", bd, gChannel, BParams[bd].Startfrequency, BParams[bd].Stopfrequency, BParams[bd].BandName);LogUart(str);
    }
}

static void LoadSingleScanListFrequencies(uint8_t listIndex)
{   if(appMode!=CHANNEL_MODE) return;
    if(listIndex >= validScanListCount) { LoadActiveScanFrequencies(); return; }
    uint8_t realList = validScanListIndices[listIndex] + 1;  // 1-based scanlist number
    memset(ScanFrequencies, 0, (MR_CHANNEL_LAST + 1) * sizeof(uint32_t));
    scanChannelsCount = 0;
    ChannelAttributes_t cache;
    for (uint16_t ch = MR_CHANNEL_FIRST; ch <= MR_CHANNEL_LAST; ch++) {
        MR_LoadChannelAttributesFromFlash(ch, &cache);
        if (cache.scanlist == realList) {
            ChannelInfo_t freqs = FetchChannelFrequency(ch);
            if (freqs.frequency) {
                ScanFrequencies[scanChannelsCount++] = freqs.frequency;
            }
        }
    }
    if (!scanChannelsCount) LoadActiveScanFrequencies();  // fallback
}

static void LoadActiveScanFrequencies(void)
{   if(appMode!=CHANNEL_MODE) return;
    memset(ScanFrequencies, 0, (MR_CHANNEL_LAST + 1) * sizeof(uint32_t));
    scanChannelsCount = 0;
    ChannelAttributes_t cache;
    for (uint16_t ch = MR_CHANNEL_FIRST; ch <= MR_CHANNEL_LAST; ch++)
    {
        MR_LoadChannelAttributesFromFlash(ch, &cache);
        if (cache.scanlist <= MR_CHANNELS_LIST) {
            ChannelInfo_t freqs  = FetchChannelFrequency(ch);
            if (freqs.frequency) {
                if (settings.scanListEnabled[cache.scanlist-1])
                    {   ScanFrequencies[scanChannelsCount] = freqs.frequency;
                        scanChannelsCount++;
                    }
                }
        }
#ifdef ENABLE_DEV
char str[64] = "";sprintf(str, "LASF %d %d %d\r\n", ch,settings.scanListEnabled[cache.scanlist-1],ScanFrequencies[scanChannelsCount-1]);LogUart(str);
#endif
    }
    if (!scanChannelsCount) { //No active scanlist
    for (uint16_t ch = MR_CHANNEL_FIRST; ch <= MR_CHANNEL_LAST; ch++)
    {
        ChannelInfo_t freqs  = FetchChannelFrequency(ch);
        if (freqs.frequency) {
                {   ScanFrequencies[scanChannelsCount] = freqs.frequency;
                    scanChannelsCount++;
                }
            }
    }
    }
}

static void LoadMonitorFrequencies(void)
{   
    memset(MonitorFreqs, 0, (MONITOR_SIZE) * sizeof(uint32_t));
    monitorChannelsCount = 0;
    ChannelAttributes_t cache;
    for (uint16_t ch = MR_CHANNEL_FIRST; ch <= MR_CHANNEL_LAST; ch++)
    {   MR_LoadChannelAttributesFromFlash(ch, &cache);
        if (cache.scanlist == 21) {
            ChannelInfo_t freqs  = FetchChannelFrequency(ch);
            if (freqs.frequency) {
                MonitorFreqs[monitorChannelsCount] = freqs.frequency;
                if (++monitorChannelsCount > MONITOR_SIZE) return; //Limit monitor freqs
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_SPECTRUM_LINES
static void MyDrawShortHLine(uint8_t y, uint8_t x_start, uint8_t x_end, uint8_t step, bool white); //ПРОСТОЙ РЕЖИМ ЛИНИИ
static void MyDrawVLine(uint8_t x, uint8_t y_start, uint8_t y_end, uint8_t step); //ПРОСТОЙ РЕЖИМ ЛИНИИ
#endif

const RegisterSpec allRegisterSpecs[] = {
    {"13_LNAs",  0x13, 8, 0b11,  1},
    {"13_LNA",   0x13, 5, 0b111, 1},
    {"13_PGA",   0x13, 0, 0b111, 1},
    {"13_MIX",   0x13, 3, 0b11,  1},
    {"XTAL F Mode Select", 0x3C, 6, 0b11, 1},
// {"--DEV & MIC--",},
    {"RF Tx Deviation", 0x40, 0, 0xFFF, 10},
    {"Compress AF Tx Ratio", 0x29, 14, 0b11, 1},
    {"Compress AF Tx 0 dB", 0x29, 7, 0x7F, 1},
    {"Compress AF Tx noise", 0x29, 0, 0x7F, 1},
    {"MIC AGC Disable", 0x19, 15, 1, 1},
// {"----AFC----",},
    {"AFC Range Select", 0x73, 11, 0b111, 1},
    {"AFC Disable", 0x73, 4, 1, 1},
    {"AFC Speed", 0x73, 5, 0b111111, 1},
    {"3kHz AF Resp K Tx", 0x74, 0, 0xFFFF, 100},
    {"300Hz AF Resp K Tx", 0x44, 0, 0xFFFF, 100},
    {"300Hz AF Resp K Tx", 0x45, 0, 0xFFFF, 100},
//  {"--RX FILT--",},
     {"300Hz AF Resp K Rx", 0x54, 0, 0xFFFF, 100},
     {"300Hz AF Resp K Rx", 0x55, 0, 0xFFFF, 100},
     {"3kHz AF Resp K Rx", 0x75, 0, 0xFFFF, 100},
};

#define STILL_REGS_MAX_LINES 3
static uint8_t stillRegSelected = 0;
static uint8_t stillRegScroll = 0;
static bool stillEditRegs = false; // false = edycja czestotliwosci, true = edycja rejestrow

uint16_t statuslineUpdateTimer = 0;

static void RelaunchScan();
static void ResetInterrupts();
static char StringCode[10] = "";

static bool parametersStateInitialized = false;

//
static char osdPopupText[32] = "";

// 
static void ShowOSDPopup(const char *str)
{   osdPopupTimer = osdPopupSetting;
    strncpy(osdPopupText, str, sizeof(osdPopupText)-1);
    osdPopupText[sizeof(osdPopupText)-1] = '\0';
}

static uint32_t stillFreq = 0;
static uint32_t GetInitialStillFreq(void) {
    uint32_t f = 0;

    if (historyListActive) {
        f = HFreqs[historyListIndex];
    } else if (SpectrumMonitor) {
        f = lastReceivingFreq;
    } else if (gIsPeak) {
        f = peak.f;
    } else {
        f = scanInfo.f;
    }

    if (f < 1400000 || f > 130000000) {
        if (scanInfo.f >= 1400000 && scanInfo.f <= 130000000) return scanInfo.f;
        if (currentFreq >= 1400000 && currentFreq <= 130000000) return currentFreq;
        return gScanRangeStart; // ostateczny fallback
    }

    return f;
}

static uint16_t GetRegMenuValue(uint8_t st) {
  RegisterSpec s = allRegisterSpecs[st];
  return (BK4819_ReadRegister(s.num) >> s.offset) & s.mask;
}

static void SetRegMenuValue(uint8_t st, bool add) {
  uint16_t v = GetRegMenuValue(st);
  RegisterSpec s = allRegisterSpecs[st];

  uint16_t reg = BK4819_ReadRegister(s.num);
  if (add && v <= s.mask - s.inc) {
    v += s.inc;
  } else if (!add && v >= 0 + s.inc) {
    v -= s.inc;
  }
  reg &= ~(s.mask << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
  
}

KEY_Code_t GetKey() {
  KEY_Code_t btn = KEYBOARD_Poll();
  if (GPIO_IsPttPressed()) {btn = KEY_PTT;}
  if (gSetting_nav_invert) {
    if (btn == KEY_UP)   btn = KEY_DOWN;
    else if (btn == KEY_DOWN) btn = KEY_UP;
  }
  return btn;
}



static void SetState(State state) {
  previousState = currentState;
  currentState = state;
}

// ============================================================
// SECTION: Radio / hardware functions
// ============================================================

static void BackupRegisters() {
  R30 = BK4819_ReadRegister(BK4819_REG_30);
  R37 = BK4819_ReadRegister(BK4819_REG_37);
  R3D = BK4819_ReadRegister(BK4819_REG_3D);
  R43 = BK4819_ReadRegister(BK4819_REG_43);
  R47 = BK4819_ReadRegister(BK4819_REG_47);
  R48 = BK4819_ReadRegister(BK4819_REG_48);
  R7E = BK4819_ReadRegister(BK4819_REG_7E);
  R02 = BK4819_ReadRegister(BK4819_REG_02);
  R3F = BK4819_ReadRegister(BK4819_REG_3F);
  R7B = BK4819_ReadRegister(BK4819_REG_7B);
  R12 = BK4819_ReadRegister(BK4819_REG_12);
  R11 = BK4819_ReadRegister(BK4819_REG_11);
  R14 = BK4819_ReadRegister(BK4819_REG_14);
  R54 = BK4819_ReadRegister(BK4819_REG_54);
  R55 = BK4819_ReadRegister(BK4819_REG_55);
  R75 = BK4819_ReadRegister(BK4819_REG_75);
}

static void RestoreRegisters() {
  BK4819_WriteRegister(BK4819_REG_30, R30);
  BK4819_WriteRegister(BK4819_REG_37, R37);
  BK4819_WriteRegister(BK4819_REG_3D, R3D);
  BK4819_WriteRegister(BK4819_REG_43, R43);
  BK4819_WriteRegister(BK4819_REG_47, R47);
  BK4819_WriteRegister(BK4819_REG_48, R48);
  BK4819_WriteRegister(BK4819_REG_7E, R7E);
  BK4819_WriteRegister(BK4819_REG_02, R02);
  BK4819_WriteRegister(BK4819_REG_3F, R3F);
  BK4819_WriteRegister(BK4819_REG_7B, R7B);
  BK4819_WriteRegister(BK4819_REG_12, R12);
  BK4819_WriteRegister(BK4819_REG_11, R11);
  BK4819_WriteRegister(BK4819_REG_14, R14);
  BK4819_WriteRegister(BK4819_REG_54, R54);
  BK4819_WriteRegister(BK4819_REG_55, R55);
  BK4819_WriteRegister(BK4819_REG_75, R75);
}

static void ToggleAFBit(bool on) {
  uint32_t reg = regs_cache[BK4819_REG_47]; //KARINA mod
    reg &= ~(1 << 8);
    if (on)
        reg |= on << 8;
    BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void ToggleAFDAC(bool on) {
  uint32_t Reg = regs_cache[BK4819_REG_30]; //KARINA mod
    Reg &= ~(1 << 9);
    if (on)
        Reg |= (1 << 9);
    BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void SetF(uint32_t sf) {
  uint32_t f = sf;
  if (f < 1400000 || f > 130000000) return;
  if (SPECTRUM_PAUSED) return;
  BK4819_SetFrequency(f);
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

static void ResetInterrupts()
{
  // disable interupts
  BK4819_WriteRegister(BK4819_REG_3F, 0);
  // reset the interrupt
  BK4819_WriteRegister(BK4819_REG_02, 0);
}

// scan step in 0.01khz
static uint32_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }

static uint16_t GetStepsCount() 
{ 
  if (appMode==CHANNEL_MODE)    { return scanChannelsCount; }
  if (appMode==SCAN_RANGE_MODE) { return (gScanRangeStop - gScanRangeStart) / scanInfo.scanStep;}
  if (appMode==SCAN_BAND_MODE)  { return (gScanRangeStop - gScanRangeStart) / scanInfo.scanStep;}
  
  return 128 >> settings.stepsCount;
}

static uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }

static uint16_t GetRandomChannelFromRSSI(uint16_t maxChannels) {
  uint32_t rssi = rssiHistory[1]*rssiHistory[maxChannels/2];
  if (maxChannels == 0 || rssi == 0) { return 1; }
    // Scale RSSI to [1, maxChannels]
    return 1 + (rssi % maxChannels);
}

static void DeInitSpectrum() {
  RestoreRegisters();
  gVfoConfigureMode = VFO_CONFIGURE;
  isInitialized = false;
  SetState(SPECTRUM);
  #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        gEeprom.CURRENT_STATE = 0;
        SETTINGS_WriteCurrentState();
  #endif
  ToggleRX(0);
  SYSTEM_DelayMs(50);
}

static void DeleteHistoryItem(void) {
    if (!historyListActive || indexFs == 0) return;
    if (historyListIndex >= indexFs) {
        historyListIndex = (indexFs > 0) ? indexFs - 1 : 0;
        if (indexFs == 0) return;
    }
    uint16_t indexToDelete = historyListIndex;
    for (uint16_t i = indexToDelete; i < indexFs - 1; i++) {
        HFreqs[i]       = HFreqs[i + 1];
        HBlacklisted[i] = HBlacklisted[i + 1];
    }
    indexFs--;
    
    HFreqs[indexFs]       = 0;
    HBlacklisted[indexFs] = 0xFF;

    if (historyListIndex >= indexFs && indexFs > 0) {
        historyListIndex = indexFs - 1;
    } else if (indexFs == 0) {
        historyListIndex = 0;
    }
    ShowOSDPopup("Deleted");
    
}


#include "settings.h"

static void SaveHistoryToFreeChannel(void) {
    if (!historyListActive) return;

    uint32_t f = HFreqs[historyListIndex];
    if (f < 1000000) return;
    char str[32];
    for (int i = 0; i < MR_CHANNEL_LAST; i++) {
        uint32_t freqInMem;
        PY25Q16_ReadBuffer(0x0000 + (i * 16), (uint8_t *)&freqInMem, 4);
        if (freqInMem != 0xFFFFFFFF && freqInMem == f) {
            sprintf(str, "Exist CH %d", i + 1);
            ShowOSDPopup(str);
            return;
        }
    }
    int freeCh = -1;
    for (int i = 0; i < MR_CHANNEL_LAST; i++) {
        uint8_t checkByte;
        PY25Q16_ReadBuffer(0x0000 + (i * 16), &checkByte, 1);
        if (checkByte == 0xFF) { 
            freeCh = i;
            break;
        }
    }

    if (freeCh != -1) {
        VFO_Info_t tempVFO;
        memset(&tempVFO, 0, sizeof(tempVFO)); 
        tempVFO.freq_config_RX.Frequency = f;
        tempVFO.freq_config_TX.Frequency = f; 
        tempVFO.TX_OFFSET_FREQUENCY = 0;
        tempVFO.Modulation = settings.modulationType;
        tempVFO.CHANNEL_BANDWIDTH = settings.listenBw; 
        tempVFO.OUTPUT_POWER = OUTPUT_POWER_LOW1;
        tempVFO.STEP_SETTING = STEP_12_5kHz; 
        SETTINGS_SaveChannel(freeCh,0, &tempVFO, 2);
        LoadActiveScanFrequencies();
        sprintf(str, "SAVED TO CH %d", freeCh + 1);
        ShowOSDPopup(str);
    } else {
        ShowOSDPopup("MEMORY FULL");
    }
}

typedef struct HistoryStruct {
    uint32_t HFreqs;
    uint8_t HBlacklisted;
} HistoryStruct;


static bool historyLoaded = false; // flaga stanu wczytania histotii spectrum

void LoadHistory(void) {
    HistoryStruct History = {0};
    for (uint16_t position = 0; position < HISTORY_SIZE; position++) {
        PY25Q16_ReadBuffer(ADRESS_HISTORY + position * sizeof(HistoryStruct),
                          (uint8_t *)&History, sizeof(HistoryStruct));

        // Stop si marque de fin trouvée
        if (History.HBlacklisted == 0xFF) {
            indexFs = position;
            break;
        }
      if (History.HFreqs){
        HFreqs[position] = History.HFreqs;
        HBlacklisted[position] = History.HBlacklisted;
        indexFs = position + 1;
      }
    }
}


void WriteHistory(void) {
    HistoryStruct History = {0};
    for (uint16_t position = 0; position < indexFs; position++) {
        History.HFreqs = HFreqs[position];
        History.HBlacklisted = HBlacklisted[position];
        PY25Q16_WriteBuffer(ADRESS_HISTORY + position * sizeof(HistoryStruct),
                           (uint8_t *)&History, sizeof(HistoryStruct), 0);
    }

    // Marque de fin (HBlacklisted = 0xFF)
    History.HFreqs = 0;
    History.HBlacklisted = 0xFF;
    PY25Q16_WriteBuffer(ADRESS_HISTORY + indexFs * sizeof(HistoryStruct),
                       (uint8_t *)&History, sizeof(HistoryStruct), 0);
    
    ShowOSDPopup("HISTORY SAVED");
}

static void ExitAndCopyToVfo() {
    RestoreRegisters();

    if (historyListActive) {
        SetF(HFreqs[historyListIndex]);
        gCurrentVfo->Modulation = MODULATION_FM;
        gRequestSaveChannel = 1;
        DeInitSpectrum();
    }

    switch (currentState) {
        case SPECTRUM:
            // PTT Mode 1: NINJA MODE (Random channel with low RSSI)
            if (PttEmission == 1 && scanChannelsCount > 0) {
                uint16_t randomChannel = GetRandomChannelFromRSSI(scanChannelsCount);
                uint32_t rndfreq = 0;
                uint16_t attempts = 0;
                SpectrumDelay = 0; //not compatible with ninja
                while (attempts < scanChannelsCount) {
                    rndfreq = ScanFrequencies[randomChannel];
                    if (rssiHistory[randomChannel] <= 120 && rndfreq) {break;}
                    attempts++;
                    randomChannel = (randomChannel + 1) % scanChannelsCount;
                }
                if (rndfreq) {
                    gCurrentVfo->freq_config_TX.Frequency = rndfreq;
                    gCurrentVfo->freq_config_RX.Frequency = rndfreq;
                    gEeprom.MrChannel[0]     = randomChannel;
                    gEeprom.ScreenChannel[0] = randomChannel;
                    gCurrentVfo->Modulation   = MODULATION_FM;
                    gCurrentVfo->STEP_SETTING = STEP_0_01kHz;
                    gRequestSaveChannel       = 1;
                }
            }
            // PTT Mode 2: Last RX
            if (PttEmission == 2) {
                SpectrumDelay = 0;
                gCurrentVfo->freq_config_TX.Frequency = lastReceivingFreq;
                gCurrentVfo->freq_config_RX.Frequency = lastReceivingFreq;
                gEeprom.MrChannel[0]     = 0;
                gEeprom.ScreenChannel[0] = 0;
                gCurrentVfo->STEP_SETTING = STEP_0_01kHz;
                gCurrentVfo->Modulation   = MODULATION_FM;
                gCurrentVfo->OUTPUT_POWER  = OUTPUT_POWER_HIGH;
                gRequestSaveChannel        = 1;
            }
            // PTT Mode 0: VFO Freq
            gComeBack = 1;
            DeInitSpectrum();
            break;

        default:
            DeInitSpectrum();
            break;
    }

    SYSTEM_DelayMs(200);
    isInitialized = false;
}

static uint16_t GetRssi(void) {
    uint16_t rssi;
    SYSTICK_DelayUs(DelayRssi * 1000);
    rssi = BK4819_GetRSSI();
    if (FREQUENCY_GetBand(scanInfo.f) > BAND4_174MHz) {rssi += UHF_NOISE_FLOOR;}
    BK4819_ReadRegister(0x63);
  return rssi;
}

static void ToggleAudio(bool on) {
    if (on == audioState) { return; }
    audioState = on;
    if (on) {AUDIO_AudioPathOn();}
    else {AUDIO_AudioPathOff();}
}

static uint16_t CountValidHistoryItems() {
    return (indexFs > HISTORY_SIZE) ? HISTORY_SIZE : indexFs;
}

static void FillfreqHistory(bool countHit)
{
    uint32_t f = peak.f;
    if (f == 0 || f < 1400000 || f > 130000000) return;

    uint16_t foundIndex = 0xFFFF;
    bool foundBlacklisted = false;

    for (uint16_t i = 0; i < indexFs; i++) {
        if (HFreqs[i] == f) {
            foundIndex = i;
            foundBlacklisted = HBlacklisted[i];
            break;
        }
    }
    bool freezeOrder = historyListActive && (SpectrumMonitor || gHistoryScan);
    if (freezeOrder) {
            lastReceivingFreq = f;
            return;
        }

    if (foundIndex != 0xFFFF) {
        for (uint16_t i = foundIndex; i + 1 < indexFs; i++) {
            HFreqs[i]       = HFreqs[i + 1];
            HBlacklisted[i] = HBlacklisted[i + 1];
        }
        if (indexFs > 0) indexFs--;
    }

    uint16_t limit = (indexFs < HISTORY_SIZE) ? indexFs : (HISTORY_SIZE - 1);
    for (int i = limit; i > 0; i--) {
        HFreqs[i]       = HFreqs[i - 1];
        HBlacklisted[i] = HBlacklisted[i - 1];
    }

    HFreqs[0] = f;
    HBlacklisted[0] = foundBlacklisted;

    if (indexFs < HISTORY_SIZE) indexFs++;
    historyListIndex = 0;
    lastReceivingFreq = f;
} 

static void ToggleRX(bool on) {
    if (SPECTRUM_PAUSED) return;
    if(!on && SpectrumMonitor == 2) {isListening = 1;return;}
    isListening = on;
    gChannel = BOARD_gMR_fetchChannel(scanInfo.f);
    isKnownChannel = (gChannel != 0xFFFF);
    
    if (on && isKnownChannel) {
        LookupChannelModulation();
        settings.modulationType = channelModulation;
        SETTINGS_FetchChannelName(channelName,gChannel );
        if(!gForceModulation) settings.modulationType = channelModulation;
        RADIO_SetupAGC(settings.modulationType == MODULATION_AM, false);
    }
    else if(on && appMode == SCAN_BAND_MODE) {
            if (!gForceModulation) settings.modulationType = BParams[bl].modulationType;
            RADIO_SetupAGC(settings.modulationType == MODULATION_AM, false);
          }
    
    if (on) { 
        Fmax = peak.f;

        //BK4819_WriteRegister(BK4819_REG_37, 0x1D0F); // 0x1D0F defoult. 0x1D0F is ok for me
        BK4819_RX_TurnOn();
        SYSTEM_DelayMs(20);
        RADIO_SetModulation(settings.modulationType);
        BK4819_SetFilterBandwidth(settings.listenBw, false);
        BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_02_CxCSS_TAIL);

    } else { 
        
        //BK4819_WriteRegister(BK4819_REG_37, 0x000F);
        RADIO_SetModulation(MODULATION_FM);
        BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE, false); //Scan in 25K bandwidth
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 0);
    }
    if (on != audioState) {
        ToggleAudio(on);
        ToggleAFDAC(on);
        ToggleAFBit(on);
    }
}

#ifdef ENABLE_BENCH
static void ResetBenchStats(void) {
    benchTickMs = 0;
    benchStepsThisSec = 0;
    benchRatePerSec = 0;
    benchLapMs = 0;
    benchLastLapMs = 0;
}
#endif

static void ResetScanStats() {
  scanInfo.rssiMax = scanInfo.rssiMin + 20 ; 
}

static bool InitScan() {
    ResetScanStats();
    scanInfo.i = 0;
    peak.i = 0;
    peak.f = 0;
    
    bool scanInitializedSuccessfully = false;

    if (appMode == SCAN_BAND_MODE) {
        if (bandCount == 0) { return false; }

        // Если ни один бенд не включён — сканируем все подряд (как в CHANNEL_MODE без списков)
        bool anyEnabled = false;
        for (uint8_t i = 0; i < bandCount; i++) {
            if (settings.bandEnabled[i]) { anyEnabled = true; break; }
        }

        uint8_t checkedBandCount = 0;
        while (checkedBandCount < bandCount) {
            bool qualify = anyEnabled ? settings.bandEnabled[nextBandToScanIndex] : true;
            if (qualify) {
                bl = nextBandToScanIndex;
                scanInfo.f = BParams[bl].Startfrequency;
                scanInfo.scanStep = scanStepValues[BParams[bl].scanStep];
                settings.scanStepIndex = BParams[bl].scanStep;
                if (BParams[bl].Startfrequency > 0) gScanRangeStart = BParams[bl].Startfrequency;
                if (BParams[bl].Stopfrequency  > 0) gScanRangeStop  = BParams[bl].Stopfrequency;
                if (!gForceModulation) settings.modulationType = BParams[bl].modulationType;
                nextBandToScanIndex = (nextBandToScanIndex + 1) % bandCount;
                scanInitializedSuccessfully = true;
                break;
            }
            nextBandToScanIndex = (nextBandToScanIndex + 1) % bandCount;
            checkedBandCount++;
        }
    } else {
        if(gScanRangeStart > gScanRangeStop)
		    SWAP(gScanRangeStart, gScanRangeStop);
        scanInfo.f = gScanRangeStart;
        scanInfo.scanStep = GetScanStep();
        scanInitializedSuccessfully = true;
      }

    if (appMode == CHANNEL_MODE) {
        if (scanChannelsCount == 0) {
            return false;
        }
        scanInfo.f = ScanFrequencies[0];
        peak.f = scanInfo.f;
        peak.i = 0;
    }

    return scanInitializedSuccessfully;
}

static void ResetModifiers() {
  for (int i = 0; i < 128; ++i) {
    if (rssiHistory[i] == RSSI_MAX_VALUE) rssiHistory[i] = 0;
  }
  LoadActiveScanFrequencies();
  RelaunchScan();
}

static void RelaunchScan() {
    InitScan();
    ToggleRX(false);
    scanInfo.rssiMin = RSSI_MAX_VALUE;
    gIsPeak = false;
#ifdef ENABLE_BENCH
    	ResetBenchStats();
#endif
}

uint8_t  BK4819_GetExNoiseIndicator(void)
{
	return BK4819_ReadRegister(BK4819_REG_65) & 0x007F;
}

static void UpdateNoiseOff(){
  if( BK4819_GetExNoiseIndicator() > Noislvl_OFF) {gIsPeak = false;ToggleRX(0);}		
}

static void UpdateNoiseOn(){
	if( BK4819_GetExNoiseIndicator() < Noislvl_ON) {gIsPeak = true;ToggleRX(1);}
}

static void UpdateScanInfo() {
  if (scanInfo.rssi > scanInfo.rssiMax) {
    scanInfo.rssiMax = scanInfo.rssi;
  }
  if (scanInfo.rssi < scanInfo.rssiMin && scanInfo.rssi > 0) {
    scanInfo.rssiMin = scanInfo.rssi;
  }
}
static void UpdateGlitch() {
    uint8_t glitch = BK4819_GetGlitchIndicator();
    if (glitch > GlitchMax) {gIsPeak = false;} 
    else {gIsPeak = true;}// if glitch is too high, receiving stopped
}

static void Measure() {
    static int16_t previousRssi = 0;
    static bool isFirst = true;
    uint16_t rssi = scanInfo.rssi = GetRssi();
    UpdateScanInfo();
    if (scanInfo.f % 1300000 == 0 || IsBlacklisted(scanInfo.f)) rssi = scanInfo.rssi = 0;

    if (isFirst) {
        previousRssi = rssi;
        gIsPeak      = false;
        isFirst      = false;
    }
    if (settings.rssiTriggerLevelUp == 50 && rssi > previousRssi + UOO_trigger) {
        peak.f = scanInfo.f;
        peak.i = scanInfo.i;
        gIsPeak = true;
        FillfreqHistory(false);
    } else {
            if (!gIsPeak && rssi > previousRssi + settings.rssiTriggerLevelUp) {
                // В режимах списков пропускаем блокирующие задержки подтверждения (10+50мс),
                // чтобы не подвешивать обработчик клавиш
                bool inMenu = (currentState == BAND_LIST_SELECT  ||
                               currentState == SCANLIST_SELECT    ||
                               currentState == PARAMETERS_SELECT
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
                               || currentState == SCANLIST_CHANNELS
#endif
                               );
                if (!inMenu) {
                    SYSTEM_DelayMs(10);
                    uint16_t rssi2 = scanInfo.rssi = GetRssi();
                    if (rssi2 > rssi+10) {
                        peak.f = scanInfo.f;
                        peak.i = scanInfo.i-1;
                    }
                    if (settings.rssiTriggerLevelUp < 50) {
                        gIsPeak = true;
                        UpdateNoiseOff();
                        UpdateGlitch();
                    }
                    SYSTEM_DelayMs(50);
                    scanInfo.rssi = GetRssi();
                } else {
                    // В меню: просто фиксируем пик без задержек
                    peak.f = scanInfo.f;
                    peak.i = scanInfo.i;
                    if (settings.rssiTriggerLevelUp < 50) {
                        gIsPeak = true;
                    }
                }
            }
    } 
    if (!gIsPeak || !isListening) previousRssi = rssi;
    else if (rssi < previousRssi) previousRssi = rssi;

    uint16_t count = GetStepsCount();
    uint16_t i = scanInfo.i;
    static uint16_t lastPixel = 255;
    if (count > 128) {
        uint16_t pixel = ((uint32_t)i * 127) / count;
        if (pixel != lastPixel) {
            rssiHistory[pixel] = rssi;
            lastPixel = pixel;
        } else if (rssi > rssiHistory[pixel]) {
            rssiHistory[pixel] = rssi;
        }
    } else {
        uint16_t base = 128 / count;
        uint16_t rem  = 128 % count;
        uint16_t start = i * base + (i < rem ? i : rem);
        uint16_t end   = (i + 1) * base + ((i + 1) < rem ? (i + 1) : rem);
        if (end > 128) end = 128;
        for (uint16_t j = start; j < end; ++j) {rssiHistory[j] = rssi;}
    }

}

static void AutoAdjustFreqChangeStep() {
  settings.frequencyChangeStep = gScanRangeStop - gScanRangeStart;
}

static void UpdateScanStep(bool inc) {
if (inc) {
    settings.scanStepIndex = (settings.scanStepIndex >= STEP_500kHz) 
                          ? STEP_0_01kHz 
                          : settings.scanStepIndex + 1;
} else {
    settings.scanStepIndex = (settings.scanStepIndex <= STEP_0_01kHz) 
                          ? STEP_500kHz 
                          : settings.scanStepIndex - 1;
}
  AutoAdjustFreqChangeStep();
  scanInfo.scanStep = settings.scanStepIndex;
}

static void UpdateCurrentFreq(bool inc) {
  if (inc && currentFreq < F_MAX) {
    gScanRangeStart += settings.frequencyChangeStep;
    gScanRangeStop += settings.frequencyChangeStep;
  } else if (!inc && currentFreq > settings.frequencyChangeStep) {
    gScanRangeStart -= settings.frequencyChangeStep;
    gScanRangeStop -= settings.frequencyChangeStep;
  } else {
    return;
  }
  ResetModifiers();
  
}

static void ToggleModulation() {
  if (settings.modulationType < MODULATION_UKNOWN - 1) {
    settings.modulationType++;
  } else {
    settings.modulationType = MODULATION_FM;
  }
  RADIO_SetModulation(settings.modulationType);
  BK4819_InitAGC(settings.modulationType);
  gForceModulation = 1;
}

static void ToggleListeningBW(bool inc) {
  settings.listenBw = ACTION_NextBandwidth(settings.listenBw, false, inc);
  BK4819_SetFilterBandwidth(settings.listenBw, false);
  
}

static void ToggleStepsCount() {
  if (settings.stepsCount == STEPS_128) {
    settings.stepsCount = STEPS_16;
  } else {
    settings.stepsCount--;
  }
  AutoAdjustFreqChangeStep();
  ResetModifiers();
  
}

static void ResetFreqInput() {
  tempFreq = 0;
  for (int i = 0; i < 10; ++i) {
    freqInputString[i] = '-';
  }
}

static void FreqInput() {
  freqInputIndex = 0;
  freqInputDotIndex = 0;
  ResetFreqInput();
  SetState(FREQ_INPUT);
  Key_1_pressed = 1;
}

static void UpdateFreqInput(KEY_Code_t key) {
  if (key != KEY_EXIT && freqInputIndex >= 10) {
    return;
  }
  if (key == KEY_STAR) {
    if (freqInputIndex == 0 || freqInputDotIndex) {
      return;
    }
    freqInputDotIndex = freqInputIndex;
  }
  if (key == KEY_EXIT) {
    freqInputIndex--;
    if(freqInputDotIndex==freqInputIndex)
      freqInputDotIndex = 0;    
  } else {
    freqInputArr[freqInputIndex++] = key;
  }

  ResetFreqInput();

  uint8_t dotIndex =
      freqInputDotIndex == 0 ? freqInputIndex : freqInputDotIndex;

  KEY_Code_t digitKey;
  for (int i = 0; i < 10; ++i) {
    if (i < freqInputIndex) {
      digitKey = freqInputArr[i];
      freqInputString[i] = digitKey <= KEY_9 ? '0' + digitKey-KEY_0 : '.';
    } else {
      freqInputString[i] = '-';
    }
  }

  uint32_t base = 100000; // 1MHz in BK units
  for (int i = dotIndex - 1; i >= 0; --i) {
    tempFreq += (freqInputArr[i]-KEY_0) * base;
    base *= 10;
  }

  base = 10000; // 0.1MHz in BK units
  if (dotIndex < freqInputIndex) {
    for (int i = dotIndex + 1; i < freqInputIndex; ++i) {
      tempFreq += (freqInputArr[i]-KEY_0) * base;
      base /= 10;
    }
  }
  
}

static bool IsBlacklisted(uint32_t f) {
    for (uint16_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] == f && HBlacklisted[i]) {
            return true;
        }
    }
    return false;
}

static void Blacklist() {
    if (peak.f == 0) return;
    gIsPeak = 0;
    ToggleRX(false);
    ResetScanStats();
    NextScanStep();
    for (uint16_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] == peak.f) {
            HBlacklisted[i] = true;
            historyListIndex = i;
            gIsPeak = 0;
            return;
        }
    }

    HFreqs[indexFs]   = peak.f;
    HBlacklisted[indexFs] = true;
    historyListIndex = indexFs;
    if (++indexFs >= HISTORY_SIZE) {
      historyScrollOffset = 0;
      indexFs=0;
    }  
}


// ============================================================
// SECTION: Display / rendering helpers
// ============================================================
// applied x2 to prevent initial rounding
static uint16_t Rssi2PX(uint16_t rssi, uint16_t pxMin, uint16_t pxMax) {
  const int16_t DB_MIN = settings.dbMin << 1;
  const int16_t DB_MAX = settings.dbMax << 1;
  const int16_t DB_RANGE = DB_MAX - DB_MIN;
  const int16_t PX_RANGE = pxMax - pxMin;
  int dbm = clamp(rssi - (160 << 1), DB_MIN, DB_MAX);
  return ((dbm - DB_MIN) * PX_RANGE + DB_RANGE / 2) / DB_RANGE + pxMin;
}

static int16_t Rssi2Y(uint16_t rssi) {
  int delta = ArrowLine*8;
  return DrawingEndY + delta -Rssi2PX(rssi, delta, DrawingEndY);
}

static void DrawSpectrum(void) {
    int16_t y_baseline = Rssi2Y(0); 
    for (uint8_t i = 0; i < 128; i++) {
        int16_t y_curr = Rssi2Y(rssiHistory[i]);
        for (int16_t y = y_curr; y <= y_baseline; y++) {
                gFrameBuffer[y >> 3][i] |= (1 << (y & 7));
            }
        }
}

static void RemoveTrailZeros(char *s) {
    char *p;
    if (strchr(s, '.')) {
        p = s + strlen(s) - 1;
        while (p > s && *p == '0') {
            *p-- = '\0';
        }
        if (*p == '.') {
            *p = '\0';
        }
    }
}

static void DrawStatus() {

  int len=0;
  int pos=0;
   
switch(SpectrumMonitor) {
    case 0:
      len = sprintf(&String[pos],"");
      pos += len;
      if (settings.rssiTriggerLevelUp == 50) len = sprintf(&String[pos],"");
      else len = sprintf(&String[pos],"DS%d ", settings.rssiTriggerLevelUp);
      pos += len;
    break;

    case 1:
      len = sprintf(&String[pos],"FL ");
      pos += len;
    break;

    case 2:
      len = sprintf(&String[pos],"M ");
      pos += len;
    break;
  } 
  
  
  len = sprintf(&String[pos],"%dms %s BW%s ", DelayRssi, gModulationStr[settings.modulationType],bwNames[settings.listenBw]);
  pos += len;
  int16_t afcVal = BK4819_GetAFCValue();
  if (afcVal) {
      len = sprintf(&String[pos],"A%+d ", afcVal);
      pos += len;
  } else {
      len = sprintf(&String[pos], "ST%s", scanStepNames[settings.scanStepIndex]);
      pos += len;
    }
   
  GUI_DisplaySmallest(String, 0, 1, true,true);
  BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryCheckCounter++ % 4],&gBatteryCurrent);

  uint16_t voltage = (gBatteryVoltages[0] + gBatteryVoltages[1] + gBatteryVoltages[2] +
             gBatteryVoltages[3]) /
            4 * 760 / gBatteryCalibration[3];

  unsigned perc = BATTERY_VoltsToPercent(voltage);
  sprintf(String,"%d%%", perc);
  GUI_DisplaySmallest(String, 112, 1, true,true);
}

// ------------------ Frequency string ------------------
static void FormatFrequency(uint32_t f, char *buf, size_t buflen) {
    snprintf(buf, buflen, "%u.%05u", f / 100000, f % 100000);
    //RemoveTrailZeros(buf);
}

// ------------------ CSS detection ------------------

static void UpdateCssDetection(void) {
    if (!isListening) { return; }

    BK4819_WriteRegister(BK4819_REG_51,
        BK4819_REG_51_ENABLE_CxCSS |
        BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
        BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
        (51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

    BK4819_CssScanResult_t scanResult = BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);

    if (scanResult == BK4819_CSS_RESULT_CDCSS) {
        uint8_t code = DCS_GetCdcssCode(cdcssFreq);
        if (code != 0xFF) {
            snprintf(StringCode, sizeof(StringCode), "D%03oN", DCS_Options[code]);
            return;
        }
    } else if (scanResult == BK4819_CSS_RESULT_CTCSS) {
        uint8_t code = DCS_GetCtcssCode(ctcssFreq);
        if (code < ARRAY_SIZE(CTCSS_Options)) {
            snprintf(StringCode, sizeof(StringCode), "%u.%uHz",
                     CTCSS_Options[code] / 10, CTCSS_Options[code] % 10);
            return;
        }
    }

    StringCode[0] = '\0';
}

static void DrawF(uint32_t f) {
    static uint32_t fprev;
    if ((f == 0) || f < 1400000 || f > 130000000) f=fprev;
    else fprev = f;
    char freqStr[18];
    snprintf(freqStr, sizeof(freqStr), "%u.%05u", f / 100000, f % 100000);
    UpdateCssDetection();
    char line1[19] = "";
    char line1b[19] = "";
    char line2[19] = "";
    char line3[19] = "";
    sprintf(line1, "%s", freqStr);
    sprintf(line1b, "%s %s", freqStr, StringCode);
    char prefix[9] = "";
    if (appMode == SCAN_BAND_MODE) {
        snprintf(prefix, sizeof(prefix), "B%u ", bl + 1);
        if (isListening && isKnownChannel) {
            snprintf(line2, sizeof(line2), "%-3s%s ", prefix, channelName);
    } else {
            snprintf(line2, sizeof(line2), "%s%s", prefix, BParams[bl].BandName);
        }
    } else if (appMode == CHANNEL_MODE) {

        if (channelName[0] != '\0') {
            snprintf(line2, sizeof(line2), "%s%s ", prefix, channelName);
        } else {
            snprintf(line2, sizeof(line2), "%s", prefix);
        }
    } else {
        line2[0] = '\0';
    }

    line3[0] = '\0';
    int pos = 0;

    // line3: RX таймер + wait — показываем всегда
    if (MaxListenTime > 0) {
        pos += sprintf(&line3[pos], "RX%d|%s", spectrumElapsedCount / 1000, labels[IndexMaxLT]);
        if (WaitSpectrum > 0) {
            if (WaitSpectrum < 61000)
                pos += sprintf(&line3[pos], "%d", WaitSpectrum / 1000);
            else
                pos += sprintf(&line3[pos], "End OO");
        }
    } else {
        pos += sprintf(&line3[pos], "RX%d", spectrumElapsedCount / 1000);
        if (WaitSpectrum > 0) {
            if (WaitSpectrum < 61000)
                pos += sprintf(&line3[pos], "%d", WaitSpectrum / 1000);
            else
                pos += sprintf(&line3[pos], "End OO");
        }
    }

    if (classic) {
            if (ShowLines == 2) {
                UI_DisplayFrequency(line1, 10, 0, 0);  // BIG FREQUENCY
                if (StringCode[0]) {
                    UI_PrintStringSmallBold(StringCode, 1, LCD_WIDTH - 1, 2); // CSS bold вместо бенда/канала
                } else {
                    UI_PrintStringSmallbackground(line2, 0, LCD_WIDTH - 1, 2, 0); // бенд/канал если нет субтона
                }
                ArrowLine = 3;
            }

            if (ShowLines == 1) {
                UI_PrintStringSmallBold(line1b, 1, LCD_WIDTH - 1, 0);   // F + CSS жирный
                UI_PrintStringSmallNormal(line2, 1, LCD_WIDTH - 1, 1);  // имя/бенд
                // Строка 3 (y=17): таймер RX, затем LAST RX или макс время
                char infoStr[32] = "";
                int ipos = 0;
                if (strlen(line3) > 0) {
                    ipos += sprintf(&infoStr[ipos], "%s ", line3);  // RX таймер
                }
                if (MaxListenTime > 0) {
                    // Уже показано в line3
                } else if (lastReceivingFreq >= 1400000 && lastReceivingFreq <= 130000000) {
                    char lastRxStr[12] = "";
                    FormatFrequency(lastReceivingFreq, lastRxStr, sizeof(lastRxStr));
                    sprintf(&infoStr[ipos], "%s", lastRxStr);  // LAST RX
                }
                if (infoStr[0]) GUI_DisplaySmallestDark(infoStr, 24, 17, false, true);
                GUI_DisplaySmallestDark(">", 8, 17, false, false);
                GUI_DisplaySmallestDark("<", 118, 17, false, false);
                ArrowLine = 3;
            }

            // ShowLines==3 (LAST RX mode) removed - last rx shown in ShowLines==1 dark font
			if (classic && ShowLines == 4) {return;} // BENCH renderujemy osobno
    if (Fmax) 
      {
          FormatFrequency(Fmax, freqStr, sizeof(freqStr));
          GUI_DisplaySmallest(freqStr,  50, Bottom_print, false,true);
      }

    } else { //Not Classic

    DrawMeter(4);
    UI_DisplayFrequency(line1, 10, 2, 0);
    UI_PrintString(line2, 5, LCD_WIDTH - 1, 5, 8);
    char rssiText[16];
    if(isListening) {
        sprintf(rssiText, "RSSI:%3d", scanInfo.rssi);
        UI_PrintStringSmallbackground(rssiText, 64, 1, 0, 0);
    }
    if (StringCode[0]) { UI_PrintStringSmallbackground(StringCode, 10, 1, 0, 0);}
#ifdef ENABLE_CPU_TEMP
    else RenderCPUTemp();
#endif
    }
}

static void LookupChannelModulation() {
	uint8_t tmp;
	uint8_t data[8];
	PY25Q16_ReadBuffer(gChannel * 16 + 8, data, sizeof(data));
	tmp = data[3] >> 4;
	if (tmp >= MODULATION_UKNOWN)
		tmp = MODULATION_FM;
	channelModulation = tmp;
	if (data[4] == 0xFF) {channelBandwidth = BK4819_FILTER_BW_WIDE;}
	else {
		const uint8_t d4 = data[4];
		channelBandwidth = !!((d4 >> 1) & 1u);
		if(channelBandwidth != BK4819_FILTER_BW_WIDE)
			channelBandwidth = ((d4 >> 5) & 3u) + 1;
	}	
    tmp = data[6];
    if (tmp >= STEP_N_ELEM)
        tmp = STEP_12_5kHz;
    channelStep = tmp;
}

static void UpdateScanListCountsCached(void) {
    if (!scanListCountsDirty) return;

    BuildValidScanListIndices();
    cachedValidScanListCount = validScanListCount;
    cachedEnabledScanListCount = 0;

    for (uint8_t i = 0; i < cachedValidScanListCount; i++) {
        uint8_t realIndex = validScanListIndices[i];
        if (settings.scanListEnabled[realIndex]) {
            cachedEnabledScanListCount++;
        }
    }

    scanListCountsDirty = false;
}


static void DrawNums() {
if (appMode==CHANNEL_MODE) 
{
  UpdateScanListCountsCached();

  // Показываем текущий листаемый список
  if (validScanListCount > 0 && scanListNavIndex < validScanListCount) {
      sprintf(String, "SL:%u", validScanListIndices[scanListNavIndex] + 1);
  } else {
      sprintf(String, "SL:--");
  }
  GUI_DisplaySmallest(String, 2, Bottom_print, false, true);

  sprintf(String, "CH:%u", scanChannelsCount);
  GUI_DisplaySmallest(String, 101, Bottom_print, false, true);

  return;
}

if(appMode!=CHANNEL_MODE){
    sprintf(String, "%u.%05u", gScanRangeStart / 100000, gScanRangeStart % 100000);
    GUI_DisplaySmallest(String, 2, Bottom_print, false, true);
 
    sprintf(String, "%u.%05u", gScanRangeStop / 100000, gScanRangeStop % 100000);
    GUI_DisplaySmallest(String, 90, Bottom_print, false, true);
    }
}

static void NextScanStep() {
    spectrumElapsedCount = 0;
#ifdef ENABLE_BENCH
    benchLapDone = false;
#endif
    static uint32_t StartF;
    if (appMode == CHANNEL_MODE) {
        if (scanChannelsCount == 0) return;
#ifdef ENABLE_BENCH
        uint16_t prevI = scanInfo.i;
#endif
        if (++scanInfo.i >= scanChannelsCount) scanInfo.i = 0;
#ifdef ENABLE_BENCH
        if (scanInfo.i < prevI) benchLapDone = true;   // pełna pętla listy kanałów
#endif
        scanInfo.f = ScanFrequencies[scanInfo.i];
        return;
    }
    // FREQUENCY / SCAN_RANGE / SCAN_BAND
#ifdef ENABLE_BENCH
    uint16_t prevI = scanInfo.i;
    uint16_t steps = GetStepsCount();
#endif
    if (scanInfo.i == 0) {
        StartF = gScanRangeStart;
        scanInfo.f = StartF;
    } else {
        scanInfo.f += jumpSizes[settings.scanStepIndex];
        if (scanInfo.f >= gScanRangeStop) {
            StartF += scanInfo.scanStep;
            scanInfo.f = StartF;
        }
    }
    scanInfo.i++;
#ifdef ENABLE_BENCH
    if (scanInfo.i > steps) {
        scanInfo.i = 0;
        newScanStart = true;
        benchLapDone = true;          // pełna pętla zakresu/pasma/freq
    } else if (scanInfo.i < prevI) {
        benchLapDone = true;
    }
#else
    if (scanInfo.i > GetStepsCount()) {
        scanInfo.i = 0;
        newScanStart = true;
    }
#endif
}

static void SortHistoryByFrequencyAscending(void) {
    uint16_t count = CountValidHistoryItems();

    if (count < 2) {
        historyListIndex = 0;
        historyScrollOffset = 0;
        return;
    }

    for (uint16_t i = 0; i < count - 1; i++) {
        for (uint16_t j = i + 1; j < count; j++) {
            if (HFreqs[j] != 0 && (HFreqs[i] == 0 || HFreqs[j] < HFreqs[i])) {
                uint32_t tf = HFreqs[i];
                bool     tb = HBlacklisted[i];

                HFreqs[i] = HFreqs[j];
                HBlacklisted[i] = HBlacklisted[j];

                HFreqs[j] = tf;
                HBlacklisted[j] = tb;
            }
        }
    }

    historyListIndex = 0;
    historyScrollOffset = 0;
    ShowOSDPopup("HISTORY SORTED");  //skrocic?
}

static void CompactHistory(void) {
    uint16_t w = 0;
    uint16_t limit = (indexFs > HISTORY_SIZE) ? HISTORY_SIZE : indexFs;

    for (uint16_t r = 0; r < limit; r++) {
        if (HFreqs[r] == 0) continue;
        if (w != r) {
            HFreqs[w]       = HFreqs[r];
            HBlacklisted[w] = HBlacklisted[r];
        }
        w++;
    }

    // wyczyść resztę
    for (uint16_t i = w; i < limit; i++) {
        HFreqs[i]       = 0;
        HBlacklisted[i] = 0;
    }

    indexFs = w;
    if (indexFs == 0) {
        historyListIndex = 0;
        historyScrollOffset = 0;
    } else {
        if (historyListIndex >= indexFs) historyListIndex = indexFs - 1;
        if (historyScrollOffset >= indexFs) {
            historyScrollOffset = (indexFs > MAX_VISIBLE_LINES) ? (indexFs - MAX_VISIBLE_LINES) : 0;
        }
    }
}

static void Skip() {
    isListening = 0;
    WaitSpectrum = 0;
    spectrumElapsedCount = 0;
    NextScanStep();
    gIsPeak = false;
    ToggleRX(false);
    peak.f = scanInfo.f;
    peak.i = scanInfo.i;
    //SetF(scanInfo.f);
}

void NextAppMode(void) {
        // 0 = FR, 1 = SL, 2 = BD, 3 = RG
        if (++Spectrum_state > 3) {Spectrum_state = 0;}
        switch (Spectrum_state) {
            case 0:  appMode = FREQUENCY_MODE;  break;
            case 1:  appMode = CHANNEL_MODE;    break;
            case 2:  appMode = SCAN_RANGE_MODE; break;
            case 3:  appMode = SCAN_BAND_MODE;  break;
            default: appMode = FREQUENCY_MODE;  break;
        }
        LoadActiveScanFrequencies();
        if (!scanChannelsCount && Spectrum_state ==1) Spectrum_state++; //No SL skip SL mode
        char sText[32];
        const char* s[] = {"FREQ", "S LIST", "RANGE", "BAND"};
        sprintf(sText, "MODE: %s", s[Spectrum_state]);
        ShowOSDPopup(sText);
        gRequestedSpectrumState = Spectrum_state;
        gSpectrumChangeRequested = true;
        isInitialized = false;
        spectrumElapsedCount = 0;
        WaitSpectrum = 0;
        gIsPeak = false;
        SPECTRUM_PAUSED = false;
        SpectrumPauseCount = 0;
        newScanStart = true;
        ToggleRX(false);
}


static void SetTrigger50(){
    char triggerText[32];
    if (settings.rssiTriggerLevelUp == 50) {
        sprintf(triggerText, "DYN SQL: OFF");
        gMonitorScan = 0;
    }
    else {
        sprintf(triggerText, "DYN SQL: %d", settings.rssiTriggerLevelUp);
    }
    ShowOSDPopup(triggerText);
    Skip();
}
static const uint8_t durations[] = {0, 20, 40, 60};

// ============================================================
// SECTION: Per-state keyboard handlers
// ============================================================

/* --- BAND_LIST_SELECT: navigate and toggle band enable flags --- */
static void HandleKeyBandList(uint8_t key) {
        switch (key) {
            case KEY_UP:
                if (bandListSelectedIndex > 0) {
                    bandListSelectedIndex--;
                if (bandListSelectedIndex < bandListScrollOffset)
                        bandListScrollOffset = bandListSelectedIndex;
                } else {
                    bandListSelectedIndex = bandCount - 1;
                }
                break;
            case KEY_DOWN:
                if (bandListSelectedIndex < bandCount - 1) {
                    bandListSelectedIndex++;
                    if (bandListSelectedIndex >= bandListScrollOffset + MAX_VISIBLE_LINES)
                        bandListScrollOffset = bandListSelectedIndex - MAX_VISIBLE_LINES + 1;
                } else {
                    bandListSelectedIndex = 0;
                }
                break;
            case KEY_4: /* toggle selected band */
                if (bandListSelectedIndex < bandCount) {
                    settings.bandEnabled[bandListSelectedIndex] = !settings.bandEnabled[bandListSelectedIndex]; 
                    nextBandToScanIndex = bandListSelectedIndex; 
                    bandListSelectedIndex++;
                }
                break;
            case KEY_5: /* select only this band */
                if (bandListSelectedIndex < bandCount) {
                    memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
                    settings.bandEnabled[bandListSelectedIndex] = true;
                    nextBandToScanIndex = bandListSelectedIndex; 
                }
                break;
            case KEY_MENU:
                if (bandListSelectedIndex < bandCount) {
                    memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
                    settings.bandEnabled[bandListSelectedIndex] = true;
                    nextBandToScanIndex = bandListSelectedIndex;
                    gForceModulation = 0; // KOLYAN ADD
                    SetState(SPECTRUM);
                    RelaunchScan();
                }
                break;
        case KEY_EXIT:
                SpectrumMonitor = 0;
                SetState(SPECTRUM);
                RelaunchScan(); 
                gForceModulation = 0; // KOLYAN ADD
                break;
            default:
                break;
        }
    }

/* --- SCANLIST_SELECT: navigate and toggle scan-list enable flags --- */
static void HandleKeyScanList(uint8_t key) {
        switch (key) {
        case KEY_UP:
                if (scanListSelectedIndex > 0) {
                    scanListSelectedIndex--;
                if (scanListSelectedIndex < scanListScrollOffset)
                        scanListScrollOffset = scanListSelectedIndex;
                } else {
                scanListSelectedIndex = validScanListCount - 1;
                    }
                break;
            case KEY_DOWN:
                if (scanListSelectedIndex < validScanListCount - 1) {
                    scanListSelectedIndex++;
                if (scanListSelectedIndex >= scanListScrollOffset + MAX_VISIBLE_LINES)
                        scanListScrollOffset = scanListSelectedIndex - MAX_VISIBLE_LINES + 1;
                } else {
                scanListSelectedIndex = 0;
                }    
                break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
            case KEY_STAR: /* drill-down into channel list for selected scanlist */
                selectedScanListIndex = scanListSelectedIndex;
                BuildScanListChannels(validScanListIndices[selectedScanListIndex]);
                scanListChannelsSelectedIndex = 0;
                scanListChannelsScrollOffset  = 0;
                SetState(SCANLIST_CHANNELS);
                break;	
#endif
        case KEY_4: /* toggle selected list, advance cursor */
            ToggleScanList(validScanListIndices[scanListSelectedIndex], 0);
            if (scanListSelectedIndex < validScanListCount - 1)
                scanListSelectedIndex++;
            break;
        case KEY_5: /* activate only selected list */
            ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
            break;
        case KEY_MENU: /* activate selected list and start scanning */
            if (scanListSelectedIndex < MR_CHANNELS_LIST) {
                ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
                SetState(SPECTRUM);
                ResetModifiers();
                gForceModulation = 0; //1 kolyan
            }
            break;
        case KEY_EXIT:
            SpectrumMonitor = 0;
            SetState(SPECTRUM);
            ResetModifiers();
            gForceModulation = 0; //1 kolyan
            break;
        default:
            break;
    }
}
      	  
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
/* --- SCANLIST_CHANNELS: scroll through channel detail list --- */
static void HandleKeyScanListChannels(uint8_t key) {
    switch (key) {
        case KEY_UP:
        if (scanListChannelsSelectedIndex > 0) {
            scanListChannelsSelectedIndex--;
                if (scanListChannelsSelectedIndex < scanListChannelsScrollOffset)
                scanListChannelsScrollOffset = scanListChannelsSelectedIndex;
        }
        break;
    case KEY_DOWN:
        if (scanListChannelsSelectedIndex < scanListChannelsCount - 1) {
            scanListChannelsSelectedIndex++;
                if (scanListChannelsSelectedIndex >= scanListChannelsScrollOffset + 3)
                scanListChannelsScrollOffset = scanListChannelsSelectedIndex - 3 + 1;
        }
        break;
        case KEY_EXIT:
        SetState(SCANLIST_SELECT);
        break;
    default:
        break;
    }
}
#endif /* ENABLE_SCANLIST_SHOW_DETAIL */

/* --- PARAMETERS_SELECT: navigate settings, edit values --- */
static void HandleKeyParameters(uint8_t key) {
      switch (key) {
          case KEY_UP:
                if (parametersSelectedIndex > 0) {
                    parametersSelectedIndex--;
                if (parametersSelectedIndex < parametersScrollOffset)
                        parametersScrollOffset = parametersSelectedIndex;
                } else {
                parametersSelectedIndex = PARAMETER_COUNT - 1;
                }
                break;
          case KEY_DOWN:
                if (parametersSelectedIndex < PARAMETER_COUNT - 1) { 
                    parametersSelectedIndex++;
                if (parametersSelectedIndex >= parametersScrollOffset + MAX_VISIBLE_LINES)
                        parametersScrollOffset = parametersSelectedIndex - MAX_VISIBLE_LINES + 1;
            } else {
                parametersSelectedIndex = 0;
            }
            break;
          case KEY_1:
          case KEY_3: {
              bool isKey3 = (key == KEY_3);
              switch (parametersSelectedIndex) {
                case 0: /* RSSI Delay */
                    DelayRssi = isKey3 ?
                                 (DelayRssi >= 6 ? 1 : DelayRssi + 1) :
                                 (DelayRssi <= 1 ? 6 : DelayRssi - 1);
                    {
                        static const int rssiMap[] = {1, 5, 10, 15, 20};
                        settings.rssiTriggerLevelUp =
                            (DelayRssi >= 1 && DelayRssi <= 5) ? rssiMap[DelayRssi - 1] : 20;
                    }
                      break;
                case 1: /* Spectrum Delay */
                    if (isKey3) {
                          if (SpectrumDelay < 61000)
                              SpectrumDelay += (SpectrumDelay < 10000) ? 1000 : 5000;
                      } else if (SpectrumDelay >= 1000) {
                          SpectrumDelay -= (SpectrumDelay < 10000) ? 1000 : 5000;
                      }
                      break;
                case 2: /* Max listen time */
                    if (isKey3) {
                          if (++IndexMaxLT > LISTEN_STEP_COUNT) IndexMaxLT = 0;
                      } else {
                          if (IndexMaxLT == 0) IndexMaxLT = LISTEN_STEP_COUNT;
                          else IndexMaxLT--;
                      }
                      MaxListenTime = listenSteps[IndexMaxLT];
                      break;
                case 3: /* Scan range start */
                case 4: /* Scan range stop  */
                          appMode = SCAN_RANGE_MODE;
                          FreqInput();
                      break;
                case 5: /* Scan step */
                    UpdateScanStep(isKey3);
                      break;
                case 6: /* Listen BW */
                case 7: /* Modulation */
                    if (isKey3 || key == KEY_1) {
                        if (parametersSelectedIndex == 6)
                              ToggleListeningBW(isKey3 ? 0 : 1);
                        else
                              ToggleModulation();
                      }
                      break;
                case 8: /* RX Backlight */
                    Backlight_On_Rx = !Backlight_On_Rx;
                      break;
                case 9: /* Power Save */
                        if (isKey3) {
                        if (++IndexPS > PS_STEP_COUNT) IndexPS = 0;
                        } else {
                          if (IndexPS == 0) IndexPS = PS_STEP_COUNT;
                          else IndexPS--;
                        }
                        SpectrumSleepMs = PS_Steps[IndexPS];
                      break;
                case 10: /* Noise level OFF */
                      Noislvl_OFF = isKey3 ? 
                                  (Noislvl_OFF >= 80 ? 30  : Noislvl_OFF + 1) :
                                  (Noislvl_OFF <= 30  ? 80 : Noislvl_OFF - 1);
                      Noislvl_ON = NoisLvl - NoiseHysteresis;                      
                      break;
                case 11: /* OSD popup duration */
                      osdPopupSetting = isKey3 ? 
                                      (osdPopupSetting >= 5000 ? 0    : osdPopupSetting + 500) :
                                      (osdPopupSetting <= 0    ? 5000 : osdPopupSetting - 500);
                      break;
                case 12: /* Record trigger */
                      UOO_trigger = isKey3 ? 
                                  (UOO_trigger >= 50 ? 0  : UOO_trigger + 1) :
                                  (UOO_trigger <= 0  ? 50 : UOO_trigger - 1);
                      break;
                case 13: /* Auto keylock */
                      AUTO_KEYLOCK = isKey3 ? 
                                   (AUTO_KEYLOCK > 2  ? 0 : AUTO_KEYLOCK + 1) :
                                 (AUTO_KEYLOCK <= 0 ? 3 : AUTO_KEYLOCK - 1);
                      gKeylockCountdown = durations[AUTO_KEYLOCK];
                      break;
                case 14: /* Glitch max */
                    if (isKey3) { if (GlitchMax < 75) GlitchMax += 5; }
                    else        { if (GlitchMax > 5) GlitchMax -= 5; }
                      break;
                case 15: /* Sound boost */
                      SoundBoost = !SoundBoost;
                      break;
                case 16: // PttEmission
                      PttEmission = isKey3 ?
                            (PttEmission >= 2 ? 0 : PttEmission + 1) :
                            (PttEmission <= 0 ? 2 : PttEmission - 1);
                      break;  
                case 17: /* gMonitorScan */
                    gMonitorScan = !gMonitorScan; 
                    break;
                case 18: /* Clear history */
                        if (isKey3) ClearHistory(0);
                        break;
                case 19: /* Clear history */
                        if (isKey3) ClearHistory(1);
                        break;
                case 20: /* Clear history */
                        if (isKey3) ClearHistory(2);
                        break;
                case 21: /* Reset to defaults */
                      if (isKey3) ClearSettings();
                      break;

              }
        break;
        }
        case KEY_7:
          SaveSettings(); 
        break;
        case KEY_EXIT:
            SetState(SPECTRUM);
            RelaunchScan();
            ResetModifiers();
            if(Key_1_pressed) {Spectrum_state = 2;APP_RunSpectrum();}
            break;
        default:
            break;
      }
}

/* --- SPECTRUM state: main spectrum view keys, including list entry shortcuts --- */
static void HandleKeySpectrum(uint8_t key) {

    /* Shortcuts that open list sub-states from SPECTRUM */
    if (appMode == SCAN_BAND_MODE && key == KEY_4) {
        SetState(BAND_LIST_SELECT);
        bandListSelectedIndex = 0;
        bandListScrollOffset  = 0;
        return;
    }
    if (appMode == CHANNEL_MODE && key == KEY_4) {
        SetState(SCANLIST_SELECT);
        scanListSelectedIndex = 0;
        scanListScrollOffset  = 0;
        return;
    }
    if (key == KEY_5) {
        if (historyListActive) {
            gHistoryScan = !gHistoryScan;
            ShowOSDPopup(gHistoryScan ? "SCAN HISTORY ON" : "SCAN HISTORY OFF");
            if (gHistoryScan) { gIsPeak = false; SpectrumMonitor = 0; }
        } else {
            SetState(PARAMETERS_SELECT);
            if (!parametersStateInitialized) {
                parametersSelectedIndex = 0;
                parametersScrollOffset = 0;
                parametersStateInitialized = true;
            }
            return;
            }
        }

    switch (key) {
        case KEY_STAR: {
            if (kbd.counter > 22) {settings.rssiTriggerLevelUp = 50;}
            else {
                int step = (settings.rssiTriggerLevelUp >= 20) ? 5 : 1;
                settings.rssiTriggerLevelUp =
                    (settings.rssiTriggerLevelUp >= 50 ? 0 : settings.rssiTriggerLevelUp + step);
            }
            SPECTRUM_PAUSED = true;
            SetTrigger50();
            break;
        }
        case KEY_F: {
            if (kbd.counter > 22) {settings.rssiTriggerLevelUp = 50;}
            else {
            int step = (settings.rssiTriggerLevelUp <= 20) ? 1 : 5;
            settings.rssiTriggerLevelUp =
                (settings.rssiTriggerLevelUp <= 0 ? 50 : settings.rssiTriggerLevelUp - step);
            }
            SPECTRUM_PAUSED = true;
            SetTrigger50();
            break;
        }
        case KEY_3:
            if (historyListActive) {
                DeleteHistoryItem();
            } else {
                ToggleListeningBW(1);
                char bwText[32];
                sprintf(bwText, "BW: %s", bwNames[settings.listenBw]);
                ShowOSDPopup(bwText);
            }
            break;
        case KEY_9: {
            ToggleModulation();
            char modText[32];
            sprintf(modText, "MOD: %s", gModulationStr[settings.modulationType]);
            ShowOSDPopup(modText);
            break;
        }
        case KEY_1:
            Skip();
            ShowOSDPopup("SKIPPED");
            break;
        case KEY_7:
            if (historyListActive) {
                WriteHistory();
            } else {
                SaveSettings();
            }
            break;
        case KEY_2:
            if (historyListActive) SaveHistoryToFreeChannel();
            // simplified screen toggle removed
            break;
        case KEY_8:
            if (historyListActive) {
                memset(HFreqs,       0, HISTORY_SIZE * sizeof(uint32_t));
                memset(HBlacklisted, 0, HISTORY_SIZE * sizeof(bool));
                historyListIndex    = 0;
                historyScrollOffset = 0;
                indexFs             = 0;
                SpectrumMonitor     = 0;
            } else if (classic) {
                ShowLines++;
#ifdef ENABLE_BENCH
                if (ShowLines > 4 || ShowLines < 1) ShowLines = 1;
#else
                if (ShowLines > 2 || ShowLines < 1) ShowLines = 1;  // 3=LAST RX removed
#endif
                char viewText[15];
                const char *viewName = "CLASSIC";
                if (ShowLines == 2) viewName = "BIG";
#ifdef ENABLE_BENCH
                else if (ShowLines == 4) viewName = "BENCH";
#endif
                sprintf(viewText, "VIEW: %s", viewName);
                ShowOSDPopup(viewText);
            }
            #ifdef ENABLE_CPU_STATS
            else {
                /* Alternative (non-classic) view: KEY_8 opens RAM diagnostics. */
                SetState(RAM_VIEW);
            }
            #endif
    break;
        case KEY_UP:
            if (historyListActive) {
                uint16_t count = CountValidHistoryItems();
                SpectrumMonitor = 1;
                if (!count) return;
                if (historyListIndex == 0) {
                    historyListIndex    = count - 1;
                    historyScrollOffset = (count > MAX_VISIBLE_LINES) ? count - MAX_VISIBLE_LINES : 0;
                } else {
                    historyListIndex--;
                }
                if (historyListIndex < historyScrollOffset)
                    historyScrollOffset = historyListIndex;
                lastReceivingFreq = HFreqs[historyListIndex];
                SetF(lastReceivingFreq);
            } else if (appMode == SCAN_BAND_MODE) {
                // Вычисляем anyEnabled один раз
                bool anyEnabled = false;
                for (uint8_t _i = 0; _i < bandCount; _i++) if (settings.bandEnabled[_i]) { anyEnabled = true; break; }
                if (anyEnabled) {
                    // Переходим только по включённым бендам
                    uint8_t tries = bandCount;
                    do {
                        bl = (bl == 0) ? bandCount - 1 : bl - 1;
                        tries--;
                    } while (tries > 0 && !settings.bandEnabled[bl]);
                } else {
                    // Все бенды — просто идём назад
                    bl = (bl == 0) ? bandCount - 1 : bl - 1;
                }
                // Синхронизируем nextBandToScanIndex чтобы InitScan стартовал с bl
                nextBandToScanIndex = bl;
                RelaunchScan();
            } else if (appMode == FREQUENCY_MODE) {
                UpdateCurrentFreq(true);
            } else if (appMode == CHANNEL_MODE) {
                BuildValidScanListIndices();
                if (validScanListCount > 0) {
                    if (cachedEnabledScanListCount == 0) {
                        scanListNavIndex = (scanListNavIndex == 0) ? validScanListCount - 1 : scanListNavIndex - 1;
                    } else {
                        uint8_t tries = validScanListCount;
                        do {
                            scanListNavIndex = (scanListNavIndex == 0) ? validScanListCount - 1 : scanListNavIndex - 1;
                            tries--;
                        } while (tries > 0 && !settings.scanListEnabled[validScanListIndices[scanListNavIndex]]);
                    }
                    scanListCountsDirty = true;  // force redraw SL:
                }
            } else if (appMode == SCAN_RANGE_MODE) {
                uint32_t rstep = gScanRangeStop - gScanRangeStart;
                gScanRangeStop  -= rstep;
                gScanRangeStart -= rstep;
                RelaunchScan();
            } else {
                Skip();
            }
            break;
  case KEY_DOWN:
      if (historyListActive) {
        uint16_t count = CountValidHistoryItems();
        SpectrumMonitor = 1;
        if (!count) return;
        if (++historyListIndex >= count) {
            historyListIndex    = 0;
            historyScrollOffset = 0;
        }
        if (historyListIndex >= historyScrollOffset + MAX_VISIBLE_LINES)
            historyScrollOffset = historyListIndex - MAX_VISIBLE_LINES + 1;
        lastReceivingFreq = HFreqs[historyListIndex];
        SetF(lastReceivingFreq);
            } else if (appMode == SCAN_BAND_MODE) {
                bool anyEnabled = false;
                for (uint8_t _i = 0; _i < bandCount; _i++) if (settings.bandEnabled[_i]) { anyEnabled = true; break; }
                if (anyEnabled) {
                    uint8_t tries = bandCount;
                    do {
                        bl = (bl + 1) % bandCount;
                        tries--;
                    } while (tries > 0 && !settings.bandEnabled[bl]);
                } else {
                    bl = (bl + 1) % bandCount;
                }
                // Синхронизируем nextBandToScanIndex чтобы InitScan стартовал с bl
                nextBandToScanIndex = bl;
                RelaunchScan(); 
        } else if (appMode == FREQUENCY_MODE) {UpdateCurrentFreq(false);}
        else if (appMode == CHANNEL_MODE) {
            BuildValidScanListIndices();
            if (validScanListCount > 0) {
                if (cachedEnabledScanListCount == 0) {
                    scanListNavIndex = (scanListNavIndex + 1) % validScanListCount;
                } else {
                    uint8_t tries = validScanListCount;
                    do {
                        scanListNavIndex = (scanListNavIndex + 1) % validScanListCount;
                        tries--;
                    } while (tries > 0 && !settings.scanListEnabled[validScanListIndices[scanListNavIndex]]);
                }
                scanListCountsDirty = true;  // force redraw SL:
            }
        } else if (appMode == SCAN_RANGE_MODE) {
                uint32_t rstep = gScanRangeStop - gScanRangeStart;
                gScanRangeStop  += rstep;
                gScanRangeStart += rstep;
            RelaunchScan();
            } else {
        Skip();
    }
  break;
  case KEY_4:
            if (appMode != SCAN_RANGE_MODE) ToggleStepsCount();
    break;
        case KEY_0:
            if (kbd.counter > 22) {
                if (!gHistorySortLongPressDone) {
                    CompactHistory();
                    SortHistoryByFrequencyAscending();

                if (!historyListActive) {
                        historyListActive   = true;
                        prevSpectrumMonitor = SpectrumMonitor;
                    }

                historyListIndex = 0;
                historyScrollOffset = 0;
                gHistorySortLongPressDone = true;
                }
            } else if (!historyListActive) {
                CompactHistory();
                historyListActive   = true;
                historyListIndex    = 0;
        historyScrollOffset = 0;
        prevSpectrumMonitor = SpectrumMonitor;
        }
    break;
  
     case KEY_6: // next mode
        NextAppMode();
        break;
    case KEY_SIDE1:
        if (SPECTRUM_PAUSED) return;
        if (++SpectrumMonitor > 2) SpectrumMonitor = 0;
        if (SpectrumMonitor == 1) {
            if (lastReceivingFreq < 1400000 || lastReceivingFreq > 130000000) {
                lastReceivingFreq = (scanInfo.f >= 1400000) ? scanInfo.f : gScanRangeStart;}
                peak.f     = lastReceivingFreq;
                scanInfo.f = lastReceivingFreq;
                SetF(lastReceivingFreq);
        }
    if (SpectrumMonitor == 2) ToggleRX(1);
    {
		char monitorText[32];
        const char *modes[] = {"NORMAL", "FREQ LOCK", "MONITOR"};
        sprintf(monitorText, "MODE: %s", modes[SpectrumMonitor]);
	    ShowOSDPopup(monitorText);
    }
    break;
    case KEY_SIDE2:
    if (historyListActive) {
        HBlacklisted[historyListIndex] = !HBlacklisted[historyListIndex];
        ShowOSDPopup(HBlacklisted[historyListIndex] ? "BL ADDED" : "BL REMOVED");
        RenderHistoryList();
        gIsPeak = 0;
        ToggleRX(false);
        ResetScanStats();
        NextScanStep();
        } else {
            Blacklist();
            WaitSpectrum = 0;
            ShowOSDPopup("BL ADD");
            }
        break;
    case KEY_PTT:
        ExitAndCopyToVfo();
        break;
        case KEY_MENU:
            if (historyListActive) scanInfo.f = HFreqs[historyListIndex];
            SpectrumMonitor = 1;
            SetState(STILL);      
            stillFreq = GetInitialStillFreq();
            if (stillFreq >= 1400000 && stillFreq <= 130000000) {
                scanInfo.f = stillFreq;
                peak.f     = stillFreq;
                SetF(stillFreq);
            }
            break;
    case KEY_EXIT:
        if (historyListActive) {
            gHistoryScan        = false;
            SetState(SPECTRUM);
            historyListActive   = false;
            SpectrumMonitor     = prevSpectrumMonitor;
            SetF(scanInfo.f);
            break;
        }
        if (WaitSpectrum) WaitSpectrum = 0;
        DeInitSpectrum(0);
    break;
   default:
      break;
  }
}

// ============================================================
// SECTION: Main keyboard dispatcher
// ============================================================

static void OnKeyDown(uint8_t key) {
    if (!backlightOn) {BACKLIGHT_TurnOn();return;}

    /* Key-lock guard: only KEY_F unlocks */
    if (gIsKeylocked) {
        if (key == KEY_F) {
            gIsKeylocked = false;
            ShowOSDPopup("Unlocked");
            gKeylockCountdown = durations[AUTO_KEYLOCK];
        } else {
            ShowOSDPopup("Unlock:F");
        }
        return;
    }
    gKeylockCountdown = durations[AUTO_KEYLOCK];

    /* Dispatch to the handler for the currently active state */
    switch (currentState) {
        case BAND_LIST_SELECT:  HandleKeyBandList(key);         break;
        case SCANLIST_SELECT:   HandleKeyScanList(key);         break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
        case SCANLIST_CHANNELS: HandleKeyScanListChannels(key); break;
#endif
        case PARAMETERS_SELECT: HandleKeyParameters(key);       break;
        default:                HandleKeySpectrum(key);         break;
    }
}

static void OnKeyDownFreqInput(uint8_t key) {
  BACKLIGHT_TurnOn();
  switch (key) {
  case KEY_0:
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
  case KEY_STAR:
    UpdateFreqInput(key);
    break;
  case KEY_EXIT: //EXIT from freq input
    if (freqInputIndex == 0) {
      SetState(previousState);
      WaitSpectrum = 0;
      break;
    }
    UpdateFreqInput(key);
    break;
  case KEY_MENU: //OnKeyDownFreqInput
    if (tempFreq > F_MAX) {
      break;
    }
    SetState(previousState);
    if (currentState == SPECTRUM) {
        currentFreq = tempFreq;
      ResetModifiers();
    }
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 3)
        gScanRangeStart = tempFreq;
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 4)
        gScanRangeStop = tempFreq;

    break;
  default:
    break;
  }
}

static int16_t storedScanStepIndex = -1;

static void OnKeyDownStill(KEY_Code_t key) {
  BACKLIGHT_TurnOn();
  switch (key) {
      case KEY_3:
         ToggleListeningBW(1);
      break;
     
      case KEY_9:
        ToggleModulation();
      break;
      case KEY_DOWN:
          if (stillEditRegs) {
            SetRegMenuValue(stillRegSelected, true);
          } else if (SpectrumMonitor > 0) {
                uint32_t step = GetScanStep();
                stillFreq += step;
                scanInfo.f = stillFreq;
                peak.f = stillFreq;
                SetF(stillFreq);
            }
        break;
      case KEY_UP:
          if (stillEditRegs) {
            SetRegMenuValue(stillRegSelected, false);
          } else if (SpectrumMonitor > 0) {
                uint32_t step = GetScanStep();
                if (stillFreq > step) stillFreq -= step;
                scanInfo.f = stillFreq;
                peak.f = stillFreq;
                SetF(stillFreq);
            }
          break;
      case KEY_2: // przewijanie w górę po liście rejestrów
          if (stillEditRegs && stillRegSelected > 0) {
            stillRegSelected--;
          }
      break;
      case KEY_8: // przewijanie w dół po liście rejestrów
          if (stillEditRegs && stillRegSelected < ARRAY_SIZE(allRegisterSpecs)-1) {
            stillRegSelected++;
          }
      break;
      case KEY_STAR:
            if (storedScanStepIndex == -1) {
                storedScanStepIndex = settings.scanStepIndex;
            }
            UpdateScanStep(1);
      break;
      case KEY_F:
            if (storedScanStepIndex == -1) {
                storedScanStepIndex = settings.scanStepIndex;
            }
            UpdateScanStep(0);
      break;
      case KEY_5:
      case KEY_0:
      case KEY_6:
      case KEY_7:
      break;
          
      case KEY_SIDE1: 
          SpectrumMonitor++;
    if (SpectrumMonitor > 2) SpectrumMonitor = 0;

    if (SpectrumMonitor == 1) {
        if (lastReceivingFreq < 1400000 || lastReceivingFreq > 130000000) {
            lastReceivingFreq = (stillFreq >= 1400000) ? stillFreq : scanInfo.f;
        }
        peak.f = lastReceivingFreq;
        scanInfo.f = lastReceivingFreq;
        SetF(lastReceivingFreq);
    }

    if (SpectrumMonitor == 2) ToggleRX(1);
      break;

      case KEY_SIDE2: 
            Blacklist();
            WaitSpectrum = 0; //don't wait if this frequency not interesting
      break;
      case KEY_PTT:
        ExitAndCopyToVfo();
        break;
      case KEY_MENU:
          stillEditRegs = !stillEditRegs;
      break;
      case KEY_EXIT: //EXIT from regs
        if (stillEditRegs) {
          stillEditRegs = false;
        break;
        }
        if (storedScanStepIndex != -1) {
            settings.scanStepIndex = storedScanStepIndex;
            storedScanStepIndex = -1;
            scanInfo.scanStep = storedScanStepIndex; 
        }
        SpectrumMonitor = 0;
        SetState(SPECTRUM);
        WaitSpectrum = 0; //Prevent coming back to still directly
        
    break;
  default:
    break;
  }
}


static void RenderFreqInput() {
  UI_PrintString(freqInputString, 2, 127, 0, 8);
}

static void RenderStatus() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  DrawStatus();
  ST7565_BlitStatusLine();
}
#ifdef ENABLE_SPECTRUM_LINES

static void MyDrawHLine(uint8_t y, bool white)
{
    if (y >= 64) return;
    uint8_t byte_idx = y / 8;
    uint8_t bit_mask = 1U << (y % 8);
    for (uint8_t x = 0; x < 128; x++) {
        if (white) {
            gFrameBuffer[byte_idx][x] &= ~bit_mask;  // белая
        } else {
            gFrameBuffer[byte_idx][x] |= bit_mask;   // чёрная
        }
    }
}

// Короткая горизонтальная пунктирная линия
static void MyDrawShortHLine(uint8_t y, uint8_t x_start, uint8_t x_end, uint8_t step, bool white)
{
    if (y >= 64 || x_start >= x_end || x_end > 127) return;
    uint8_t byte_idx = y / 8;
    uint8_t bit_mask = 1U << (y % 8);

    for (uint8_t x = x_start; x <= x_end; x++) {
        if (step > 1 && (x % step) != 0) continue;  // пунктир

        if (white) {
            gFrameBuffer[byte_idx][x] &= ~bit_mask;  // белая
        } else {
            gFrameBuffer[byte_idx][x] |= bit_mask;   // чёрная
        }
    }
}

static void MyDrawVLine(uint8_t x, uint8_t y_start, uint8_t y_end, uint8_t step)
{
    if (x >= 128) return;
    for (uint8_t y = y_start; y <= y_end && y < 64; y++) {
        if (step > 1 && (y % step) != 0) continue;  // пунктир
        uint8_t byte_idx = y / 8;
        uint8_t bit_mask = 1U << (y % 8);
        gFrameBuffer[byte_idx][x] |= bit_mask;  // чёрная (для белой сделай отдельно или параметр)
    }
}

static void MyDrawFrameLines(void)
{
    MyDrawHLine(50, true);   // белая горизонтальная на y=50
    MyDrawHLine(49, false);  // чёрная горизонтальная на y=49
    MyDrawVLine(0,   21, 49, 1);  // левая вертикальная сплошная
    MyDrawVLine(127, 21, 49, 1);  // правая вертикальная сплошная
    MyDrawVLine(0,   0, 17, 1);  // левая вертикальная сплошная
    MyDrawVLine(127, 0, 17, 1);  // правая вертикальная сплошная
    MyDrawShortHLine(0, 0, 3, 1, false);  // верх кор лев
    MyDrawShortHLine(0, 4, 8, 2, false);  // верх кор лев
    MyDrawShortHLine(0, 124, 127, 1, false);  // верх кор прав
    MyDrawShortHLine(0, 118, 123, 2, false);  // верх кор прав
    MyDrawShortHLine(17, 0, 10, 1, false);  // верх кор лев
    MyDrawShortHLine(21, 0, 10, 1, false);  // верх кор лев
    //MyDrawShortHLine(19, 11, 119, 2, false);  // центр длин
    MyDrawShortHLine(21, 120, 127, 1, false);  // кор прав
    MyDrawShortHLine(17, 120, 127, 1, false);  // кор прав
}
#endif

#ifdef ENABLE_BENCH
static void RenderBenchmark(void) {
    char line[32];
    // pełna czarna belka nagłówka (linia 0, y: 0..7)
    for (uint8_t x = 0; x < LCD_WIDTH; x++) {
        for (uint8_t y = 0; y < 8; y++) {
            PutPixel(x, y, true);
        }
    }
    // biały napis na czarnej belce
    UI_PrintStringSmallbackground("BENCHMARK", 1, LCD_WIDTH - 1, 0, 1);
    if (appMode == CHANNEL_MODE) {
        snprintf(line, sizeof(line), "Mode: CH  Steps:%u", scanChannelsCount);
    } else {
        snprintf(line, sizeof(line), "Mode: FR  Steps:%u", GetStepsCount());
    }
    UI_PrintStringSmallbackground(line, 1, LCD_WIDTH - 1, 1, 0);
    snprintf(line, sizeof(line), "Rate: %u /s", benchRatePerSec);
    UI_PrintStringSmallbackground(line, 1, LCD_WIDTH - 1, 2, 0);
    if (benchLastLapMs > 0) {
        snprintf(line, sizeof(line), "Full scan: %lu.%01lus",
                 benchLastLapMs / 1000, (benchLastLapMs % 1000) / 100);
    } else {
        snprintf(line, sizeof(line), "Full scan: ---");
    }
    UI_PrintStringSmallbackground(line, 1, LCD_WIDTH - 1, 3, 0);
    snprintf(line, sizeof(line), "Cur lap: %lu.%01lus",
             benchLapMs / 1000, (benchLapMs % 1000) / 100);
    UI_PrintStringSmallbackground(line, 1, LCD_WIDTH - 1, 4, 0);
    UI_PrintStringSmallbackground("8:VIEW 1:SKIP 5:PARAM", 1, LCD_WIDTH - 1, 6, 0);
}
#endif

static void RenderSpectrum()
{
#ifdef ENABLE_BENCH
	    if (ShowLines == 4) {
        RenderBenchmark();
        return;
    }
#endif
    if (classic) {
        DrawNums();
        UpdateDBMaxAuto();
        DrawSpectrum();

#ifdef ENABLE_SPECTRUM_LINES
MyDrawFrameLines();
#endif
        
  }

    if(isListening) { DrawF(peak.f);}
    else {
      if (SpectrumMonitor) DrawF(lastReceivingFreq);
      else DrawF(scanInfo.f);
    }
}

static void DrawMeter(int line) {
    const uint8_t METER_PAD_LEFT = 7;
    const uint8_t NUM_SQUARES    = 23;          // чуть короче, чтобы точно влез
    const uint8_t SQUARE_SIZE    = 4;
    const uint8_t SQUARE_GAP     = 1;
    const uint8_t Y_START_BIT    = 2;
    uint8_t max_width_px = NUM_SQUARES * (SQUARE_SIZE + SQUARE_GAP) - SQUARE_GAP;
    uint8_t fill_px      = Rssi2PX(scanInfo.rssi, 0, max_width_px);
    uint8_t fill_count   = fill_px / (SQUARE_SIZE + SQUARE_GAP);
    settings.dbMax = -60; 
    settings.dbMin = -120;
    // Очистка строки
    for (uint8_t px = 0; px < 128; px++) {
        gFrameBuffer[line][px] = 0;
    }

    // Рисуем все квадратики с обводкой
    for (uint8_t sq = 0; sq < NUM_SQUARES; sq++) {
        uint8_t x_left  = METER_PAD_LEFT + sq * (SQUARE_SIZE + SQUARE_GAP);
        uint8_t x_right = x_left + SQUARE_SIZE - 1;

        if (x_right >= 128) break;

        // Верх и низ
        for (uint8_t x = x_left; x <= x_right; x++) {
            gFrameBuffer[line][x] |= (1 << Y_START_BIT);
            gFrameBuffer[line][x] |= (1 << (Y_START_BIT + SQUARE_SIZE - 1));
        }

        // Лево и право
        for (uint8_t bit = Y_START_BIT; bit < Y_START_BIT + SQUARE_SIZE; bit++) {
            gFrameBuffer[line][x_left]  |= (1 << bit);
            gFrameBuffer[line][x_right] |= (1 << bit);
        }
    }

    // Заполняем активные квадратики
    for (uint8_t sq = 0; sq < fill_count; sq++) {
        uint8_t x_left  = METER_PAD_LEFT + sq * (SQUARE_SIZE + SQUARE_GAP);
        uint8_t x_right = x_left + SQUARE_SIZE - 1;

        if (x_right >= 128) break;

        for (uint8_t x = x_left; x <= x_right; x++) {
            for (uint8_t bit = Y_START_BIT; bit < Y_START_BIT + SQUARE_SIZE; bit++) {
                gFrameBuffer[line][x] |= (1 << bit);
            }
        }
    }
}

static void RenderStill() {
  classic=1;
  char freqStr[18];
  //if (SpectrumMonitor) FormatFrequency(HFreqs[historyListIndex], freqStr, sizeof(freqStr));
  //else
  FormatFrequency(stillFreq, freqStr, sizeof(freqStr));
  UI_DisplayFrequency(freqStr, 0, 0, 0);
  DrawMeter(2);
  sLevelAttributes sLevelAtt;
  sLevelAtt = GetSLevelAttributes(scanInfo.rssi, stillFreq);

  if(sLevelAtt.over > 0)
    snprintf(String, sizeof(String), "S%2d+%2d", sLevelAtt.sLevel, sLevelAtt.over);
  else
    snprintf(String, sizeof(String), "S%2d", sLevelAtt.sLevel);

  GUI_DisplaySmallest(String, 4, 25, false, true);
  snprintf(String, sizeof(String), "%d dBm", sLevelAtt.dBmRssi);
  GUI_DisplaySmallest(String, 40, 25, false, true);
  uint8_t total = ARRAY_SIZE(allRegisterSpecs);
  uint8_t lines = STILL_REGS_MAX_LINES;
  if (total < lines) lines = total;
  if (stillRegSelected >= total) stillRegSelected = total-1;
  if (stillRegSelected < stillRegScroll) stillRegScroll = stillRegSelected;
  if (stillRegSelected >= stillRegScroll + lines) stillRegScroll = stillRegSelected - lines + 1;

  for (uint8_t i = 0; i < lines; ++i) {
    uint8_t idx = i + stillRegScroll;
    RegisterSpec s = allRegisterSpecs[idx];
    uint16_t v = GetRegMenuValue(idx);
    char buf[32];
    // Przygotuj tekst do wyświetlenia
    if (stillEditRegs && idx == stillRegSelected)
      snprintf(buf, sizeof(buf), ">%-18s %6u", s.name, v);
    else
      snprintf(buf, sizeof(buf), " %-18s %6u", s.name, v);
    uint8_t y = 32 + i * 8;
    if (stillEditRegs && idx == stillRegSelected) {
      // Najpierw czarny prostokąt na wysokość linii
      for (uint8_t px = 0; px < 128; ++px)
        for (uint8_t py = y; py < y + 6; ++py) // 6 = wysokość fontu 3x5
          PutPixel(px, py, true); // 
      // Następnie białe litery (fill = true)
      GUI_DisplaySmallest(buf, 0, y, false, false);
    } else {
      // Zwykły tekst: czarne litery na białym
      GUI_DisplaySmallest(buf, 0, y, false, true);
    }
  }
}

static void Render() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  
  switch (currentState) {
  case SPECTRUM:
    if(historyListActive) RenderHistoryList();
    else RenderSpectrum();
    break;
  case FREQ_INPUT:
    RenderFreqInput();
    break;
  case STILL:
    RenderStill();
    break;
  
    case BAND_LIST_SELECT:
      RenderBandSelect();
    break;

    case SCANLIST_SELECT:
      RenderScanListSelect();
    break;
    case PARAMETERS_SELECT:
      RenderParametersSelect();
    break;
#ifdef ENABLE_CPU_STATS
    case RAM_VIEW:
      RenderRAMView();
    break;
    case MEM_BUFFERS:
      RenderMemBuffers();
    break;
    case MEM_VIEWER:
      RenderMemViewer();
    break;
    case CPU_VIEW:
      RenderCPUView();
    break;
#endif

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
    case SCANLIST_CHANNELS: // NOWY CASE
      RenderScanListChannels();
      break;
#endif // ENABLE_SCANLIST_SHOW_DETAIL
    
  }
  #ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
    SCREENSHOT_Update(1);
  #endif
  ST7565_BlitFullScreen();
}

static void HandleUserInput(void) {
    kbd.prev = kbd.current;
    kbd.current = GetKey();
    // ---- Anti-rebond + répétition ----
    if (kbd.current != KEY_INVALID && kbd.current == kbd.prev) {
        kbd.counter++;
    } else {
        if (kbd.prev == KEY_0) {
            gHistorySortLongPressDone = false;
        }
          kbd.counter = 0;
      }

if (kbd.counter == 2 || (kbd.counter > 17 && (kbd.counter % 15 == 0))) {
       
        switch (currentState) {
            case SPECTRUM:
                OnKeyDown(kbd.current);
                break;
            case FREQ_INPUT:
                OnKeyDownFreqInput(kbd.current);
                break;
            case STILL:
                OnKeyDownStill(kbd.current);
                break;
            case BAND_LIST_SELECT:
                OnKeyDown(kbd.current);
                break;
            case SCANLIST_SELECT:
                OnKeyDown(kbd.current);
                break;
            case PARAMETERS_SELECT:
                OnKeyDown(kbd.current);
                break;
#ifdef ENABLE_CPU_STATS
            case RAM_VIEW:
                OnKeyDownRAMView(kbd.current);
                break;
            case MEM_BUFFERS:
                OnKeyDownMemBuffers(kbd.current);
                break;
            case MEM_VIEWER:
                OnKeyDownMemViewer(kbd.current);
                break;
            case CPU_VIEW:
                OnKeyDownCPUView(kbd.current);
                break;
#endif
        #ifdef ENABLE_SCANLIST_SHOW_DETAIL
            case SCANLIST_CHANNELS:
                OnKeyDown(kbd.current);
                break;
        #endif
        }
    }
}

static void NextHistoryScanStep() {
    uint16_t count = CountValidHistoryItems();
    if (count == 0) return;
    uint16_t start = historyListIndex;
    do {
        historyListIndex++;
        if (historyListIndex >= count) historyListIndex = 0;
        if (historyListIndex == start && HBlacklisted[historyListIndex]) return;
    } while (HBlacklisted[historyListIndex]);

    if (historyListIndex < historyScrollOffset) {
        historyScrollOffset = historyListIndex;
    } else if (historyListIndex >= historyScrollOffset + MAX_VISIBLE_LINES) {
        historyScrollOffset = historyListIndex - MAX_VISIBLE_LINES + 1;
    }
    scanInfo.f = HFreqs[historyListIndex];
    spectrumElapsedCount = 0;
}

static uint32_t savedScanF;

static void UpdateScan() {
    if (SPECTRUM_PAUSED || gIsPeak || SpectrumMonitor || WaitSpectrum) return;
    SetF(scanInfo.f);
    Measure();
    if (gIsPeak || SpectrumMonitor || WaitSpectrum) return;
#ifdef ENABLE_BENCH
    benchStepsThisSec++;
#endif

    if (gMonitorScan && gNextTimeslice_Monitor && monitorChannelsCount) { 
        gNextTimeslice_Monitor = false;
        savedScanF = scanInfo.f; // Sauvegarde avant interruption
        MonitorIndex = monitorChannelsCount + 1;
    }
    if (MonitorIndex) {
        scanInfo.f = MonitorFreqs[--MonitorIndex-1];
        if (!MonitorIndex) {
            scanInfo.f = savedScanF;
            NextScanStep();
        }
        return;
    }
    if (gHistoryScan && historyListActive) NextHistoryScanStep();
    else NextScanStep();
#ifdef ENABLE_BENCH
    if (benchLapDone) { benchLastLapMs = benchLapMs; benchLapMs = 0; }
#endif
    if (SpectrumSleepMs) {
        BK4819_Sleep();
        BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);
        SPECTRUM_PAUSED = true;
        SpectrumPauseCount = SpectrumSleepMs;
    }
}


static void UpdateListening(void) { // called every 10ms
    static uint32_t stableFreq = 1;
    static uint16_t stableCount = 0;
    static bool SoundBoostsave = false; // Initialisation
    
    scanInfo.rssi = GetRssi();
    
    if (SoundBoost != SoundBoostsave) {
        if (SoundBoost) {
            BK4819_WriteRegister(0x54, 0x90D1);    // default is 0x9009
            BK4819_WriteRegister(0x55, 0x3271);    // default is 0x31a9
            BK4819_WriteRegister(0x75, 0xFC13);    // default is 0xF50B
        } 
        else {
            BK4819_WriteRegister(0x54, 0x9009);
            BK4819_WriteRegister(0x55, 0x31a9);
            BK4819_WriteRegister(0x75, 0xF50B);
        }
        SoundBoostsave = SoundBoost;
    }
    if (peak.f == stableFreq) {
        if (++stableCount >= 2) {  // ~600ms
            if (!SpectrumMonitor) FillfreqHistory(true);
            stableCount = 0;
            if (gEeprom.BACKLIGHT_MAX > 5)
                BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 1);
            if(Backlight_On_Rx) BACKLIGHT_TurnOn();
        }
    } else {
        stableFreq = peak.f;
        stableCount = 0;
    }
    
    UpdateNoiseOff();
    if (!isListening) {
        UpdateNoiseOn();
        UpdateGlitch();
    }
        
    spectrumElapsedCount += 10; //in ms
    uint32_t maxCount = (uint32_t)MaxListenTime * 1000;

    if (MaxListenTime && spectrumElapsedCount >= maxCount && !SpectrumMonitor) {
        Skip();
        return;
    }

    // --- Gestion du pic ---
    if (gIsPeak) {
        WaitSpectrum = SpectrumDelay;   // reset timer
        return;
    }

    if (WaitSpectrum > 61000)
        return;

    if (WaitSpectrum > 10) {
        WaitSpectrum -= 10;
        return;
    }
    // timer écoulé
    WaitSpectrum = 0;
    ResetScanStats();

}

static void Tick() {
    if (gNextTimeslice_500ms) {
        if (gBacklightCountdown_500ms > 0) {
            if (--gBacklightCountdown_500ms == 0)	BACKLIGHT_TurnOff();}
            gNextTimeslice_500ms = false;
        if (gKeylockCountdown > 0) {gKeylockCountdown--;}
        if (AUTO_KEYLOCK && !gKeylockCountdown) {
            if (!gIsKeylocked) ShowOSDPopup("Locked"); 
            gIsKeylocked = true;
	    }
    }

    if (gNextTimeslice_10ms) {
        gNextTimeslice_10ms = 0;
        HandleUserInput();
#ifdef ENABLE_BENCH
        if (!isListening && !SPECTRUM_PAUSED && !SpectrumMonitor && !WaitSpectrum) {
            benchTickMs += 10;
            benchLapMs  += 10;
            if (benchTickMs >= 1000) {
                benchTickMs -= 1000;
                benchRatePerSec = benchStepsThisSec;
                benchStepsThisSec = 0;
            }
        }
#endif
        if(SpectrumPauseCount) SpectrumPauseCount--;
        if (osdPopupTimer > 0) {
            UI_DisplayPopup(osdPopupText);
            ST7565_BlitFullScreen();
            osdPopupTimer -= 10; 
            if (osdPopupTimer <= 0) {osdPopupText[0] = '\0';}
            return;
            }

    if (gNextTimeslice_listening) {
        gNextTimeslice_listening = 0;
        if (isListening || SpectrumMonitor || WaitSpectrum) UpdateListening(); 
    }
  }

  if (SPECTRUM_PAUSED && (SpectrumPauseCount == 0)) {
      // fin de la pause
      SPECTRUM_PAUSED = false;
      BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
      BK4819_RX_TurnOn(); //Wake up
      SYSTEM_DelayMs(10);
  }

  if(!isListening && gIsPeak && !SpectrumMonitor && !SPECTRUM_PAUSED) {
     SetF(peak.f);
     ToggleRX(true);
     return;
  }

  if (newScanStart) {
    newScanStart = false;
    InitScan();
  }

  if (!isListening) {UpdateScan();}
  
  if (gNextTimeslice_display) {
#ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
    SCREENSHOT_Update(true);
#endif
    //if (isListening || SpectrumMonitor || WaitSpectrum) UpdateListening(); // Kolyan test
    gNextTimeslice_display = 0;
    RenderStatus();
    Render();
  } 
}



void APP_RunSpectrumMode(uint8_t mode) {
    Spectrum_state = mode & 3;
    APP_RunSpectrum();
}

void APP_RunSpectrum(void) {
    // Выделяем историю один раз — так же как ScanFrequencies/BParams
    if (HFreqs == NULL)
        HFreqs = (uint32_t *)malloc(HISTORY_SIZE * sizeof(uint32_t));
    if (HBlacklisted == NULL)
        HBlacklisted = (bool *)malloc(HISTORY_SIZE * sizeof(bool));
    // При нехватке памяти обнуляем флаг чтобы LoadHistory не упал
    if (!HFreqs || !HBlacklisted) {
        historyLoaded = true; // пропустить LoadHistory
    }

    for (;;) {
        LoadMonitorFrequencies ();
        Mode mode;
        uint8_t requested_state = Spectrum_state;  // save before LoadSettings may overwrite it
        if (!Key_1_pressed ) LoadSettings();
        Spectrum_state = requested_state;           // restore: button choice has priority over EEPROM
        gComeBack = 0; 
        switch (Spectrum_state) {
            case 0:  mode = FREQUENCY_MODE;  break;
            case 1:  mode = CHANNEL_MODE;    break;
            case 2:  mode = SCAN_RANGE_MODE; break;
            case 3:  mode = SCAN_BAND_MODE;  break;
            default: mode = FREQUENCY_MODE;  break;
        }
        if(mode == CHANNEL_MODE) {
            if (ScanFrequencies == NULL) {
                ScanFrequencies = (uint32_t *)malloc((MR_CHANNEL_LAST + 1) * sizeof(uint32_t));}
            if (ScanFrequencies) LoadActiveScanFrequencies();
        }
#ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        gEeprom.CURRENT_STATE = 4;
        SETTINGS_WriteCurrentState();
#endif
        if (!Key_1_pressed) LoadSettings(); 
        // LoadActiveBands ПОСЛЕ LoadSettings — иначе settings.bandEnabled ещё не загружены из EEPROM
        if(mode == SCAN_BAND_MODE){
            if (BParams == NULL) {
                BParams = (bandparameters *)malloc((MAX_BANDS) * sizeof(bandparameters));}
            if(BParams) LoadActiveBands();
        }
        appMode = mode;
        ResetModifiers();
        if (appMode==FREQUENCY_MODE && !Key_1_pressed) {
            currentFreq = gTxVfo->pRX->Frequency;
            gScanRangeStart = currentFreq - (GetBW() >> 1);
            gScanRangeStop  = currentFreq + (GetBW() >> 1);
        }
        Key_1_pressed = 0;
        BackupRegisters();
        BK4819_WriteRegister(BK4819_REG_30, 0);
        SYSTEM_DelayMs(10);
        BK4819_RX_TurnOn();
        SYSTEM_DelayMs(50);
        uint8_t CodeType = gTxVfo->pRX->CodeType;
        uint8_t Code     = gTxVfo->pRX->Code;
        BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(CodeType, Code));
        ResetInterrupts();
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        BK4819_WriteRegister(BK4819_REG_47, 0x6040);
        BK4819_WriteRegister(BK4819_REG_48, 0xB3A8);  // AF gain
	    ToggleRX(true), ToggleRX(false); // hack to prevent noise when squelch off
        RADIO_SetModulation(settings.modulationType = gTxVfo->Modulation);
        BK4819_SetFilterBandwidth(settings.listenBw, false);
        isListening = true;
        newScanStart = true;
        AutoAdjustFreqChangeStep();
        RelaunchScan();
        for (int i = 0; i < 128; ++i) { rssiHistory[i] = 0; }
        isInitialized = true;
        historyListActive = false;
#ifdef ENABLE_CPU_TEMP
        temp_dc = CpuTemp_ReadDeciCelsius();
        //cpu_hz = CpuInfo_GetClockHz();
#endif
        while (isInitialized) {Tick();}

        if (gSpectrumChangeRequested) {
            Spectrum_state = gRequestedSpectrumState;
            gSpectrumChangeRequested = false;
            RestoreRegisters(); 
            continue;
        } else {
            RestoreRegisters();
            break;
        }
    }
    // Освобождаем mode-специфичные буферы при выходе из спектра
    if (ScanFrequencies) { free(ScanFrequencies); ScanFrequencies = NULL; }
    if (BParams)         { free(BParams);         BParams = NULL; }
    // HFreqs/HBlacklisted не освобождаем — история сохраняется между запусками
}



static void ToggleScanList(int scanListNumber, int single )
{
    if (appMode == SCAN_BAND_MODE) {
      if (single) memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
        else settings.bandEnabled[scanListNumber-1] = !settings.bandEnabled[scanListNumber-1];
    }
    if (appMode == CHANNEL_MODE) {
        if (single) {memset(settings.scanListEnabled, 0, sizeof(settings.scanListEnabled));}
        if(scanListNumber < MR_CHANNELS_LIST){
            settings.scanListEnabled[scanListNumber] = !settings.scanListEnabled[scanListNumber];
            refreshScanListName = true;
        }
    }
}

// ============================================================
// SECTION: EEPROM / Settings persistence
// ============================================================

#ifndef ENABLE_DEV
bool IsVersionMatching(void) {
    uint16_t stored,app_version;
    app_version = APP_VERSION;
    PY25Q16_ReadBuffer(ADRESS_VERSION, &stored, 2);
    SYSTEM_DelayMs(50);
    if (stored != APP_VERSION) PY25Q16_WriteBuffer(ADRESS_VERSION, &app_version, 2, 0);
    return (stored == APP_VERSION);
}
#endif


typedef struct {
    int ShowLines;
    uint8_t DelayRssi;
    uint8_t PttEmission; 
    uint8_t listenBw;
	uint64_t bandListFlags;            // Bits 0-63: bandEnabled[0..63]
    uint32_t scanListFlags;            // Bits 0-31: scanListEnabled[0..31]
    int16_t Trigger;
    uint32_t RangeStart;
    uint32_t RangeStop;
    STEP_Setting_t scanStepIndex;
    uint16_t R40;                      // RF TX Deviation
    uint16_t R29;                      // AF TX noise compressor, AF TX 0dB compressor, AF TX compression ratio
    uint16_t R19;                      // Disable MIC AGC
    uint16_t R73;                      // AFC range select
    uint16_t R10;
    uint16_t R11;
    uint16_t R12;
    uint16_t R13;
    uint16_t R14;
    uint16_t R3C;
    uint16_t R43;
    uint16_t R2B;
    uint16_t SpectrumDelay;
    uint8_t IndexMaxLT;
    uint8_t IndexPS;
    uint8_t Noislvl_OFF;
    uint16_t UOO_trigger;
    uint16_t osdPopupSetting;
    uint8_t GlitchMax;  
    uint8_t Spectrum_state;  
    bool Backlight_On_Rx;
    bool SoundBoost;  
    bool gMonitorScan;
    bool classic_mode;
} SettingsEEPROM;


void LoadSettings()
{
  if(SettingsLoaded) return;
  SettingsEEPROM  eepromData  = {0};
#ifndef ENABLE_DEV
  if(!IsVersionMatching()) ClearSettings();
#endif
  PY25Q16_ReadBuffer(ADRESS_PARAMS, &eepromData, sizeof(eepromData));
  
  BK4819_WriteRegister(BK4819_REG_10, eepromData.R10);
  BK4819_WriteRegister(BK4819_REG_11, eepromData.R11);
  BK4819_WriteRegister(BK4819_REG_12, eepromData.R12);
  BK4819_WriteRegister(BK4819_REG_13, eepromData.R13);
  BK4819_WriteRegister(BK4819_REG_14, eepromData.R14);

  for (int i = 0; i < MR_CHANNELS_LIST; i++) {
    settings.scanListEnabled[i] = (eepromData.scanListFlags >> i) & 0x01;
  }
  settings.rssiTriggerLevelUp = eepromData.Trigger;
  settings.listenBw = eepromData.listenBw;
  BK4819_SetFilterBandwidth(settings.listenBw, false);
  if (eepromData.RangeStart >= 1400000) gScanRangeStart = eepromData.RangeStart;
  if (eepromData.RangeStop >= 1400000) gScanRangeStop = eepromData.RangeStop;
  settings.scanStepIndex = eepromData.scanStepIndex;
  for (int i = 0; i < MAX_BANDS; i++) {
    settings.bandEnabled[i] = (eepromData.bandListFlags & ((uint64_t)1 << i)) != 0;
    }
  DelayRssi = eepromData.DelayRssi;
  if (DelayRssi > 6) DelayRssi =6;
  PttEmission = eepromData.PttEmission;
  validScanListCount = 0;
  ShowLines = eepromData.ShowLines;
  if (ShowLines < 1 || ShowLines > 4) ShowLines = 1;
  SpectrumDelay = eepromData.SpectrumDelay;
  
  IndexMaxLT = eepromData.IndexMaxLT;
  MaxListenTime = listenSteps[IndexMaxLT];
  
  IndexPS = eepromData.IndexPS;
  SpectrumSleepMs = PS_Steps[IndexPS];
  Noislvl_OFF = eepromData.Noislvl_OFF;
  Noislvl_ON  = Noislvl_OFF - NoiseHysteresis; 
  UOO_trigger = eepromData.UOO_trigger;
  osdPopupSetting = eepromData.osdPopupSetting;
  Backlight_On_Rx = eepromData.Backlight_On_Rx;
  GlitchMax = eepromData.GlitchMax;    
  Spectrum_state = eepromData.Spectrum_state;    
  SoundBoost = eepromData.SoundBoost;
  gMonitorScan = eepromData.gMonitorScan;    
  classic = eepromData.classic_mode;    
  BK4819_WriteRegister(BK4819_REG_40, eepromData.R40);
  BK4819_WriteRegister(BK4819_REG_29, eepromData.R29);
  BK4819_WriteRegister(BK4819_REG_19, eepromData.R19);
  BK4819_WriteRegister(BK4819_REG_73, eepromData.R73);
  BK4819_WriteRegister(BK4819_REG_3C, eepromData.R3C);
  BK4819_WriteRegister(BK4819_REG_43, eepromData.R43);
  BK4819_WriteRegister(BK4819_REG_2B, eepromData.R2B);
  
 if (!historyLoaded) {
        LoadHistory();
        historyLoaded = true;
 }
SettingsLoaded = true;
}

static void SaveSettings() 
{
  SettingsEEPROM  eepromData  = {0};
  for (int i = 0; i < MR_CHANNELS_LIST; i++) {
    if (settings.scanListEnabled[i]) eepromData.scanListFlags |= (1 << i);
  }
  eepromData.Trigger = settings.rssiTriggerLevelUp;
  eepromData.listenBw = settings.listenBw;
  eepromData.RangeStart = gScanRangeStart;
  eepromData.RangeStop = gScanRangeStop;
  eepromData.DelayRssi = DelayRssi;
  eepromData.PttEmission = PttEmission;
  eepromData.scanStepIndex = settings.scanStepIndex;
  eepromData.ShowLines = ShowLines;
  eepromData.SpectrumDelay = SpectrumDelay;
  eepromData.IndexMaxLT = IndexMaxLT;
  eepromData.IndexPS = IndexPS;
  eepromData.Backlight_On_Rx = Backlight_On_Rx;
  eepromData.Noislvl_OFF = Noislvl_OFF;
  eepromData.UOO_trigger = UOO_trigger;
  eepromData.osdPopupSetting = osdPopupSetting;
  eepromData.GlitchMax = 20;
  eepromData.GlitchMax  = GlitchMax;   
  eepromData.Spectrum_state = Spectrum_state;    
  eepromData.SoundBoost = SoundBoost;
  eepromData.gMonitorScan = gMonitorScan;
  eepromData.classic_mode = classic;
  for (int i = 0; i < MAX_BANDS; i++) { 
    if (settings.bandEnabled[i]) {
        eepromData.bandListFlags |= ((uint64_t)1 << i);
    }
    }
  eepromData.R40 = BK4819_ReadRegister(BK4819_REG_40);
  eepromData.R29 = BK4819_ReadRegister(BK4819_REG_29);
  eepromData.R19 = BK4819_ReadRegister(BK4819_REG_19);
  eepromData.R73 = BK4819_ReadRegister(BK4819_REG_73);
  eepromData.R10 = BK4819_ReadRegister(BK4819_REG_10);
  eepromData.R11 = BK4819_ReadRegister(BK4819_REG_11);
  eepromData.R12 = BK4819_ReadRegister(BK4819_REG_12);
  eepromData.R13 = BK4819_ReadRegister(BK4819_REG_13);
  eepromData.R14 = BK4819_ReadRegister(BK4819_REG_14);
  eepromData.R3C = BK4819_ReadRegister(BK4819_REG_3C);
  eepromData.R43 = BK4819_ReadRegister(BK4819_REG_43);
  eepromData.R2B = BK4819_ReadRegister(BK4819_REG_2B);
  PY25Q16_WriteBuffer(ADRESS_PARAMS, ((uint8_t*)&eepromData),sizeof(eepromData),0);
  if (Cleared)   ShowOSDPopup("DEFAULT SETTINGS");
  else ShowOSDPopup("PARAMS SAVED");
  Cleared = 0;
}

static void ClearHistory(uint8_t mode) {
    if (mode == 0) {
        memset(HFreqs, 0, HISTORY_SIZE * sizeof(uint32_t));
        memset(HBlacklisted, 0, HISTORY_SIZE * sizeof(bool));
    } else if (mode == 1) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            if (!HBlacklisted[i]) {
                HFreqs[i] = 0;
            }
        }
    } else if (mode == 2) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            if (HBlacklisted[i]) {
                HFreqs[i] = 0;
                HBlacklisted[i] = 0;
            }
        }
    }
    historyListIndex = 0;
    historyScrollOffset = 0;
    indexFs = HISTORY_SIZE;
    WriteHistory();
    indexFs = 0;
    //SaveSettings();
    LoadHistory();
}

void ClearSettings() 
{
  for (int i = 0; i < MR_CHANNELS_LIST; i++) {settings.scanListEnabled[i] = 0;}
  settings.rssiTriggerLevelUp = 5;
  settings.listenBw = 0;
  gScanRangeStart = 43000000;
  gScanRangeStop  = 44000000;
  DelayRssi = 3;
  PttEmission = 2;
  settings.scanStepIndex = STEP_10kHz;
  ShowLines = 1;
  SpectrumDelay = 0;
  MaxListenTime = 0;
  IndexMaxLT = 0;
  IndexPS = 0;
  Backlight_On_Rx = 1;
  Noislvl_OFF = NoisLvl; 
  Noislvl_ON = NoisLvl - NoiseHysteresis;  
  UOO_trigger = 15;
  osdPopupSetting = 500;
  GlitchMax = 20;  
  Spectrum_state = 1; 
  SoundBoost = 0;
  gMonitorScan = false;
  classic = true;
  for (int i = 0; i < MAX_BANDS; i++) {settings.bandEnabled[i] = 0;}
  BK4819_WriteRegister(BK4819_REG_10, 0x0145);
  BK4819_WriteRegister(BK4819_REG_11, 0x01B5);
  BK4819_WriteRegister(BK4819_REG_12, 0x0393);
  BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
  BK4819_WriteRegister(BK4819_REG_14, 0x0019);
  BK4819_WriteRegister(BK4819_REG_40, 13520);
  BK4819_WriteRegister(BK4819_REG_29, 43840);
  BK4819_WriteRegister(BK4819_REG_19, 4161);
  BK4819_WriteRegister(BK4819_REG_73, 18066);
  BK4819_WriteRegister(BK4819_REG_3C, 20360);
  BK4819_WriteRegister(BK4819_REG_43, 13896);
  BK4819_WriteRegister(BK4819_REG_2B, 49152);
  Cleared = 1;
  SaveSettings();
}

// ============================================================
// SECTION: List item text helpers
// ============================================================

static bool GetScanListLabel(uint8_t scanListIndex, char* bufferOut) {
    char channel_name[12];
    uint16_t first_channel = 0xFFFF;
    for (uint16_t ch = MR_CHANNEL_FIRST; ch <= MR_CHANNEL_LAST; ch++) {
        ChannelAttributes_t *att = MR_GetChannelAttributes(ch);
        if (att->scanlist == (uint8_t)(scanListIndex + 1)) {
            first_channel = ch;
            break;
        }
    }
    if (first_channel == 0xFFFF) return false; 
    SETTINGS_FetchChannelName(channel_name, first_channel);
    char nameOrFreq[13];
    if (channel_name[0] == '\0') {
        uint32_t freq = 0;
        PY25Q16_ReadBuffer(0x0000 + (first_channel * 16), (uint8_t *)&freq, 4);
        if (freq < 1400000) {
            return false;
        }

        sprintf(nameOrFreq, "%u.%05u", freq / 100000, freq % 100000);
    } else {
        strncpy(nameOrFreq, channel_name, 12);
        nameOrFreq[12] = '\0';
    }

    if (settings.scanListEnabled[scanListIndex]) {
      sprintf(bufferOut, "%d:%-11s*", scanListIndex + 1, nameOrFreq);
    } else {
        sprintf(bufferOut, "%d:%-11s", scanListIndex + 1, nameOrFreq);
    }

    return true;
}

static void BuildValidScanListIndices() {
    uint8_t ScanListCount = 0;
    char tempName[17];
    for (uint8_t i = 0; i < MR_CHANNELS_LIST; i++) {

        if (GetScanListLabel(i, tempName)) {
            validScanListIndices[ScanListCount++] = i;
        }
    }
    validScanListCount = ScanListCount;
}


static void GetFilteredScanListText(uint16_t displayIndex, char* buffer) {
    uint8_t realIndex = validScanListIndices[displayIndex];
    GetScanListLabel(realIndex, buffer);
}


// ============================================================
// SECTION: Unified list renderer
// ============================================================

#define CHAR_WIDTH_PX 7
static uint8_t ListRightX(const char *s) {
    size_t len = strlen(s);
    return (len > 0 && len * CHAR_WIDTH_PX < 128)
           ? (uint8_t)(128 - len * CHAR_WIDTH_PX) : 1;
}

static void ListDrawSelectedBg(uint8_t line) {
    for (uint8_t x = 0; x < LCD_WIDTH; x++)
        for (uint8_t y = (uint8_t)(line * 8); y < (uint8_t)((line + 1) * 8); y++)
            PutPixel(x, y, true);
}

static void RenderUnifiedList(
    const char  *title,
    const char  *rightHeader,
    bool         useMeter,
    uint16_t     numItems,
    uint16_t     selectedIndex,
    uint16_t     scrollOffset,
    bool         invertSelected,
    bool         boldSelected,
    bool         twoLineMode,
    GetListRowFn getRow)
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    /* Header row — title left, rightHeader right */
    if (useMeter && historyListActive && SpectrumMonitor > 0)
        DrawMeter(0);
    else if (title)
        UI_PrintStringSmallBold(title, 1, 0, 0);
    if (rightHeader && rightHeader[0])
        UI_PrintStringSmallBold(rightHeader, ListRightX(rightHeader), 0, 0);

    const uint8_t maxItems = twoLineMode ? 3 : MAX_VISIBLE_LINES;

    /* Clamp scroll offset */
    if (numItems <= maxItems)
        scrollOffset = 0;
    else if (selectedIndex < scrollOffset)
        scrollOffset = selectedIndex;
    else if (selectedIndex >= scrollOffset + maxItems)
        scrollOffset = selectedIndex - maxItems + 1;

    for (uint8_t i = 0; i < maxItems; i++) {
        uint16_t itemIndex = i + scrollOffset;
        if (itemIndex >= numItems) break;

        ListRow row;
        row.left[0]  = '\0';
        row.right[0] = '\0';
        row.enabled  = false;
        getRow(itemIndex, &row);

        if (row.left[0] == '\0') continue;

        bool sel     = (itemIndex == selectedIndex) && invertSelected;
        bool boldSel = (itemIndex == selectedIndex) && boldSelected;

        if (!twoLineMode) {
            uint8_t line = (uint8_t)(1 + i);
            if (sel) {
                /* Active row: draw bold on BLACK line, then XOR = white bold on black bg */
                if (boldSelected) {
                    /* 1. Строка уже чистая (0x00 после memset gFrameBuffer в начале)
                       2. Рисуем Bold - пиксели букв = 1 (тёмные)
                       3. XOR строки: фон 0x00->0xFF (белый), буквы 1->0 (чёрные)
                       Итог: белый фон, чёрные жирные буквы */
                    UI_PrintStringSmallBold(row.left, 1, 0, line);
                    if (row.right[0])
                        UI_PrintStringSmallBold(row.right, ListRightX(row.right), 0, line);
                    if (row.enabled)
                        UI_PrintStringSmallBold("<", LCD_WIDTH - 7, 0, line);
                    for (uint8_t _x = 0; _x < LCD_WIDTH; _x++)
                        gFrameBuffer[line][_x] ^= 0xFF;
                } else {
                    UI_PrintStringSmallbackground(row.left, 1, 0, line, 1);
                    if (row.right[0])
                        UI_PrintStringSmallbackground(row.right, ListRightX(row.right), 0, line, 1);
                    if (row.enabled)
                        UI_PrintStringSmallbackground("<", LCD_WIDTH - 7, 0, line, 1);
                }
            } else if (boldSel || row.enabled) {
                /* Enabled row: bold, no inversion, marker right */
                UI_PrintStringSmallBold(row.left, 1, 0, line);
                if (row.right[0])
                    UI_PrintStringSmallBold(row.right, ListRightX(row.right), 0, line);
                if (row.enabled)
                    UI_PrintStringSmallBold("<", LCD_WIDTH - 7, 0, line);
            } else {
                /* Normal row */
                UI_PrintStringSmallNormal(row.left, 1, 0, line);
                if (row.right[0])
                    UI_PrintStringSmallNormal(row.right, ListRightX(row.right), 0, line);
            }
        } else {
            /* Two-line mode */
            uint8_t line1 = (uint8_t)(1 + i * 2);
            uint8_t line2 = (uint8_t)(2 + i * 2);
            if (sel) {
                ListDrawSelectedBg(line1);
                ListDrawSelectedBg(line2);
                UI_PrintStringSmallbackground(row.left,  1, 0, line1, 1);
                UI_PrintStringSmallbackground(row.right, 1, 0, line2, 1);
            } else {
                UI_PrintStringSmallNormal(row.left,  1, 0, line1);
                UI_PrintStringSmallNormal(row.right, 1, 0, line2);
            }
        }
    }
    ST7565_BlitFullScreen();
}

/* ---- GetRow callbacks for each list type ---- */

/* History list: frequency[+channel name] on left, TX count on right */
static void GetHistoryRow(uint16_t index, ListRow *row) {
    row->left[0]  = '\0';
    row->right[0] = '\0';
    row->enabled  = false;
    uint32_t f = HFreqs[index];
    if (!f) return;

    char freqStr[10];
    snprintf(freqStr, sizeof(freqStr), "%u.%05u", f / 100000, f % 100000);
    RemoveTrailZeros(freqStr);

    char Name[12] = "";
    uint16_t ch = BOARD_gMR_fetchChannel(f);
    if (ch != 0xFFFF) {
        SETTINGS_FetchChannelName(Name, ch);
        Name[10] = '\0';
    }
    const char *prefix = HBlacklisted[index] ? "#" : "";
    if (Name[0])
        snprintf(row->left, sizeof(row->left), "%s%s %s", prefix, freqStr, Name);
    else
        snprintf(row->left, sizeof(row->left), "%s%s", prefix, freqStr);
}

/* Scanlist multiselect: plain text on left, enabled flag drives bold+marker */
static void GetScanListRow(uint16_t displayIndex, ListRow *row) {
    char buf[20];
    GetFilteredScanListText(displayIndex, buf);
    /* Strip trailing '*' marker and padding spaces */
    size_t len = strlen(buf);
    bool enabled = (len > 0 && buf[len - 1] == '*');
    if (enabled) buf[--len] = '\0';
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
    snprintf(row->left, sizeof(row->left), "%s", buf);
    row->right[0] = '\0';
    uint8_t realIndex = validScanListIndices[displayIndex];
    row->enabled = settings.scanListEnabled[realIndex];
}

static void GetBandRow(uint16_t index, ListRow *row) {
    snprintf(row->left, sizeof(row->left), "%d:%-11s", index + 1, BParams[index].BandName);
    row->right[0] = '\0';
    row->enabled = settings.bandEnabled[index];
}

static void GetParametersRow(uint16_t index, ListRow *row) {
    row->right[0] = '\0';
    row->enabled  = false;
    switch (index) {
        case 0:
            snprintf(row->left,  sizeof(row->left),  "RSSI Delay:");
            snprintf(row->right, sizeof(row->right), "%dms", DelayRssi);
            break;
        case 1:
            snprintf(row->left, sizeof(row->left), "Spectrum Delay:");
            if (SpectrumDelay < 65000)
                snprintf(row->right, sizeof(row->right), "%us", SpectrumDelay / 1000);
            else
                strncpy(row->right, "OFF", sizeof(row->right) - 1);
            break;
        case 2:
            snprintf(row->left,  sizeof(row->left),  "MaxListenTime:");
            snprintf(row->right, sizeof(row->right), "%s", labels[IndexMaxLT]);
            break;
        case 3: {
            /* Preserve full frequency precision with trailing-zero removal */
            char tmp[12];
            snprintf(tmp, sizeof(tmp), "%u.%05u",
                     gScanRangeStart / 100000, gScanRangeStart % 100000);
           // RemoveTrailZeros(tmp);
            snprintf(row->left,  sizeof(row->left),  "Fstart:");
            snprintf(row->right, sizeof(row->right), "%s", tmp);
            break;
        }
        case 4: {
            /* Preserve full frequency precision with trailing-zero removal */
            char tmp[12];
            snprintf(tmp, sizeof(tmp), "%u.%05u",
                     gScanRangeStop / 100000, gScanRangeStop % 100000);
           // RemoveTrailZeros(tmp);
            snprintf(row->left,  sizeof(row->left),  "Fstop:");
            snprintf(row->right, sizeof(row->right), "%s", tmp);
            break;
        }
        case 5: {
            uint32_t step = GetScanStep();
            snprintf(row->left, sizeof(row->left), "Step:");
            snprintf(row->right, sizeof(row->right),
                     step % 100 ? "%uk%02u" : "%uk", step / 100, step % 100);
            break;
        }
        case 6:
            snprintf(row->left,  sizeof(row->left),  "Listen BW:");
            snprintf(row->right, sizeof(row->right), "%s", bwNames[settings.listenBw]);
            break;
        case 7:
            snprintf(row->left,  sizeof(row->left),  "Modulation:");
            snprintf(row->right, sizeof(row->right), "%s", gModulationStr[settings.modulationType]);
            break;
        case 8:
            snprintf(row->left, sizeof(row->left), "RX Backlight:");
            strncpy(row->right, Backlight_On_Rx ? "ON" : "OFF", sizeof(row->right) - 1);
            break;
        case 9:
            snprintf(row->left,  sizeof(row->left),  "Power Save:");
            snprintf(row->right, sizeof(row->right), "%s", labelsPS[IndexPS]);
            break;
        case 10:
            snprintf(row->left,  sizeof(row->left),  "Nois LVL OFF:");
            snprintf(row->right, sizeof(row->right), "%d", Noislvl_OFF);
            break;
        case 11:
            snprintf(row->left, sizeof(row->left), "Popups:");
            if (osdPopupSetting) {
                uint8_t sec = osdPopupSetting / 1000;
                uint8_t dec = (osdPopupSetting % 1000) / 100;
                if (dec) snprintf(row->right, sizeof(row->right), "%d.%ds", sec, dec);
                else     snprintf(row->right, sizeof(row->right), "%ds", sec);
            } else {
                strncpy(row->right, "OFF", sizeof(row->right) - 1);
            }
            break;
        case 12:
            snprintf(row->left,  sizeof(row->left),  "Record Trig:");
            snprintf(row->right, sizeof(row->right), "%d", UOO_trigger);
            break;
        case 13:
            if (AUTO_KEYLOCK) {
                snprintf(row->left,  sizeof(row->left),  "Keylock:");
                snprintf(row->right, sizeof(row->right), "%ds", durations[AUTO_KEYLOCK] / 2);
            } else {
                snprintf(row->left, sizeof(row->left), "Key Unlocked");
            }
            break;
        case 14:
            snprintf(row->left,  sizeof(row->left),  "GlitchMax:");
            snprintf(row->right, sizeof(row->right), "%d", GlitchMax);
            break;
        case 15:
            snprintf(row->left, sizeof(row->left), "SoundBoost:");
            strncpy(row->right, SoundBoost ? "ON" : "OFF", sizeof(row->right) - 1);
            break;
        case 16:
            snprintf(row->left, sizeof(row->left), "PTT:");
            if      (PttEmission == 0) strncpy(row->right, "VFO FREQ", sizeof(row->right) - 1);
            else if (PttEmission == 1) strncpy(row->right, "NINJA",    sizeof(row->right) - 1);
            else                       strncpy(row->right, "LAST RX",  sizeof(row->right) - 1);
            break;
        case 17:
            snprintf(row->left, sizeof(row->left), "Monitor SL");
            if (gMonitorScan) snprintf(row->right, sizeof(row->right), "ON");
            else snprintf(row->right, sizeof(row->right), "OFF");
            break;
        case 18:
            snprintf(row->left, sizeof(row->left), "Clear Histo ALL");
            strncpy(row->right, ">", sizeof(row->right) - 1);
            break;
        case 19:
            snprintf(row->left, sizeof(row->left), "Clear Histo N BL");
            strncpy(row->right, ">", sizeof(row->right) - 1);
            break;
        case 20:
            snprintf(row->left, sizeof(row->left), "Clear Histo BL");
            strncpy(row->right, ">", sizeof(row->right) - 1);
            break;
        case 21:
            snprintf(row->left, sizeof(row->left), "Reset Default");
            strncpy(row->right, ">", sizeof(row->right) - 1);
            break;

        default:
            row->left[0] = '\0';
            break;
    }
}

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
/* ScanList channel detail (two-line mode):
 *   row.left  = "NNN: channel_name"  (line 1)
 *   row.right = "    freq"           (line 2) */
static void GetScanListChannelRow(uint16_t index, ListRow *row) {
    uint16_t channelIndex = scanListChannels[index];
    char channel_name[12];
    SETTINGS_FetchChannelName(channel_name, channelIndex);
    uint32_t freq = gMR_ChannelFrequencyAttributes[channelIndex].Frequency;
    char freqStr[16];
    sprintf(freqStr, " %u.%05u", freq / 100000, freq % 100000);
    //RemoveTrailZeros(freqStr);
    snprintf(row->left,  sizeof(row->left),  "%3d: %s", channelIndex + 1, channel_name);
    snprintf(row->right, sizeof(row->right), "    %s", freqStr);
}
#endif

// ============================================================
// SECTION: List screen render functions
// ============================================================

static void RenderScanListSelect() {
    if (refreshScanListName) {
        BuildValidScanListIndices(); 
        refreshScanListName = false;
    }
    uint8_t selectedCount = 0;
    for (uint8_t i = 0; i < validScanListCount; i++) {
        if (settings.scanListEnabled[validScanListIndices[i]]) selectedCount++;
    }
    char title[24];
    snprintf(title, sizeof(title), "SCANLISTS:");
    char right[8];
    snprintf(right, sizeof(right), "%u/%u", selectedCount, validScanListCount);
    RenderUnifiedList(title, right, false, validScanListCount, scanListSelectedIndex,
                      scanListScrollOffset, true, true,  false, GetScanListRow);
}

static void RenderParametersSelect() {
    RenderUnifiedList("PARAMETERS:", NULL, false, PARAMETER_COUNT, parametersSelectedIndex,
                      parametersScrollOffset, true, true,  false, GetParametersRow);
}

void RenderBandSelect() {
    uint8_t sel = 0;
    for (uint8_t i = 0; i < bandCount; i++)
        if (settings.bandEnabled[i]) sel++;
    char right[8];
    snprintf(right, sizeof(right), "%u/%u", sel, bandCount);
    RenderUnifiedList("BANDS:", right, false, bandCount, bandListSelectedIndex,
                      bandListScrollOffset, true, true,  false, GetBandRow);
}

static void RenderHistoryList() {
    uint16_t count = CountValidHistoryItems();
    char title[24];
    sprintf(title, "HISTORY:%d", count);
    char rssiStr[8];
    sprintf(rssiStr, "R:%d", scanInfo.rssi);
    RenderUnifiedList(title, rssiStr, false, count, historyListIndex,
                      historyScrollOffset, true, true,  false, GetHistoryRow);
    ST7565_BlitFullScreen();
}

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
static void BuildScanListChannels(uint8_t scanListIndex) {
    scanListChannelsCount = 0;
    ChannelAttributes_t att;
    for (uint16_t i = 0; i < MR_CHANNEL_LAST + 1; i++) {
        att = gMR_ChannelAttributes[i];
        if (att.scanlist == scanListIndex + 1) {
            if (scanListChannelsCount < MR_CHANNEL_LAST + 1)
                scanListChannels[scanListChannelsCount++] = i;
        }
    }
}

/* Two-line detail view: 3 items visible, each occupying 2 display lines.
 * Header shows the scanlist number; items rendered via RenderUnifiedList. */
static void RenderScanListChannels() {
    char headerString[24];
    uint8_t realScanListIndex = validScanListIndices[selectedScanListIndex];
    sprintf(headerString, "SL %d CHANNELS:", realScanListIndex + 1);
    RenderUnifiedList(headerString, NULL, false, scanListChannelsCount,
                      scanListChannelsSelectedIndex, scanListChannelsScrollOffset,
                      true, false, true, GetScanListChannelRow);
}
#endif /* ENABLE_SCANLIST_SHOW_DETAIL */
