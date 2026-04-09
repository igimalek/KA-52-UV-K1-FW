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

#include <string.h>
#include <stdlib.h>

#include "../app/dtmf.h"
#include "../app/menu.h"
#include "../bitmaps.h"
#include "../board.h"
#include "../dcs.h"
#include "../driver/backlight.h"
#include "../driver/bk4819.h"
#include "../driver/eeprom.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../frequencies.h"
#include "../helper/battery.h"
#include "../misc.h"
#include "../settings.h"

#ifdef ENABLE_FEAT_F4HWN
    #include "../version.h"
#endif

#include "helper.h"
#include "inputbox.h"
#include "menu.h"
#include "ui.h"


const t_menu_item MenuList[] =
{
//   text,          menu ID
    {"Step",        MENU_STEP          },
    {"Sql",         MENU_SQL           },
    {"Mode",        MENU_AM            },
    {"Power",       MENU_TXP           },
    {"RxMode",      MENU_TDR           },
    {"RxDCS",       MENU_R_DCS         },
    {"RxCTCS",      MENU_R_CTCS        },
    {"TxDCS",       MENU_T_DCS         },
    {"TxCTCS",      MENU_T_CTCS        },
    {"TxOffs",      MENU_OFFSET        },
    {"TxODir",      MENU_SFT_D         },
    {"W/N",         MENU_W_N           },
    #ifdef ENABLE_FEAT_F4HWN_NARROWER
    {"SetNFM",      MENU_SET_NFM       },
    #endif
    {"Compnd",      MENU_COMPAND       },
#ifdef ENABLE_SCRAMBLER
    {"Scramb",      MENU_SCR           },
#endif
    {"Roger",       MENU_ROGER         },
    {"1 Call",      MENU_1_CALL        },
    {"STE",         MENU_STE           },
    {"RP STE",      MENU_RP_STE        },
    {"ScList",      MENU_S_LIST        },
    {"ScnRev",      MENU_SC_REV        },
    {"ChSave",      MENU_MEM_CH        },
    {"ChDele",      MENU_DEL_CH        },
    {"ChName",      MENU_MEM_NAME      },
    {"ChDisp",      MENU_MDF           },
    {"F1Shrt",      MENU_F1SHRT        },
    {"F1Long",      MENU_F1LONG        },
    {"F2Shrt",      MENU_F2SHRT        },
    {"F2Long",      MENU_F2LONG        },
    {"M Long",      MENU_MLONG         },
    {"KeyLck",      MENU_AUTOLK        },
    {"TxTOut",      MENU_TOT           },
    {"BatSav",      MENU_SAVE          },
    {"Mic",         MENU_MIC           },
    {"POnMsg",      MENU_PONMSG        },
    {"BLTime",      MENU_ABR           },
    {"BLMin",       MENU_ABR_MIN       },
    {"BLMax",       MENU_ABR_MAX       },
    {"BLTxRx",      MENU_ABR_ON_TX_RX  },
#ifdef ENABLE_FEAT_F4HWN
 //   {"SetPwr",      MENU_SET_PWR       },
    {"SetInv",      MENU_SET_INV       },
    {"BtnInv",      MENU_BTN_INV       },
    {"SetLck",      MENU_SET_LCK       },
    {"BatTxt",      MENU_BAT_TXT       },
    {"MicBar",      MENU_SET_MET       },
    #ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER
    {"SetTmr",      MENU_SET_TMR       },
    #endif
#endif
    // hidden menu items (PTT + upper side button at power-on)
    {"F Lock",      MENU_F_LOCK        },
#ifdef ENABLE_SCRAMBLER
    {"ScraEn",      MENU_SCREN         },
#endif
#ifdef ENABLE_F_CAL_MENU
    {"FrCali",      MENU_F_CALI        },
#endif
    {"BatCal",      MENU_BATCAL        },
    {"BatTyp",      MENU_BATTYP        },
    {"Reset",       MENU_RESET         },
    {"",            0xff               }
};

const uint8_t FIRST_HIDDEN_MENU_ITEM = MENU_F_LOCK;

const char gSubMenu_TXP[][6] =
{
    "OFF",
    "LOW 1",
    "LOW 2",
    "LOW 3",
    "LOW 4",
    "LOW 5",
    "MID",
    "HIGH"
};

const char gSubMenu_SFT_D[][4] =
{
    "OFF",
    "+",
    "-"
};

const char gSubMenu_W_N[][7] =
{
    "WIDE",
    "NARROW"
};

const char gSubMenu_OFF_ON[][4] =
{
    "OFF",
    "ON"
};

const char gSubMenu_NA[4] =
{
    "N/A"
};

const char* const gSubMenu_RXMode[] =
{
    "MAIN\nONLY",       // 0 TX and RX on main only
    "DUAL RX\nRESPOND", // 1 Watch both, TX switches to active VFO
    "MAIN TX\nDUAL RX", // 2 Always TX on main, RX on both
};


