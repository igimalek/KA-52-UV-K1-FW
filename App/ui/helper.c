//H
#include <string.h>

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "misc.h"
#include "settings.h"


void UI_GenerateChannelString(char *pString, const uint16_t Channel)
{
    unsigned int i;

    if (gInputBoxIndex == 0)
    {
        sprintf(pString, "CH-%02u", Channel + 1);
        return;
    }

    pString[0] = 'C';
    pString[1] = 'H';
    pString[2] = '-';
    for (i = 0; i < 2; i++)
        pString[i + 3] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
}

void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint16_t ChannelNumber)
{
    if (gInputBoxIndex > 0) {
        for (unsigned int i = 0; i < 4; i++) {
            pString[i] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
        }

        pString[4] = 0;
        return;
    }

    if (bShowPrefix) {
        // BUG here? Prefixed NULLs are allowed
        sprintf(pString, "CH-%04u", ChannelNumber + 1);
    } else if (ChannelNumber == MR_CHANNEL_LAST + 1) {
        strcpy(pString, "None");
    } else if (ChannelNumber == 0xFFFF) {
        strcpy(pString, "NULL");
    } else {
        sprintf(pString, "%04u", ChannelNumber + 1);
    }
}

void UI_PrintStringBuffer(const char *pString, uint8_t * buffer, uint32_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;
    for (size_t i = 0; i < Length; i++) {
        const unsigned int index = pString[i] - ' ' - 1;
        if (pString[i] > ' ' && pString[i] < 127) {
            const uint32_t offset = i * char_spacing + 1;
            memcpy(buffer + offset, font + index * char_width, char_width);
        }
    }
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
    size_t i;
    size_t Length = strlen(pString);

    if (End > Start)
        Start += (((End - Start) - (Length * Width)) + 1) / 2;

    for (i = 0; i < Length; i++)
    {
        const unsigned int ofs   = (unsigned int)Start + (i * Width);
        if (pString[i] > ' ' && pString[i] < 127)
        {
            const unsigned int index = pString[i] - ' ' - 1;
            memcpy(gFrameBuffer[Line + 0] + ofs, &gFontBig[index][0], 7);
            memcpy(gFrameBuffer[Line + 1] + ofs, &gFontBig[index][7], 7);
        }
    }
}

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;

    if (End > Start) {
        Start += (((End - Start) - Length * char_spacing) + 1) / 2;
    }

    UI_PrintStringBuffer(pString, gFrameBuffer[Line] + Start, char_width, font);
}


void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    UI_PrintStringSmall(pString, Start, End, Line, ARRAY_SIZE(gFontSmall[0]), (const uint8_t *)gFontSmall);
}

void UI_PrintStringSmallNormalInverse(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    // First draw the string normally
    UI_PrintStringSmallNormal(pString, Start, End, Line);

    // Now invert the framebuffer bits for the rendered area
    uint8_t len = strlen(pString);
    uint8_t char_width = 7; // small font is typically 6px wide

    uint8_t x_start = Start;
    uint8_t x_end   = Start + (len * char_width) + 1;

    if (End != 0 && x_end > End)
        x_end = End;

    //gFrameBuffer[Line][x_start - 2] ^= 0x3E;
    gFrameBuffer[Line][x_start - 1] ^= 0x7F;
    //gFrameBuffer[Line][x_start - 1] ^= 0xFF;
    for (uint8_t x = x_start; x < x_end; x++)
    {
        gFrameBuffer[Line][x] ^= 0xFF;
        gFrameBuffer[Line - 1][x] ^= 0x80;
    }
    //gFrameBuffer[Line][x_end + 0] ^= 0xFF;
    gFrameBuffer[Line][x_end + 0] ^= 0x7F;
    //gFrameBuffer[Line][x_end + 1] ^= 0x3E;
}


