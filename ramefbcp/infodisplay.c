/* Copyright 2015,2016 rameplayerorg
 * Licensed under GPLv2, which you must read from the included LICENSE file.
 *
 * Info display code for part of the LCD screen (secondary framebuffer).
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include "infodisplay.h"
#include "icon-data.h"
#include "ttf.h"
#include "debug.h"


static const int infodisplay_icon_text_horiz_gap = 2;
static const int infodisplay_progress_bar_height = 2;

static const unsigned long play_bar_color_32 = 0xfff12b24;
static const unsigned short play_bar_color_16_565 = 0xf144;

static const int animation_cycle_length_ms = 1000;

static const float scroll_speed_pix_per_s = 20.0f;
static const float scroll_startpoint_delay_s = 1.0f;
static const float scroll_endpoint_delay_s = 2.0f;


static int mini(int a, int b)
{
    return a < b ? a : b;
}

static int maxi(int a, int b)
{
    return a > b ? a : b;
}

static float minf(float a, float b)
{
    return a < b ? a : b;
}

static float maxf(float a, float b)
{
    return a > b ? a : b;
}

static float clampf(float value, float min_val, float max_val)
{
    return maxf(min_val, minf(value, max_val));
}

static float stepf(float value, float step_pos)
{
    return value < step_pos ? 0.0f : 1.0f;
}

static float boxstepf(float value, float slope_start, float slope_end)
{
    float diff = slope_end - slope_start;
    if (diff == 0)
        return stepf(value, slope_start);
    return clampf((value - slope_start) / diff, 0.0f, 1.0f);
}

static float boxpulsef(float value,
                       float up_slope_start, float up_slope_end,
                       float down_slope_start, float down_slope_end)
{
    return boxstepf(value,   up_slope_start,   up_slope_end)
         - boxstepf(value, down_slope_start, down_slope_end);
}



// Clipped blit from grayscale 8bpp source to 16bpp target,
// mixing with target pixels using OR operation.
static void blit_8_or(INFODISPLAY *disp,                        // target display
                      int clip_top_left_x, int clip_top_left_y, // target clip rect top-left
                      int clip_width, int clip_height,          // target clip rect size
                      int top_left_x, int top_left_y,           // target top-left coordinate
                      const unsigned char * restrict src,       // 8bpp grayscale source buffer
                      int src_width, int src_height,            // source rect size
                      int src_pitch)                            // source buffer pitch
{
    const int offs_r = disp->offs_r, bits_r = disp->bits_r;
    const int offs_g = disp->offs_g, bits_g = disp->bits_g;
    const int offs_b = disp->offs_b, bits_b = disp->bits_b;
    const int offs_a = disp->offs_a, bits_a = disp->bits_a;
    const int bits_lum = 8;
    PIXEL * restrict dest = disp->backbuf;
    const int dest_pitch = disp->width;

    // clip cliprect against disp size
    // result: top-left inclusive; bottom-right exclusive
    int clip_left = maxi(0, clip_top_left_x);
    int clip_top = maxi(0, clip_top_left_y);
    int clip_right = mini(disp->width, clip_top_left_x + clip_width);
    int clip_bottom = mini(disp->height, clip_top_left_y + clip_height);
    if (clip_right <= clip_left || clip_bottom <= clip_top)
        return; // empty target clip rect
    // NB! now invalid: clip_top_left_x clip_top_left_y clip_width clip_height

    // clip target pos and source size against the clip rect
    // result: top-left inclusive; bottom-right exclusive
    int target_left = maxi(top_left_x, clip_left);
    int target_top = maxi(top_left_y, clip_top);
    int target_right = mini(top_left_x + src_width, clip_right);
    int target_bottom = mini(top_left_y + src_height, clip_bottom);
    if (target_right <= target_left || target_bottom <= target_top)
        return; // empty result drawing rect

    // adjust source buffer offset to match clipped top-left part
    int src_offs = 0;
    if (target_left > top_left_x)
        src_offs += target_left - top_left_x;
    if (target_top > top_left_y)
        src_offs += (target_top - top_left_y) * src_pitch;

    int dest_offs = target_top * dest_pitch + target_left;

    for (int y = target_top; y < target_bottom; ++y)
    {
        PIXEL * restrict destpx = dest + dest_offs;
        const unsigned char * restrict srcpx = src + src_offs;
        for (int x = target_left; x < target_right; ++x)
        {
            PIXEL pix = 0;
            const unsigned char lum = *srcpx;
            pix |= (lum >> (bits_lum - bits_r)) << offs_r;
            pix |= (lum >> (bits_lum - bits_g)) << offs_g;
            pix |= (lum >> (bits_lum - bits_b)) << offs_b;
            pix |= (0xff >> (8 - bits_a)) << offs_a;
            *destpx |= pix;
            ++destpx;
            ++srcpx;
        }
        dest_offs += dest_pitch;
        src_offs += src_pitch;
    }
}


/*
static int get_text_size(INFODISPLAY *disp, const char *text,
                         int *width, int *height)
{
    if (width == NULL || height == NULL)
        return -1;
    *width = *height = 0;
    if (text == NULL)
        return -1;
    return TTF_SizeUTF8(disp->font, text, width, height);
}
*/