/*const char* const gSubMenu_RXMode[] =
{
    "MAIN\nONLY",       // 0 -> 0   TX and RX on main only
    "DUAL RX\nMAIN TX"  // 1 -> 3   always TX on main, but RX on both
    "DUAL RX\nRESPOND", // 2 -> 1   Watch both and respond
    "CROSS\nBAND",      // 3 -> 2   TX on main, RX on secondary
};
*/


#ifdef ENABLE_VOICE
    const char gSubMenu_VOICE[][4] =
    {
        "OFF",
        "CHI",
        "ENG"
    };
#endif

const char* const gSubMenu_MDF[] =
{
    "FREQ",
    "CHANNEL\nNUMBER",
    "NAME",
    "NAME\n+\nFREQ"
};

#ifdef ENABLE_ALARM
    const char gSubMenu_AL_MOD[][5] =
    {
        "SITE",
        "TONE"
    };
#endif

#ifdef ENABLE_DTMF_CALLING
const char gSubMenu_D_RSP[][11] =
{
    "DO\nNOTHING",
    "RING",
    "REPLY",
    "BOTH"
};
#endif

const char* const gSubMenu_PTT_ID[] =
{
    "OFF",
    "UP CODE",
    "DOWN CODE",
    "UP+DOWN\nCODE",
    "APOLLO\nQUINDAR"
};

const char gSubMenu_PONMSG[][8] =
{
    "ON",
    "OFF"
};

const char gSubMenu_ROGER[][6] =
{
    "OFF",
    "ROGER",
    "MDC"
};

const char gSubMenu_RESET[][4] =
{
    "VFO",
    "ALL"
};

const char * const gSubMenu_F_LOCK[] =
{
    "UNLOCK\nALL",      // 0 = F_LOCK_NONE    — TX везде
    "LOCK\nALL",        // 1 = F_LOCK_ALL     — TX заблокирован
    "136-174\n400-500", // 2 = F_LOCK_136_500
    "PMR+\nLPD",        // 3 = F_LOCK_PMR_LPD
};

const char gSubMenu_RX_TX[][6] =
{
    "OFF",
    "TX",
    "RX",
    "TX/RX"
};

const char gSubMenu_BAT_TXT[][8] =
{
    "NONE",
    "VOLTAGE",
    "PERCENT"
};

const char * const gSubMenu_BATTYP[] =
{
    "1400mAh\nUV-K1",
    "1400mAh\nUV-K1",
    "2500mAh\nUV-K1",
    "1600mAh\nUV-K5",
    "2200mAh\nUV-R5+",
    "3500mAh\nUV-K5"
};

#ifdef ENABLE_SCRAMBLER
const char gSubMenu_SCRAMBLER[][7] =
{
    "OFF",
    "2600Hz",
    "2700Hz",
    "2800Hz",
    "2900Hz",
    "3000Hz",
    "3100Hz",
    "3200Hz",
    "3300Hz",
    "3400Hz",
    "3500Hz"
};
#endif


#ifdef ENABLE_FEAT_F4HWN
    const char gSubMenu_SET_PWR[][6] =
    {
        "< 20m",
        "125m",
        "250m",
        "500m",
        "1",
        "2",
        "5"
    };

    // const char gSubMenu_SET_PTT[][8] =
    // {
    //     "CLASSIC",
    //     "ONEPUSH"
    // };// PTTDEL

    const char gSubMenu_SET_TOT[][7] =    
    {
        "OFF",
        "SOUND",
        "VISUAL",
        "ALL"
    };

    const char gSubMenu_SET_LCK[][9] =
    {
        "KEYS",
        "KEYS+PTT"
    };



    #ifdef ENABLE_FEAT_F4HWN_NARROWER
        const char gSubMenu_SET_NFM[][9] =
        {
            "NARROW",
            "NARROWER"
        };
    #endif

#endif

const t_sidefunction gSubMenu_SIDEFUNCTIONS[] =
{
    {"NONE",            ACTION_OPT_NONE},
#ifdef ENABLE_FLASHLIGHT
    {"FLASH\nLIGHT",    ACTION_OPT_FLASHLIGHT},
#endif
    {"POWER",           ACTION_OPT_POWER},
    {"MONITOR",         ACTION_OPT_MONITOR},
    {"SCAN",            ACTION_OPT_SCAN},
#ifdef ENABLE_VOX
    {"VOX",             ACTION_OPT_VOX},
#endif
#ifdef ENABLE_ALARM
    {"ALARM",           ACTION_OPT_ALARM},
#endif
#ifdef ENABLE_FMRADIO
    {"FM RADIO",        ACTION_OPT_FM},
#endif
#ifdef ENABLE_TX1750
    {"1750Hz",          ACTION_OPT_1750},
#endif
    {"LOCK\nKEYPAD",    ACTION_OPT_KEYLOCK},
    {"VFO A\nVFO B",    ACTION_OPT_A_B},
    {"VFO\nMEM",        ACTION_OPT_VFO_MR},
    {"MODE",            ACTION_OPT_SWITCH_DEMODUL},
#ifdef ENABLE_BLMIN_TMP_OFF
    {"BLMIN\nTMP OFF",  ACTION_OPT_BLMIN_TMP_OFF},        
#endif
#ifdef ENABLE_FEAT_F4HWN
    {"RX MODE",         ACTION_OPT_RXMODE},
    {"MAIN ONLY",       ACTION_OPT_MAINONLY},
    {"PTT",             ACTION_OPT_PTT},
    {"WIDE\nNARROW",    ACTION_OPT_WN},
      
    {"MUTE",            ACTION_OPT_MUTE},
      
#endif
};