void UI_PrintStringSmallbackground(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t background)
{
    if (pString == NULL)
        return;

    const size_t Length = strlen(pString);
    const unsigned int char_width  = ARRAY_SIZE(gFontSmall[0]);
    const unsigned int spacing     = 1;
    const unsigned int space_width = 4;

    if (Line >= ARRAY_SIZE(gFrameBuffer))
        return;

    size_t start_pos = (size_t)Start;
    size_t end_pos   = (size_t)End;

    // Legacy compatibility:
    // old UI code (main/VFO) uses Start = LCD_WIDTH + x, End = 0
    // meaning "draw starting at x without centering"
    const bool legacy_absolute_mode = (End == 0 && Start >= LCD_WIDTH);
    if (legacy_absolute_mode)
        start_pos -= LCD_WIDTH;

    // Calculate actual visible text width
    size_t text_width = 0;
    for (size_t i = 0; i < Length; i++) {
        const char c = pString[i];

        if (c > ' ') {
            const unsigned int index = (unsigned int)c - ' ' - 1;
            if (index < ARRAY_SIZE(gFontSmall)) {
                unsigned int char_width_used = char_width;
                while (char_width_used > 0 && gFontSmall[index][char_width_used - 1] == 0)
                    char_width_used--;

                text_width += char_width_used;
                if (i + 1 < Length)
                    text_width += spacing;
            }
        } else {
            text_width += space_width;
        }
    }

    // Center only when a real [Start..End] range is provided,
    // not in legacy absolute mode
    if (!legacy_absolute_mode && end_pos > start_pos) {
        const size_t available = end_pos - start_pos;
        if (available > text_width)
            start_pos += (available - text_width + 1) / 2;
    }

    if (start_pos >= LCD_WIDTH)
        return;

    size_t draw_limit = LCD_WIDTH;
    if (!legacy_absolute_mode && end_pos > start_pos && end_pos < draw_limit)
        draw_limit = end_pos;

    uint8_t *line_buf = gFrameBuffer[Line];

    // Fill background only under the text area
    if (background) {
        size_t fill_width = text_width;
        if (start_pos + fill_width > draw_limit)
            fill_width = draw_limit - start_pos;

        memset(line_buf + start_pos, 0xFF, fill_width);
    }

    size_t cursor_pos = start_pos;

    for (size_t i = 0; i < Length; i++)
    {
        const char c = pString[i];

        if (c > ' ')
        {
            const unsigned int index = (unsigned int)c - ' ' - 1;
            if (index < ARRAY_SIZE(gFontSmall))
            {
                unsigned int char_width_used = char_width;
                while (char_width_used > 0 && gFontSmall[index][char_width_used - 1] == 0)
                    char_width_used--;

                if (cursor_pos < draw_limit) {
                    size_t writable = char_width_used;
                    if (cursor_pos + writable > draw_limit)
                        writable = draw_limit - cursor_pos;

                    switch (background) {
                        case 0:
                            memmove(line_buf + cursor_pos, gFontSmall[index], writable);
                            break;

                        case 1:
                            for (size_t j = 0; j < writable; j++)
                                line_buf[cursor_pos + j] = (uint8_t)~gFontSmall[index][j];
                            break;

                        default:
                            memmove(line_buf + cursor_pos, gFontSmall[index], writable);
                            break;
                    }
                }

                cursor_pos += char_width_used;

                if (i + 1 < Length)
                    cursor_pos += spacing;
            }
        }
        else
        {
            cursor_pos += space_width;
        }

        if (cursor_pos >= draw_limit)
            break;
    }
}

void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif

    UI_PrintStringSmall(pString, Start, End, Line, char_width, font);
}

// Жирный шрифт инвертированный - сначала рисуем, потом XOR
void UI_PrintStringSmallBoldInverse(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    if (pString == NULL) return;
    if (Line >= ARRAY_SIZE(gFrameBuffer)) return;

    // Сначала рисуем жирный текст обычно
    UI_PrintStringSmallBold(pString, Start, End, Line);

    // Вычисляем ширину текста
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
    const size_t  Length     = strlen(pString);
    size_t        text_width = 0;
    for (size_t i = 0; i < Length; i++) {
        char c = pString[i];
        if (c > ' ') {
            unsigned int idx = (unsigned int)c - ' ' - 1;
            if (idx < ARRAY_SIZE(gFontSmallBold)) {
                unsigned int w = char_width;
                while (w > 0 && gFontSmallBold[idx][w-1] == 0) w--;
                text_width += w + (i + 1 < Length ? 1 : 0);
            }
        } else {
            text_width += 4;
        }
    }

    uint8_t x_start = Start;
    uint8_t x_end   = (uint8_t)((size_t)x_start + text_width);
    if (x_end >= LCD_WIDTH) x_end = LCD_WIDTH - 1;

    // XOR всей строки = инвертируем нарисованный текст
    for (uint8_t x = x_start; x <= x_end; x++)
        gFrameBuffer[Line][x] ^= 0xFF;
}