static void draw_text_to_row_textsurf(INFODISPLAY *disp, int info_row,
                                      const char *text)
{
    int width, height;

    if (disp == NULL)
        return; // error
    if (text == NULL || text[0] == 0)
        return; // skip null or empty input text

    disp->info_row_text_width[info_row] = 0;

    if (TTF_SizeUTF8(disp->font, text, &width, &height) < 0
        || width == 0 || height == 0)
    {
        return; // empty result
    }

    if (disp->info_row_textsurf[info_row] == NULL ||
        disp->info_row_textsurf[info_row]->w < width ||
        disp->info_row_textsurf[info_row]->h < height)
    {
        // reallocate larger work area, using max width&height of earlier and new
        if (disp->info_row_textsurf[info_row] != NULL)
        {
            width = maxi(disp->info_row_textsurf[info_row]->w, width);
            height = maxi(disp->info_row_textsurf[info_row]->h, height);
        }
        TTF_FreeSurface(disp->info_row_textsurf[info_row]);
        disp->info_row_textsurf[info_row] = TTF_CreateSurface(width, height);
    }
    else
        TTF_ClearSurface(disp->info_row_textsurf[info_row]);

    if (disp->info_row_textsurf[info_row] == NULL)
        return; // error

    TTF_RenderUTF8_Shaded_Surface(disp->info_row_textsurf[info_row], disp->font, text);
    disp->info_row_text_width[info_row] = width;
}

static void blit_row_textsurf(INFODISPLAY *disp, int info_row, int dx, int dy,
                              int clip_top_left_x, int clip_top_left_y,
                              int clip_width, int clip_height)
{
    if (disp == NULL || disp->info_row_textsurf[info_row] == NULL)
        return; // error
    TTF_Surface *surf = disp->info_row_textsurf[info_row];
    int width = surf->w, height = surf->h;
    blit_8_or(disp,                                      // target & clip rect:
              clip_top_left_x, clip_top_left_y, clip_width, clip_height,
              dx, dy,                                    // target pos
              surf->pixels, width, height, surf->pitch); // src data, size and pitch
}


// Custom icon drawing (from 8bpp grayscale to 16bpp destination buffer).
// Note: No support for fb_var_screeninfo rgb msb_right!=0.
static void draw_icon(INFODISPLAY *disp, int dx, int dy, unsigned char *icon)
{
    blit_8_or(disp, 0, 0, disp->width, disp->height,      // target & clip rect
              dx, dy,                                     // target pos
              icon, ICON_WIDTH, ICON_HEIGHT, ICON_WIDTH); // src data, size and pitch
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
    free(disp->backbuf);
    for (int a = 0; a < INFODISPLAY_ROW_COUNT; ++a)
    {
        free(disp->info_rows[a]);
        TTF_FreeSurface(disp->info_row_textsurf[a]);
    }
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
        disp->info_row_scroll_time_s[row] = 0;

        draw_text_to_row_textsurf(disp, row, disp->info_rows[row]);

        // after moving rendering here, we technically wouldn't need to store
        // the string itself anymore. (but it's kept in the struct for now)
    }
}