const uint8_t gSubMenu_SIDEFUNCTIONS_size = ARRAY_SIZE(gSubMenu_SIDEFUNCTIONS);

bool    gIsInSubMenu;
uint8_t gMenuCursor;
int UI_MENU_GetCurrentMenuId() {
    if(gMenuCursor < ARRAY_SIZE(MenuList))
        return MenuList[gMenuCursor].menu_id;

    return MenuList[ARRAY_SIZE(MenuList)-1].menu_id;
}

uint8_t UI_MENU_GetMenuIdx(uint8_t id)
{
    for(uint8_t i = 0; i < ARRAY_SIZE(MenuList); i++)
        if(MenuList[i].menu_id == id)
            return i;
    return 0;
}

int32_t gSubMenuSelection;

// edit box
char    edit_original[17]; // a copy of the text before editing so that we can easily test for changes/difference
char    edit[17];
int     edit_index;

void UI_DisplayMenu(void)
{
    const unsigned int menu_list_width = 6; // max no. of characters on the menu list (left side)
    const unsigned int menu_item_x1    = (9 * menu_list_width) + 3;
    const unsigned int menu_item_x2    = LCD_WIDTH - 1;
    unsigned int       i;
    char               String[64];  // bigger cuz we can now do multi-line in one string (use '\n' char)

#ifdef ENABLE_DTMF_CALLING
    char               Contact[16];
#endif

    UI_DisplayClear();



#ifndef ENABLE_CUSTOM_MENU_LAYOUT
        // original menu layout
    for (i = 0; i < 3; i++)
        if (gMenuCursor > 0 || i > 0)
            if ((gMenuListCount - 1) != gMenuCursor || i != 2)
                UI_PrintString(MenuList[gMenuCursor + i - 1].name, 0, 0, i * 2, 8);

    // invert the current menu list item pixels
    for (i = 0; i < (8 * menu_list_width); i++)
    {
        gFrameBuffer[2][i] ^= 0xFF;
        gFrameBuffer[3][i] ^= 0xFF;
    }

    // draw vertical separating dotted line
    for (i = 0; i < 7; i++)
        gFrameBuffer[i][(8 * menu_list_width) + 1] = 0xAA;

    // draw the little sub-menu triangle marker
    if (gIsInSubMenu)
        memcpy(gFrameBuffer[0] + (8 * menu_list_width) + 1, BITMAP_CurrentIndicator, sizeof(BITMAP_CurrentIndicator));

    // draw the menu index number/count
    sprintf(String, "%2u.%u", 1 + gMenuCursor, gMenuListCount);

    UI_PrintStringSmallNormal(String, 2, 0, 6);

#else
    {   // custom menu layout: 4 visible items, selected at row 2, inverted
        const int menu_index = gMenuCursor;

        // Draw 4 items: -1, 0(selected), +1, +2 at rows 1..4
        for (int slot = 1; slot < 5; slot++)
        {
            const int k = menu_index + slot - 2;  // item index relative to selected

            // resolve wrap-around
            int ki = k;
            if (ki < 0)
                ki += (int)gMenuListCount;
            else if (ki >= (int)gMenuListCount)
                ki -= (int)gMenuListCount;

            if (ki < 0 || ki >= (int)gMenuListCount)
                continue;

            const unsigned int row = (unsigned int)slot;
            if (slot == 2)
                UI_PrintStringSmallBold(MenuList[ki].name, 0, 0, row);   // selected — bold
            else
                UI_PrintStringSmallNormal(MenuList[ki].name, 0, 0, row); // others — normal
        }

        // Invert selected row (row 2), left side only
        for (i = 0; i < (unsigned int)(8 * menu_list_width); i++)
            gFrameBuffer[2][i] ^= 0xFF;

        // Sub-menu triangle marker at separator, same row as selected
        if (gIsInSubMenu)
            memcpy(gFrameBuffer[2] + (8 * menu_list_width) + 1,
                   BITMAP_CurrentIndicator, sizeof(BITMAP_CurrentIndicator));

        // Index counter at row 6
        sprintf(String, "%02u/%u", 1 + gMenuCursor, gMenuListCount);
        UI_PrintStringSmallNormal(String, 6, 0, 6);
    }
#endif

    // **************

    memset(String, 0, sizeof(String));

    bool already_printed = false;

    /* Brightness is set to max in some entries of this menu. Return it to the configured brightness
       level the "next" time we enter here.I.e., when we move from one menu to another.
       It also has to be set back to max when pressing the Exit key. */

    BACKLIGHT_TurnOn();

    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
        uint8_t gaugeLine = 0;
        uint8_t gaugeMin = 0;
        uint8_t gaugeMax = 0;
    //#endif

    switch (UI_MENU_GetCurrentMenuId())
    {
        case MENU_SQL:
            sprintf(String, "%d", gSubMenuSelection);
            break;

        case MENU_MIC:
            {   // display the mic gain in actual dB rather than just an index number
                const uint8_t mic = gMicGain_dB2[gSubMenuSelection];
                sprintf(String, "+%u.%01udB", mic / 2, mic % 2);
            }
            break;

        case MENU_STEP: {
            uint16_t step = gStepFrequencyTable[FREQUENCY_GetStepIdxFromSortedIdx(gSubMenuSelection)];
            sprintf(String, "%d.%02ukHz", step / 100, step % 100);
            break;
        }

        case MENU_TXP:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, gSubMenu_TXP[gSubMenuSelection]);
            }
            else
            {
                sprintf(String, "%s\n%sW", gSubMenu_TXP[gSubMenuSelection], gSubMenu_SET_PWR[gSubMenuSelection - 1]);
            }
            break;

        case MENU_R_DCS:
        case MENU_T_DCS:
            if (gSubMenuSelection == 0)
                strcpy(String, gSubMenu_OFF_ON[0]);
            else if (gSubMenuSelection < 105)
                sprintf(String, "D%03oN", DCS_Options[gSubMenuSelection -   1]);
            else
                sprintf(String, "D%03oI", DCS_Options[gSubMenuSelection - 105]);
            break;

        case MENU_R_CTCS:
        case MENU_T_CTCS:
        {
            if (gSubMenuSelection == 0)
                strcpy(String, gSubMenu_OFF_ON[0]);
            else
                sprintf(String, "%u.%uHz", CTCSS_Options[gSubMenuSelection - 1] / 10, CTCSS_Options[gSubMenuSelection - 1] % 10);
            break;
        }

        case MENU_SFT_D:
            strcpy(String, gSubMenu_SFT_D[gSubMenuSelection]);
            break;

        case MENU_OFFSET:
            if (!gIsInSubMenu || gInputBoxIndex == 0)
            {
                sprintf(String, "%3d.%05u", gSubMenuSelection / 100000, abs(gSubMenuSelection) % 100000);
                UI_PrintStringSmallBold(String, menu_item_x1, menu_item_x2, 2);
            }
            else
            {
                const char * ascii = INPUTBOX_GetAscii();
                sprintf(String, "%.3s.%.3s  ",ascii, ascii + 3);
                UI_PrintStringSmallBold(String, menu_item_x1, menu_item_x2, 2);
            }

            UI_PrintStringSmallBold("MHz",  menu_item_x1, menu_item_x2, 3);

            already_printed = true;
            break;

        case MENU_W_N:
            strcpy(String, gSubMenu_W_N[gSubMenuSelection]);
            break;