void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t * buffer)
{
    UI_PrintStringBuffer(pString, buffer, ARRAY_SIZE(gFontSmall[0]), (uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif
    UI_PrintStringBuffer(pString, buffer, char_width, font);
}

void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    uint8_t len = strlen(string);
    for(int i = 0; i < len; i++) {
        char c = string[i];
        if(c=='-') c = '9' + 1;
        if (bCanDisplay || c != ' ')
        {
            bCanDisplay = true;
            if(c>='0' && c<='9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c-'0'],                  char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c-'0'] + char_width - 3, char_width - 3);
            }
            else if(c=='.') {
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                continue;
            }

        }
        else if (center) {
            pFb0 -= 6;
            pFb1 -= 6;
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}

/*
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    if (center) {
        uint8_t len = 0;
        for (const char *ptr = string; *ptr; ptr++)
            if (*ptr != ' ') len++; // Ignores spaces for centering

        X -= (len * char_width) / 2; // Centering adjustment
        pFb0 = gFrameBuffer[Y] + X;
        pFb1 = pFb0 + 128;
    }

    for (; *string; string++) {
        char c = *string;
        if (c == '-') c = '9' + 1; // Remap of '-' symbol

        if (bCanDisplay || c != ' ') {
            bCanDisplay = true;
            if (c >= '0' && c <= '9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c - '0'], char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c - '0'] + char_width - 3, char_width - 3);
            } else if (c == '.') {
                memset(pFb1, 0x60, 3); // Replaces the three assignments
                pFb0 += 3;
                pFb1 += 3;
                continue;
            }
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}
*/

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black)
{
    const uint8_t pattern = 1 << (y % 8);
    if(black)
        buffer[y/8][x] |= pattern;
    else
        buffer[y/8][x] &= ~pattern;
}

static void sort(int16_t *a, int16_t *b)
{
    if(*a > *b) {
        int16_t t = *a;
        *a = *b;
        *b = t;
    }
}

#ifdef ENABLE_FEAT_F4HWN
    /*
    void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
    {
        if(x2==x1) {
            sort(&y1, &y2);
            for(int16_t i = y1; i <= y2; i+=2) {
                UI_DrawPixelBuffer(buffer, x1, i, black);
            }
        } else {
            const int multipl = 1000;
            int a = (y2-y1)*multipl / (x2-x1);
            int b = y1 - a * x1 / multipl;

            sort(&x1, &x2);
            for(int i = x1; i<= x2; i+=2)
            {
                UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
            }
        }
    }
    */

    void PutPixel(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(gFrameBuffer, x, y, fill);
    }

    void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(&gStatusLine, x, y, fill);
    }

    void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                    bool statusbar, bool fill) {
      uint8_t c;
      uint8_t pixels;
      const uint8_t *p = (const uint8_t *)pString;

      while ((c = *p++) && c != '\0') {
        c -= 0x20;
        for (int i = 0; i < 3; ++i) {
          pixels = gFont3x5[c][i];
          for (int j = 0; j < 6; ++j) {
            if (pixels & 1) {
              if (statusbar)
                PutPixelStatus(x + i, y + j, fill);
              else
                PutPixel(x + i, y + j, fill);
            }
            pixels >>= 1;
          }
        }
        x += 4;
      }
    }
    void UI_DisplayUnlockKeyboard(uint8_t shift) {
        if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
        {   // tell user how to unlock the keyboard
            
            //memcpy(gFrameBuffer[shift] + 2, gFontKeyLock, sizeof(gFontKeyLock));
            UI_PrintStringSmallBold("UNLOCK KEYBOARD", 12, 0, shift);
            //memcpy(gFrameBuffer[shift] + 120, gFontKeyLock, sizeof(gFontKeyLock));

            /*
            for (uint8_t i = 12; i < 116; i++)
            {
                gFrameBuffer[shift][i] ^= 0xFF;
            }
            */
        }
    }

    bool IsEmptyName(const char *name, uint8_t len) {
        if (name[0] == '\0' || name[0] == '\xff')
            return true;
        for (uint8_t i = 0; i < len; i++) {
            if (name[i] != ' ' && name[i] != '\xff' && name[i] != '\0')
                return false;
        }
        return true;
    }
