/* Copyright 2015 rameplayerorg
 * Licensed under GPLv2, which you must read from the included LICENSE file.
 *
 * Info display code for part of the LCD screen (secondary framebuffer).
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "infodisplay.h"
#include "icon-data.h"
#include "ttf.h"
#include "debug.h"


static const int infodisplay_icon_text_horiz_gap = 2;
static const int infodisplay_progress_bar_height = 2;

static const unsigned long play_bar_color_32 = 0xfff12b24;
static const unsigned short play_bar_color_16_565 = 0xf144;

static const int animation_cycle_length_ms = 1000;


// TODO: Implement proper clipping, now there's just some rudimentary check for rect size but dx,dy must stay within screen
static void draw_text(INFODISPLAY *disp, int dx, int dy, char *text)
{
    int width, height;

    if (text == NULL || text[0] == 0)
        return; // skip null or empty input text

    if (TTF_SizeUTF8(disp->font, text, &width, &height) < 0 || width == 0 || height == 0)
        return; // empty result

    if (disp->textsurf == NULL ||
        disp->textsurf->w < width ||
        disp->textsurf->h < height)
    {
        // reallocate larger work area, using max width&height of earlier and new
        if (disp->textsurf != NULL)
        {
            width = disp->textsurf->w > width ? disp->textsurf->w : width;
            height = disp->textsurf->h > height ? disp->textsurf->h : height;
        }
        TTF_FreeSurface(disp->textsurf);
        disp->textsurf = TTF_CreateSurface(width, height);
        //printf("allocated text surface: %d,%d,%d\n", disp->textsurf->w, disp->textsurf->h, disp->textsurf->pitch);
    }
    else
        TTF_ClearSurface(disp->textsurf);

    if (disp->textsurf == NULL)
        return; // error

    TTF_RenderUTF8_Shaded_Surface(disp->textsurf, disp->font, text);
    TTF_Surface *surf = disp->textsurf;

    const int offs_r = disp->offs_r, bits_r = disp->bits_r;
    const int offs_g = disp->offs_g, bits_g = disp->bits_g;
    const int offs_b = disp->offs_b, bits_b = disp->bits_b;
    const int offs_a = disp->offs_a, bits_a = disp->bits_a;
    const int bits_lum = 8;
    PIXEL * restrict dest = disp->backbuf;
    const char * restrict src = (const char *)surf->pixels;

    // TODO: proper clipping
    if (dx + width > disp->width)
        width -= dx + width - disp->width;
    if (dy + height > disp->height)
        height -= dy + height - disp->height;

    const int src_w = width, src_h = height;

    int src_row_offs = 0;
    int dest_row_offs = dy * disp->width;
    
    for (int y = 0; y < src_h; ++y)
    {
        int dest_offs = dest_row_offs + dx;
        int src_offs = src_row_offs;
        for (int x = 0; x < src_w; ++x)
        {
            PIXEL16 pix = 0;
            const unsigned char lum = src[src_offs];
            pix |= (lum >> (bits_lum - bits_r)) << offs_r;
            pix |= (lum >> (bits_lum - bits_g)) << offs_g;
            pix |= (lum >> (bits_lum - bits_b)) << offs_b;
            pix |= (0xff >> (8 - bits_a)) << offs_a;
            dest[dest_offs] |= pix;
            ++dest_offs;
            ++src_offs;
        }
        dest_row_offs += disp->width;
        src_row_offs += surf->pitch;
    }
}


// Hacky hardcoded way to draw icons defined in 8bpp arrays.
// Warning: No clipping!
// Note: No support for fb_var_screeninfo rgb msb_right!=0.
static void draw_icon(INFODISPLAY *disp, int dx, int dy, unsigned char *icon)
{
    const int offs_r = disp->offs_r, bits_r = disp->bits_r;
    const int offs_g = disp->offs_g, bits_g = disp->bits_g;
    const int offs_b = disp->offs_b, bits_b = disp->bits_b;
    const int offs_a = disp->offs_a, bits_a = disp->bits_a;
    const int bits_lum = 8;
    PIXEL * restrict backbuf = disp->backbuf;

    int src_offs = 0;
    int dest_row_offs = dy * disp->width;
    for (int y = 0; y < ICON_HEIGHT; ++y)
    {
        int dest_offs = dest_row_offs + dx;
        for (int x = 0; x < ICON_WIDTH; ++x)
        {
            PIXEL16 pix = 0;
            const unsigned char lum = icon[src_offs];
            pix |= (lum >> (bits_lum - bits_r)) << offs_r;
            pix |= (lum >> (bits_lum - bits_g)) << offs_g;
            pix |= (lum >> (bits_lum - bits_b)) << offs_b;
            pix |= (0xff >> (8 - bits_a)) << offs_a;
            backbuf[dest_offs] = pix;
            ++dest_offs;
            ++src_offs;
        }
        dest_row_offs += disp->width;
    }
}


// creates and initializes a new infodisplay
INFODISPLAY * infodisplay_create(int width, int height,
                                 int offs_r, int bits_r,
                                 int offs_g, int bits_g,
                                 int offs_b, int bits_b,
                                 int offs_a, int bits_a,
                                 const char *ttf_filename)
{
    INFODISPLAY *disp;

    if (width <= 0 || height <= 0)
    {
        fprintf(stderr, "Invalid infodisplay size: %d,%d\n", width, height);
        return NULL;
    }

    disp = (INFODISPLAY *)calloc(1, sizeof(INFODISPLAY));
    if (disp == NULL)
    {
        fprintf(stderr, "Can't alloc infodisplay\n");
        return NULL;
    }

    disp->backbuf_size = width * height * sizeof(PIXEL);
    disp->backbuf = (PIXEL *)malloc(disp->backbuf_size);
    if (disp->backbuf == NULL)
    {
        fprintf(stderr, "Can't alloc infodisplay backbuf (%d)\n", disp->backbuf_size);
        free(disp);
        return NULL;
    }

    disp->width = width;
    disp->height = height;
    disp->progress_bar_row = INFODISPLAY_DEFAULT_PROGRESS_BAR_ROW;
    disp->progress_bar_height = infodisplay_progress_bar_height;
    disp->row_height = (height - disp->progress_bar_height) / INFODISPLAY_ROW_COUNT;

    disp->offs_r = offs_r;
    disp->offs_g = offs_g;
    disp->offs_b = offs_b;
    disp->offs_a = offs_a;
    disp->bits_r = bits_r;
    disp->bits_g = bits_g;
    disp->bits_b = bits_b;
    disp->bits_a = bits_a;

    if (ttf_filename != NULL)
    do {
        int res = TTF_Init();
        if (res < 0)
        {
            fprintf(stderr, "Couldn't initialize TTF: %s\n", TTF_GetError());
            break;
        }

        int font_pt_size = disp->row_height * 8 / 10; // pt size 80% of row height
        disp->font = TTF_OpenFont(ttf_filename, font_pt_size);
        if (disp->font == NULL)
        {
            fprintf(stderr, "Couldn't load %d pt font from %s: %s\n",
                    font_pt_size, ttf_filename, TTF_GetError());
            TTF_Quit();
            break;
        }

        TTF_SetFontStyle(disp->font, TTF_STYLE_NORMAL);
    } while (0); // end of ttf init

    #ifdef DEBUG_SUPPORT
    dbg_printf("Infodisplay created:\n");
    dbg_printf("- width x height,backbuf_size: %dx%d,%d\n",
               disp->width, disp->height, disp->backbuf_size);
    dbg_printf("- progress_bar_row,height: %d,%d\n",
               disp->progress_bar_row, disp->progress_bar_height);
    dbg_printf("- row_height: %d\n", disp->row_height);
    dbg_printf("- offs_r,g,b,a: %d,%d,%d,%d\n",
               disp->offs_r, disp->offs_g, disp->offs_b, disp->offs_a);
    dbg_printf("- bits_r,g,b,a: %d,%d,%d,%d\n",
               disp->bits_r, disp->bits_g, disp->bits_b, disp->bits_a);
    #endif

    return disp;
}


// closes infodisplay and frees its memory
void infodisplay_close(INFODISPLAY *disp)
{
    if (disp == NULL)
        return;
    if (disp->font != NULL)
        TTF_CloseFont(disp->font);
    TTF_FreeSurface(disp->textsurf);
    free(disp->backbuf);
    for (int a = 0; a < INFODISPLAY_ROW_COUNT; ++a)
        free(disp->info_rows[a]);
    memset(disp, 0, sizeof(INFODISPLAY));
    free(disp);
}


// progress=[0..1]
void infodisplay_set_progress(INFODISPLAY *disp, float progress)
{
    disp->info_progress = progress;
}

// row=[0..INFODISPLAY_ROW_COUNT[, text in UTF8
void infodisplay_set_row_text(INFODISPLAY *disp, int row, const char *text)
{
    #ifdef DEBUG_SUPPORT
    if (row < 0 || row >= INFODISPLAY_ROW_COUNT)
    {
        dbg_printf("infodisplay_set_row_text: Invalid row number %d\n", row);
        return;
    }
    #endif

    if (text == NULL)
        text = ""; // simplify things

    int len = strlen(text);
    int mem = len + 1;
    if (disp->info_row_mem[row] < mem)
    {
        // realloc row to larger amount
        const int mem_padded = (mem & ~0x0f) + 0x10; // pad up to next 16 byte boundary
        char *nr = (char *)realloc(disp->info_rows[row], mem_padded);
        if (nr != NULL)
        {
            disp->info_rows[row] = nr;
            disp->info_row_mem[row] = mem_padded;
        }
        // note: if realloc failed, earlier mem block is still used (if any)
    }

    if (disp->info_row_mem[row] > 0)
    {
        strncpy(disp->info_rows[row], text, disp->info_row_mem[row]);
        disp->info_rows[row][disp->info_row_mem[row] - 1] = 0;
    }
}

// sets row icon
void infodisplay_set_row_icon(INFODISPLAY *disp, int row, INFODISPLAY_ICON icon)
{
    if (row < 0 || row >= INFODISPLAY_ROW_COUNT)
    {
        #ifdef DEBUG_SUPPORT
        dbg_printf("infodisplay_set_row_icon: Invalid row number %d\n", row);
        #endif
        return;
    }
    disp->info_row_icon[row] = icon;
}

// shorthand for formatting given row to given times in [h:]mm:ss.0 / [h:]mm:ss.0 format
void infodisplay_set_row_times(INFODISPLAY *disp, int row, int time1_ms, int time2_ms)
{
    // example string at max length: "-596:31:23.6 / 596:31:23.6" (hhh:mm:ss.0)
    // (effective min/max values due to int32 range)
    char res[27] = { 0 }, tmp[13] = { 0 };
    int times[2] = { time1_ms, time2_ms };
    for (int a = 0; a < 2; ++a)
    {
        const char *sign;
        unsigned int t;
        if (times[a] < 0)
        {
            if (a > 0)
                break; // negative value is only accepted for first one
            t = -times[a];
            sign = "-";
        }
        else
        {
            t = times[a];
            sign = "";
        }
        if (a > 0)
            strcat(res, " / ");
        t /= 100; // t is initially in milliseconds
        int tenths = t % 10;
        t /= 10;
        int seconds = t % 60;
        t /= 60;
        int minutes = t % 60;
        t /= 60;
        int hours = t % 1000;
        if (hours > 0)
        {
            const char *fmt = "%s%d:%02d:%02d.%d";
            sprintf(tmp, fmt, sign, hours, minutes, seconds, tenths);
        }
        else
        {
            const char *fmt = "%s%d:%02d.%d";
            sprintf(tmp, fmt, sign, minutes, seconds, tenths);
        }
        strcat(res, tmp);
    }
    infodisplay_set_row_text(disp, row, res);
}


static void draw_progress(INFODISPLAY *disp, int progress_bar_y)
{
    // progress bar length in pixels (scaled&clamped to width)
    int progress = (int)(disp->info_progress * disp->width);
    if (progress < 0) progress = 0;
    if (progress >= disp->width) progress = disp->width;
    const int h = disp->progress_bar_height;
    PIXEL * restrict dest_row = disp->backbuf + progress_bar_y * disp->width;
    for (int y = 0; y < h; ++y)
    {
        PIXEL * restrict dest = dest_row;
        for (int x = 0; x < progress; ++x)
            *dest++ = play_bar_color_16_565;
        dest_row += disp->width;
    }
}


static time_t s_start_time_sec = 0;

// renders the current display state to the backbuffer (disp->backbuf)
void infodisplay_update(INFODISPLAY *disp)
{
    int y, progress_bar_y = 0;
    struct timeval tv;
    int anim_time_ms = 0;

    if (disp == NULL || disp->backbuf == NULL)
        return;

    if (gettimeofday(&tv, NULL) == 0)
    {
        // anim_time_ms is meant for overall tracking of passing time for very
        // simple animation (starts & periodically restarts from 0)
        if (s_start_time_sec == 0 ||
            (tv.tv_sec - s_start_time_sec) > 24 * 60 * 60) // rough reset once per day
        {
            s_start_time_sec = tv.tv_sec;
        }
        anim_time_ms = (tv.tv_sec - s_start_time_sec) * 1000 + tv.tv_usec / 1000;
    }

    memset(disp->backbuf, 0, disp->backbuf_size);

    y = 0;
    for (int row = 0; row < INFODISPLAY_ROW_COUNT; ++row)
    {
        int x = 0;

        if (disp->progress_bar_row == row)
        {
            // reserve space for progress bar
            progress_bar_y = y;
            y += disp->progress_bar_height;
        }

        if (disp->info_row_icon[row] != INFODISPLAY_ICON_NONE)
        {
            unsigned char *icon = NULL;
            if (disp->info_row_icon[row] == INFODISPLAY_ICON_PLAYING)
                icon = icon_playing;
            else if (disp->info_row_icon[row] == INFODISPLAY_ICON_PAUSED)
                icon = icon_paused;
            else if (disp->info_row_icon[row] == INFODISPLAY_ICON_STOPPED)
                icon = icon_stopped;
            else if (disp->info_row_icon[row] == INFODISPLAY_ICON_BUFFERING)
            {
                int anim_frame = anim_time_ms * ICON_BUFFERING_FRAMES / animation_cycle_length_ms;
                icon = icon_buffering[anim_frame % ICON_BUFFERING_FRAMES];
            }
            else if (disp->info_row_icon[row] == INFODISPLAY_ICON_WAITING)
            {
                int anim_frame = anim_time_ms * ICON_WAITING_FRAMES / animation_cycle_length_ms;
                icon = icon_waiting[anim_frame % ICON_WAITING_FRAMES];
            }
            if (icon != NULL)
            {
                int icon_y = y + (disp->row_height - ICON_HEIGHT) / 2;
                draw_icon(disp, x, icon_y, icon);
            }

            // indent text when icon space is in use
            x += ICON_WIDTH + infodisplay_icon_text_horiz_gap;
        }

        if (disp->info_rows[row] != NULL)
        {
            //printf("%d: %s\n", row, disp->info_rows[row]);
            draw_text(disp, x, y, disp->info_rows[row]);
        }
        y += disp->row_height;
    }

    if (disp->info_progress > 0)
        draw_progress(disp, progress_bar_y);
}