#ifdef ENABLE_SCRAMBLER
        case MENU_SCR:
            strcpy(String, gSubMenu_SCRAMBLER[gSubMenuSelection]);
            
            if(gRxVfo->Modulation == MODULATION_FM){
                if (gSubMenuSelection > 0 && gSetting_ScrambleEnable)
                {
                    BK4819_EnableScramble(gSubMenuSelection - 1);
                }
                else
                {
                    BK4819_DisableScramble();
                }
        }
            //if (gRxVfo->Modulation == MODULATION_AM)
            //    BK4819_SetFilterBandwidth(BK4819_FILTER_BW_AM, true);
            break;
#endif


        case MENU_VOX:
            #ifdef ENABLE_VOX
                sprintf(String, gSubMenuSelection == 0 ? gSubMenu_OFF_ON[0] : "%u", gSubMenuSelection);
            #else
                strcpy(String, gSubMenu_NA);
            #endif
            break;

        case MENU_ABR:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, gSubMenu_OFF_ON[0]);
            }
            else if(gSubMenuSelection < 61)
            {
                sprintf(String, "%02dm:%02ds", (((gSubMenuSelection) * 5) / 60), (((gSubMenuSelection) * 5) % 60));
                //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                //ST7565_Gauge(4, 1, 60, gSubMenuSelection);
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 60;
                //#endif
            }
            else
            {
                strcpy(String, "ON");
            }

            // Obsolete ???
            //if(BACKLIGHT_GetBrightness() < 4)
            //    BACKLIGHT_SetBrightness(4);
            break;

        case MENU_ABR_MIN:
        case MENU_ABR_MAX:
            sprintf(String, "%d", gSubMenuSelection);
            if(gIsInSubMenu)
                BACKLIGHT_SetBrightness(gSubMenuSelection);
            // Obsolete ???
            //else if(BACKLIGHT_GetBrightness() < 4)
            //    BACKLIGHT_SetBrightness(4);
            break;

        case MENU_AM:
            strcpy(String, gModulationStr[gSubMenuSelection]);
            break;

        case MENU_AUTOLK:
            if (gSubMenuSelection == 0)
                strcpy(String, gSubMenu_OFF_ON[0]);
            else
            {
                sprintf(String, "%02dm:%02ds", ((gSubMenuSelection * 15) / 60), ((gSubMenuSelection * 15) % 60));
                //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                //ST7565_Gauge(4, 1, 40, gSubMenuSelection);
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 40;
                //#endif
            }
            break;

        case MENU_COMPAND:
        case MENU_ABR_ON_TX_RX:
            strcpy(String, gSubMenu_RX_TX[gSubMenuSelection]);
            break;
        case MENU_BEEP:
        case MENU_S_ADD1:
        case MENU_S_ADD2:
        case MENU_S_ADD3:
        case MENU_STE:
        case MENU_D_ST:
#ifdef ENABLE_DTMF_CALLING
        case MENU_D_DCD:
#endif
#ifdef ENABLE_DTMF_LIVE
        case MENU_D_LIVE_DEC:
#endif
        #ifdef ENABLE_NOAA
            case MENU_NOAA_S:
        #endif
#ifndef ENABLE_FEAT_F4HWN
        case MENU_350TX:
        case MENU_200TX:
        case MENU_500TX:
#endif
#ifdef ENABLE_350EN
        case MENU_350EN:
#endif
#ifdef ENABLE_SCRAMBLER
        case MENU_SCREN:
#endif

#ifdef ENABLE_FEAT_F4HWN
        #ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER //calypso
        case MENU_SET_TMR:
        #endif
#endif
            strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
            break;

        case MENU_MEM_CH:
        case MENU_1_CALL:
        case MENU_DEL_CH:
        {
            const bool valid = RADIO_CheckValidChannel(gSubMenuSelection, false, 0);

            UI_GenerateChannelStringEx(String, valid, gSubMenuSelection);
            UI_PrintStringSmallBold(String, menu_item_x1, menu_item_x2, 1);

            if (valid && !gAskForConfirmation)
            {   // show the frequency so that the user knows the channels frequency
                const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);
                sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
                UI_PrintStringSmallBold(String, menu_item_x1, menu_item_x2, 3);
            }

            SETTINGS_FetchChannelName(String, gSubMenuSelection);
            UI_PrintStringSmallBold(String[0] ? String : "--", menu_item_x1, menu_item_x2, 2);
            already_printed = true;
            break;
        }

        case MENU_MEM_NAME:
        {
            const bool valid = RADIO_CheckValidChannel(gSubMenuSelection, false, 0);

            UI_GenerateChannelStringEx(String, valid, gSubMenuSelection);
            UI_PrintStringSmallBold(String, menu_item_x1, menu_item_x2, 1);

            if (valid)
            {
                const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);

                //if (!gIsInSubMenu || edit_index < 0)
                if (!gIsInSubMenu)
                    edit_index = -1;
                if (edit_index < 0)
                {   // show the channel name
                    SETTINGS_FetchChannelName(String, gSubMenuSelection);
                    char *pPrintStr = String[0] ? String : "--";
                    UI_PrintStringSmallBold(pPrintStr, menu_item_x1, menu_item_x2, 2);
                }
                else
                {   // show the channel name being edited
                    //UI_PrintString(edit, menu_item_x1, 0, 2, 8);
                    UI_PrintStringSmallBold(edit, menu_item_x1, menu_item_x2, 2);
                    if (edit_index < 10)
                        //UI_PrintString("^", menu_item_x1 + (8 * edit_index), 0, 4, 8);  // show the cursor
                        UI_PrintStringSmallBold("^", menu_item_x1 - 1 + (8 * edit_index), 0, 4); // show the cursor
                }

                if (!gAskForConfirmation)
                {   // show the frequency so that the user knows the channels frequency
                    sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
                    UI_PrintStringSmallBold(String, menu_item_x1, menu_item_x2, 3 + (gIsInSubMenu && edit_index >= 0));
                }
            }

            already_printed = true;
            break;
        }

        case MENU_SAVE:
            sprintf(String, gSubMenuSelection == 0 ? gSubMenu_OFF_ON[0] : "1:%u", gSubMenuSelection);
            break;

        case MENU_TDR:
            strcpy(String, gSubMenu_RXMode[gSubMenuSelection]);
            // 0=MAIN ONLY, 1=DUAL RX RESPOND, 2=MAIN TX DUAL RX
            switch (gSubMenuSelection) {
                case 0:
                    gEeprom.DUAL_WATCH       = DUAL_WATCH_OFF;
                    gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
                    break;
                case 1:
                    gEeprom.DUAL_WATCH       = gEeprom.TX_VFO + 1;
                    gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
                    break;
                case 2:
                    gEeprom.DUAL_WATCH       = gEeprom.TX_VFO + 1;
                    gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_CHAN_A;
                    break;
            }
            #ifdef ENABLE_FEAT_F4HWN
                gDW = gEeprom.DUAL_WATCH;
                gSaveRxMode = true;
            #endif
            break;

        case MENU_TOT:
            sprintf(String, "%02dm:%02ds", (((gSubMenuSelection + 1) * 5) / 60), (((gSubMenuSelection + 1) * 5) % 60));
            //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
            //ST7565_Gauge(4, 5, 179, gSubMenuSelection);
            gaugeLine = 4;
            gaugeMin = 5;
            gaugeMax = 179;
            //#endif
            break;

        #ifdef ENABLE_VOICE
            case MENU_VOICE:
                strcpy(String, gSubMenu_VOICE[gSubMenuSelection]);
                break;
        #endif

        case MENU_SC_REV:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, "STOP");
            }
            else if(gSubMenuSelection < 81)
            {
                sprintf(String, "CARRIER\n%02ds:%03dms", ((gSubMenuSelection * 250) / 1000), ((gSubMenuSelection * 250) % 1000));
             
            }
            else
            {
                sprintf(String, "TIMEOUT\n%02dm:%02ds", (((gSubMenuSelection - 80) * 5) / 60), (((gSubMenuSelection - 80) * 5) % 60));
                //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                //ST7565_Gauge(5, 80, 104, gSubMenuSelection);
                gaugeLine = 5;
                gaugeMin = 80;
                gaugeMax = 104;
                //#endif
            }
            break;

        case MENU_MDF:
            strcpy(String, gSubMenu_MDF[gSubMenuSelection]);
            break;

        case MENU_RP_STE:
            sprintf(String, gSubMenuSelection == 0 ? gSubMenu_OFF_ON[0] : "%u*100ms", gSubMenuSelection);
            break;

        case MENU_S_LIST:
            if (gSubMenuSelection == 0) {
                strcpy(String, "OFF");
            } else if (gSubMenuSelection == MR_CHANNELS_LIST + 1) {
                strcpy(String, "MONITOR");
            } else {
                // Show list name if set, otherwise show number
                const char *lname = gListName[gSubMenuSelection - 1];
                if (!IsEmptyName(lname, sizeof(gListName[0])))
                    snprintf(String, sizeof(String), "%.3s\n[%02d]", lname, gSubMenuSelection);
                else
                    sprintf(String, "[%02d]", gSubMenuSelection);
            }
            break;

        #ifdef ENABLE_ALARM
            case MENU_AL_MOD:
                sprintf(String, gSubMenu_AL_MOD[gSubMenuSelection]);
                break;
        #endif