#endif
    
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    if(x2==x1) {
        sort(&y1, &y2);
        for(int16_t i = y1; i <= y2; i++) {
            UI_DrawPixelBuffer(buffer, x1, i, black);
        }
    } else {
        const int multipl = 1000;
        int a = (y2-y1)*multipl / (x2-x1);
        int b = y1 - a * x1 / multipl;

        sort(&x1, &x2);
        for(int i = x1; i<= x2; i++)
        {
            UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
        }
    }
}

void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    UI_DrawLineBuffer(buffer, x1,y1, x1,y2, black);
    UI_DrawLineBuffer(buffer, x1,y1, x2,y1, black);
    UI_DrawLineBuffer(buffer, x2,y1, x2,y2, black);
    UI_DrawLineBuffer(buffer, x1,y2, x2,y2, black);
}

void UI_DisplayPopup(const char *string)
{
    for(uint8_t i = 2; i < 4; i++) {
        memset(gFrameBuffer[i], 0x00, 128);
    }
    UI_PrintString(string, 12, 116, 2, 8);
    for (uint8_t x = 0; x < 128; x++) {
        gFrameBuffer[2][x] ^= 0xFF;
        gFrameBuffer[3][x] ^= 0xFF;}

}

void UI_DisplayClear()
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
}

/***********ИНВЕРСИЯ МЕЛКОГО ТЕКСТА**********INVERSION FONT SMALL**********************************/
// wide_spacing = true: 6 px
// wide_spacing = false: 4 px
void GUI_DisplaySmallestDark(const char *pString, uint8_t x, uint8_t y, bool statusbar, bool wide_spacing)
{
    if (!pString || !*pString) return;

    const uint8_t char_height = 6;
    const uint8_t char_width = wide_spacing ? 6 : 4;

    uint8_t base_x = x;
    uint8_t end_x = x;

    uint8_t c;
    const uint8_t *p = (const uint8_t *)pString;

    while ((c = *p++) != '\0')
    {
        if (c < 0x20) {
            end_x += char_width;
            continue;
        }

        c -= 0x20;

        // Линия сверху 
        if (y > 0)
        {
            for (uint8_t dx = 0; dx < char_width; dx++)
            {
                if (statusbar)
                    PutPixelStatus(end_x + dx, y - 1, true);
                else
                    PutPixel(end_x + dx, y - 1, true);
            }
        }

        // Чёрный фон
        for (uint8_t dy = 0; dy < char_height; dy++)
        {
            for (uint8_t dx = 0; dx < char_width; dx++)
            {
                if (statusbar)
                    PutPixelStatus(end_x + dx, y + dy, true);
                else
                    PutPixel(end_x + dx, y + dy, true);
            }
        }

        // Белые буквы
        const uint8_t *glyph = gFont3x5[c];
        for (uint8_t col = 0; col < 3; col++)
        {
            uint8_t pixels = glyph[col];
            for (uint8_t row = 0; row < 6; row++)
            {
                if (pixels & 1)
                {
                    uint8_t offset = wide_spacing ? 1 : 0;
                    if (statusbar)
                        PutPixelStatus(end_x + col + offset, y + row, false);
                    else
                        PutPixel(end_x + col + offset, y + row, false);
                }
                pixels >>= 1;
            }
        }

        end_x += char_width;
    }

    // Вертикальные линии — в обоих режимах две слева, одна справа
    for (uint8_t dy = 0; dy <= char_height; dy++)
    {
        uint8_t line_y = y + dy - 1;
        if (line_y < 64)
        {
            // Две линии слева
            if (base_x >= 2)
            {
                if (statusbar)
                    PutPixelStatus(base_x - 2, line_y, true);
                else
                    PutPixel(base_x - 2, line_y, true);
            }
            if (base_x >= 1)
            {
                if (statusbar)
                    PutPixelStatus(base_x - 1, line_y, true);
                else
                    PutPixel(base_x - 1, line_y, true);
            }

            // Линия справа
            if (end_x < 128)
            {
                if (statusbar)
                    PutPixelStatus(end_x, line_y, true);
                else
                    PutPixel(end_x, line_y, true);
            }
        }
    }
}