// reset scroll position of row
void infodisplay_reset_row_scroll(INFODISPLAY *disp, int row)
{
    if (row < 0 || row >= INFODISPLAY_ROW_COUNT)
    {
        #ifdef DEBUG_SUPPORT
        dbg_printf("infodisplay_reset_row_scroll: Invalid row number %d\n", row);
        #endif
        return;
    }
    disp->info_row_scroll_time_s[row] = 0;
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

// Renders the current display state to the backbuffer (disp->backbuf).
// If ret_req!=NULL, writes value to it where non-zero value means request for
// new refresh on next available frame (instead of waiting for outside event).
void infodisplay_update(INFODISPLAY *disp, int *ret_req)
{
    int y, progress_bar_y = 0;
    struct timeval tv;
    int anim_time_ms = 0;
    int req_refresh = 0;
    float anim_time_delta_s = 0;

    if (ret_req != NULL)
        *ret_req = 0;

    if (disp == NULL || disp->backbuf == NULL)
        return;

    if (gettimeofday(&tv, NULL) == 0)
    {
        int prev_anim_time_ms = disp->prev_anim_time_ms;
        // anim_time_ms is meant for overall tracking of passing time for very
        // simple animation (starts & periodically restarts from 0)
        if (s_start_time_sec == 0 ||
            (tv.tv_sec - s_start_time_sec) > 24 * 60 * 60) // rough reset once per day
        {
            s_start_time_sec = tv.tv_sec;
            prev_anim_time_ms = 0;
        }
        anim_time_ms = (tv.tv_sec - s_start_time_sec) * 1000 + tv.tv_usec / 1000;
        if (prev_anim_time_ms == 0)
        {
            // prev_anim_time_ms is set to 0 either above,
            // or when not requesting constant refresh (and needs
            // to be set to 0, otherwise it falls too much behind)
            prev_anim_time_ms = anim_time_ms;
        }
        int anim_time_delta_ms = anim_time_ms - prev_anim_time_ms;
        anim_time_delta_s = anim_time_delta_ms / 1000.0f;
        disp->prev_anim_time_ms = anim_time_ms;
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
            //const char *text = disp->info_rows[row];
            int rem_horiz_space = disp->width - x;
            int tx = x, ty = y, tw; // scrolling text pos and width
            tw = disp->info_row_text_width[row];

            if (tw > 0)
            {
                //int th = mini(disp->info_row_textsurf[row]->h, disp->row_height);

                if (tw > rem_horiz_space)
                {
                    float anim_time_s = disp->info_row_scroll_time_s[row];
                    int scroll_length_pix = tw - rem_horiz_space;
                    float scroll_length_s = scroll_length_pix / scroll_speed_pix_per_s;
                    float period_length_s = 2.0f * scroll_length_s +
                        scroll_startpoint_delay_s + scroll_endpoint_delay_s;
                    float scroll_time_s = fmodf(anim_time_s, period_length_s);
                    // v Period:      
                    // |     _c_      .
                    // |    /   \     .
                    // |_a_/b   d\    .
                    // +------------t .
                    // a = startpoint delay
                    // b = scroll forwards
                    // c = endpoint delay
                    // d = scroll backwards
                    // uss = start of scroll forwards slope b (end of a)
                    // use = end of scroll forwards slope b (start of c)
                    // dss = start of scroll backwards slope d (end of c)
                    // dse = end of scroll backdwards slope d (start of a, wrap)
                    float scroll_uss = scroll_startpoint_delay_s;
                    float scroll_use = scroll_uss + scroll_length_s;
                    float scroll_dss = scroll_use + scroll_endpoint_delay_s;
                    float scroll_dse = scroll_dss + scroll_length_s;
                    float scroll_val = boxpulsef(scroll_time_s,
                                                 scroll_uss, scroll_use,
                                                 scroll_dss, scroll_dse);
                    tx = (int)(x - scroll_val * scroll_length_pix);

                    disp->info_row_scroll_time_s[row] += anim_time_delta_s;
                    req_refresh = 1; // scrolling row, need constant refresh
                }
                
                blit_row_textsurf(disp, row, tx, ty,     // target, row #, text pos
                                  x, y, rem_horiz_space, disp->row_height); // clip rect
            } // tw > 0
        } // text on row != NULL

        y += disp->row_height;
    }

    if (disp->info_progress > 0)
        draw_progress(disp, progress_bar_y);

    if (!req_refresh)
        disp->prev_anim_time_ms = 0; // reset anim time delta (unknown time until next refresh)

    if (ret_req != NULL)
        *ret_req |= req_refresh;
}