#ifdef ENABLE_DTMF_CALLING
        case MENU_ANI_ID:
            strcpy(String, gEeprom.ANI_DTMF_ID);
            break;
#endif
#ifdef ENABLE_DTMF_CODES
        case MENU_UPCODE:
            sprintf(String, "%.8s\n%.8s", gEeprom.DTMF_UP_CODE, gEeprom.DTMF_UP_CODE + 8);
            break;

        case MENU_DWCODE:
            sprintf(String, "%.8s\n%.8s", gEeprom.DTMF_DOWN_CODE, gEeprom.DTMF_DOWN_CODE + 8);
            break;
#endif

#ifdef ENABLE_DTMF_CALLING
        case MENU_D_RSP:
            strcpy(String, gSubMenu_D_RSP[gSubMenuSelection]);
            break;

        case MENU_D_HOLD:
            sprintf(String, "%ds", gSubMenuSelection);
            break;
#endif
#ifdef ENABLE_DTMF_PRE
        case MENU_D_PRE:
            sprintf(String, "%d*10ms", gSubMenuSelection);
            break;
#endif

        case MENU_PTT_ID:
            strcpy(String, gSubMenu_PTT_ID[gSubMenuSelection]);
            break;

        case MENU_BAT_TXT:
            strcpy(String, gSubMenu_BAT_TXT[gSubMenuSelection]);
            break;

#ifdef ENABLE_DTMF_CALLING
        case MENU_D_LIST:
            gIsDtmfContactValid = DTMF_GetContact((int)gSubMenuSelection - 1, Contact);
            if (!gIsDtmfContactValid)
                strcpy(String, "NULL");
            else
                memcpy(String, Contact, 8);
            break;
#endif

        case MENU_PONMSG:
            strcpy(String, gSubMenu_PONMSG[gSubMenuSelection]);
            break;

        case MENU_ROGER:
            strcpy(String, gSubMenu_ROGER[gSubMenuSelection]);
            break;

        case MENU_RESET:
            strcpy(String, gSubMenu_RESET[gSubMenuSelection]);
            break;

case MENU_F_LOCK: // разрешить всё
    strcpy(String, gSubMenu_F_LOCK[gSubMenuSelection]);
    break;

        #ifdef ENABLE_F_CAL_MENU
            case MENU_F_CALI:
                {
                    const uint32_t value   = 22656 + gSubMenuSelection;
                    const uint32_t xtal_Hz = (0x4f0000u + value) * 5;

                    writeXtalFreqCal(gSubMenuSelection, false);

                    sprintf(String, "%d\n%u.%06u\nMHz",
                        gSubMenuSelection,
                        xtal_Hz / 1000000, xtal_Hz % 1000000);
                }
                break;
        #endif

        case MENU_BATCAL:
        {
            const uint16_t vol = (uint32_t)gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection;
            sprintf(String, "%u.%02uV\n%u", vol / 100, vol % 100, gSubMenuSelection);
            break;
        }

        case MENU_BATTYP:
            strcpy(String, gSubMenu_BATTYP[gSubMenuSelection]);
            break;

        case MENU_F1SHRT:
        case MENU_F1LONG:
        case MENU_F2SHRT:
        case MENU_F2LONG:
        case MENU_MLONG:
            strcpy(String, gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].name);
            break;

#ifdef ENABLE_FEAT_F4HWN_SLEEP
        case MENU_SET_OFF:
            if(gSubMenuSelection == 0)
            {
                strcpy(String, gSubMenu_OFF_ON[0]);
            }
            else if(gSubMenuSelection < 121)
            {
                sprintf(String, "%dh:%02dm", (gSubMenuSelection / 60), (gSubMenuSelection % 60));
                //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                //ST7565_Gauge(4, 1, 120, gSubMenuSelection);
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 120;
                //#endif
            }
            break;
#endif

#ifdef ENABLE_FEAT_F4HWN
     //   case MENU_SET_PWR:
     //       sprintf(String, "%s\n%sW", gSubMenu_TXP[gSubMenuSelection + 1], gSubMenu_SET_PWR[gSubMenuSelection]);
     //       break;

#ifdef ENABLE_SET_TOT_EOT
        case MENU_SET_TOT:
        case MENU_SET_EOT:
            strcpy(String, gSubMenu_SET_TOT[gSubMenuSelection]);
            break;
#endif

        case MENU_SET_CTR:
            #ifdef ENABLE_FEAT_F4HWN_CTR
                sprintf(String, "%d", gSubMenuSelection);
                gSetting_set_ctr = gSubMenuSelection;
                ST7565_ContrastAndInv();
            #else
                strcpy(String, gSubMenu_NA);
            #endif
            break;

        case MENU_SET_INV:
            #ifdef ENABLE_FEAT_F4HWN_INV
                strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
                ST7565_ContrastAndInv();
            #else
                strcpy(String, gSubMenu_NA);
            #endif
            break;

        case MENU_BTN_INV:
            strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
            break;

      /*  case MENU_TX_LOCK:
            if(TX_freq_check(gEeprom.VfoInfo[gEeprom.TX_VFO].pTX->Frequency) == 0)
            {
                strcpy(String, "Inside\nF Lock\nPlan");
            }
            else
            {
                strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
            }
            break;
      */
        case MENU_SET_LCK:
            strcpy(String, gSubMenu_SET_LCK[gSubMenuSelection]);
            break;

        case MENU_SET_MET:
            strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);   
            break;

        #ifdef ENABLE_FEAT_F4HWN_NARROWER
            case MENU_SET_NFM:
                strcpy(String, gSubMenu_SET_NFM[gSubMenuSelection]);
                break;
        #endif

        #ifdef ENABLE_FEAT_F4HWN_VOL
            case MENU_SET_VOL:
                if(gSubMenuSelection == 0)
                {
                    strcpy(String, gSubMenu_OFF_ON[0]);
                }
                else if(gSubMenuSelection < 64)
                {
                    sprintf(String, "%02u", gSubMenuSelection);
                    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
                    //ST7565_Gauge(4, 1, 63, gSubMenuSelection);
                    gaugeLine = 4;
                    gaugeMin = 1;
                    gaugeMax = 63;
                    //#endif
                }
                gEeprom.VOLUME_GAIN = gSubMenuSelection;
                BK4819_WriteRegister(BK4819_REG_48,
                    (11u << 12)                |     // ??? .. 0 ~ 15, doesn't seem to make any difference
                    ( 0u << 10)                |     // AF Rx Gain-1
                    (gEeprom.VOLUME_GAIN << 4) |     // AF Rx Gain-2
                    (gEeprom.DAC_GAIN    << 0));     // AF DAC Gain (after Gain-1 and Gain-2)
                break;
        #endif
#endif

    }

    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
    if(gaugeLine != 0)
    {
        ST7565_Gauge(gaugeLine, gaugeMin, gaugeMax, gSubMenuSelection);
    }
    //#endif

    if (!already_printed)
    {   // we now do multi-line text in a single string

        unsigned int y;
        unsigned int lines = 1;
        unsigned int len   = strlen(String);
        bool         small = false;

        if (len > 0)
        {
            // count number of lines
            for (i = 0; i < len; i++)
            {
                if (String[i] == '\n' && i < (len - 1))
                {   // found new line char
                    lines++;
                    String[i] = 0;  // null terminate the line
                }
            }

            if (lines > 3)
            {   // use small text
                small = true;
                if (lines > 7)
                    lines = 7;
            }

            // center vertically'ish
            /*
            if (small)
                y = 3 - ((lines + 0) / 2);  // untested
            else
                y = 2 - ((lines + 0) / 2);
            */

            // center vertically in rows 0..5 (row 6 = counter)
            y = (6 - (lines < 6 ? lines : 6)) / 2;

            // only for SysInf
            if(UI_MENU_GetCurrentMenuId() == MENU_VOL)
            {
                sprintf(edit, "%u.%02uV %u%%",
                    gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100,
                    BATTERY_VoltsToPercent(gBatteryVoltageAverage)
                );

                UI_PrintStringSmallNormal(edit, 54, 127, 1);

                #ifdef ENABLE_FEAT_F4HWN
                    UI_PrintStringSmallNormal(Edition, 54, 127, 6);
                #endif

                y = 2;
            }

            // draw the text lines
            for (i = 0; i < len && lines > 0; lines--)
            {
                if (small)
                    UI_PrintStringSmallNormal(String + i, menu_item_x1, menu_item_x2, y);
                else
                    UI_PrintStringSmallBold(String + i, menu_item_x1, menu_item_x2, y);

                // look for start of next line
                while (i < len && String[i] >= 32)
                    i++;

                // hop over the null term char(s)
                while (i < len && String[i] < 32)
                    i++;

                y += 1;
            }
        }
    }

    if (UI_MENU_GetCurrentMenuId() == MENU_SLIST1 || UI_MENU_GetCurrentMenuId() == MENU_SLIST2 || UI_MENU_GetCurrentMenuId() == MENU_SLIST3)
    {
        i = UI_MENU_GetCurrentMenuId() - MENU_SLIST1;

        char *pPrintStr = String;

        if (gSubMenuSelection < 0) {
            pPrintStr = "NULL";
        } else {
            UI_GenerateChannelStringEx(String, true, gSubMenuSelection);
            pPrintStr = String;
        }

        // channel number
        UI_PrintStringSmallBold(pPrintStr, menu_item_x1, menu_item_x2, 2);

        SETTINGS_FetchChannelName(String, gSubMenuSelection);
        pPrintStr = String[0] ? String : "--";

        // channel name
        // (scan list priority display removed - Robzyl uses single list system)
        UI_PrintStringSmallBold(pPrintStr, menu_item_x1, menu_item_x2, 3);
    }

    if ((UI_MENU_GetCurrentMenuId() == MENU_R_CTCS || UI_MENU_GetCurrentMenuId() == MENU_R_DCS) && gCssBackgroundScan)
        UI_PrintStringSmallBold("SCAN", menu_item_x1, menu_item_x2, 4);

#ifdef ENABLE_DTMF_CALLING
    if (UI_MENU_GetCurrentMenuId() == MENU_D_LIST && gIsDtmfContactValid) {
        Contact[11] = 0;
        memcpy(&gDTMF_ID, Contact + 8, 4);
        sprintf(String, "ID:%4s", gDTMF_ID);
        UI_PrintStringSmallBold(String, menu_item_x1, menu_item_x2, 4);
    }
#endif

    if (UI_MENU_GetCurrentMenuId() == MENU_R_CTCS ||
        UI_MENU_GetCurrentMenuId() == MENU_T_CTCS ||
        UI_MENU_GetCurrentMenuId() == MENU_R_DCS  ||
        UI_MENU_GetCurrentMenuId() == MENU_T_DCS
#ifdef ENABLE_DTMF_CALLING
        || UI_MENU_GetCurrentMenuId() == MENU_D_LIST
#endif
    ) {

    //      // ОТДЕЛЬНЫЕ НАСТРОЙКИ ДЛЯ X/Y — МЕНЯЙ ЗДЕСЬ
    // uint8_t base_x = 90;  // Базовый X (центр экрана, можно менять)
    // uint8_t y_pos  = 50;   // Y=0 (верхняя строка, меняй на 1, 2 и т.д.)

    //     sprintf(String, "%2d", gSubMenuSelection);
    // uint8_t text_len = strlen(String);
    // uint8_t text_width = text_len * 4;  // ширина символа в GUI_DisplaySmallestDark — 4 пикселя
    // uint8_t x_pos = base_x - (text_width / 2);  // центр относительно base_x

    // Выводим dark-шрифтом (самый маленький)
   // GUI_DisplaySmallestDark(String, x_pos, y_pos, false, true);
  // UI_PrintStringSmallBold(String, 90, 0, 6);


    }

    if ((UI_MENU_GetCurrentMenuId() == MENU_RESET    ||
         UI_MENU_GetCurrentMenuId() == MENU_MEM_CH   ||
         UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME ||
         UI_MENU_GetCurrentMenuId() == MENU_DEL_CH) && gAskForConfirmation)
    {     
        char *pPrintStr = (gAskForConfirmation == 1) ? "SURE?" : "WAIT!";
        UI_PrintStringSmallBold(pPrintStr, menu_item_x1, menu_item_x2, 4);
    }

//*******************ЛИНИИ-LINES***************** */

        for (uint8_t y = 4; y <= 57; y += 2) {
            UI_DrawLineBuffer(gFrameBuffer, 49, y, 49, y, 1); // Левая вертикальная пунктирная(X = 30)
        }
    
        for (uint8_t i = 0; i <= 127; i += 2) {
            UI_DrawLineBuffer(gFrameBuffer, i, 2, i, 2, 1); // Hory X
        }

        for (uint8_t i = 0; i <= 47; i += 2) {
            UI_DrawLineBuffer(gFrameBuffer, i, 44, i, 44, 1); // Hory X
        }

        for (uint8_t i = 51; i <= 127; i += 2) {
            UI_DrawLineBuffer(gFrameBuffer, i, 44, i, 44, 1); // Hory X
        }
  GUI_DisplaySmallestDark(" DEV:OURO.SU", 55, 48, false, true);

    ST7565_BlitFullScreen();
}