#ifndef INFODISPLAY_H_INCLUDED
#define INFODISPLAY_H_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif


#define PIXEL16 unsigned short
//#define PIXEL32 unsigned int
typedef PIXEL16 PIXEL; // hardcoded to 16bpp for now, maybe expand format support later

#define INFODISPLAY_ROW_COUNT 7
#define INFODISPLAY_DEFAULT_PROGRESS_BAR_ROW (INFODISPLAY_ROW_COUNT - 2)
#define INFODISPLAY_DEFAULT_PROGRESS_BAR_COLOR ((unsigned long)0xfff12b24)


typedef enum INFODISPLAY_ICON_ENUM
{
    INFODISPLAY_ICON_NONE = 0, // no icon, no reserved space
    INFODISPLAY_ICON_EMPTY, // no icon but reserved space
    INFODISPLAY_ICON_PLAYING,
    INFODISPLAY_ICON_PAUSED,
    INFODISPLAY_ICON_STOPPED,
    INFODISPLAY_ICON_BUFFERING,
    INFODISPLAY_ICON_WAITING,
    INFODISPLAY_ICON_MEMCARD,
    INFODISPLAY_ICON_FOLDER,
    INFODISPLAY_ICON_PLAYLIST,
    INFODISPLAY_ICON_RECORDING,
    INFODISPLAY_ICON_STREAMING,
    INFODISPLAY_ICON_REPEATPLAYING,
    INFODISPLAY_ICON_COUNT //
} INFODISPLAY_ICON;

typedef enum INFODISPLAY_ROW_TYPE_ENUM
{
    INFODISPLAY_ROW_TYPE_TEXT = 0,
    INFODISPLAY_ROW_TYPE_CLOCK,
    INFODISPLAY_ROW_TYPE_COUNT //
} INFODISPLAY_ROW_TYPE;
    
typedef struct _TTF_Font TTF_Font;
typedef struct _TTF_Surface TTF_Surface;

typedef struct _INFODISPLAY
{
    PIXEL *backbuf;
    int backbuf_size;
    int width, height;
    int progress_bar_row; // draw bar above this text row (affects text row y)
    int progress_bar_height;
    unsigned long progress_bar_color;
    int row_height; // amount of pixels per text row
    // rgba offset & bit width inside pixels:
    unsigned char offs_r, bits_r, offs_g, bits_g, offs_b, bits_b, offs_a, bits_a;
    TTF_Font *font;
    float info_progress; // progress bar length, [0..1]
    int prev_anim_time_ms; // prev.animation time in milliseconds
    INFODISPLAY_ROW_TYPE info_row_type[INFODISPLAY_ROW_COUNT]; // row type
    char *info_rows[INFODISPLAY_ROW_COUNT]; // text row contents
    int info_row_mem[INFODISPLAY_ROW_COUNT]; // amount of bytes per row allocated
    INFODISPLAY_ICON info_row_icon[INFODISPLAY_ROW_COUNT]; // icon state for each row
    TTF_Surface *info_row_textsurf[INFODISPLAY_ROW_COUNT]; // cached rendered texts
    float info_row_scroll_time_s[INFODISPLAY_ROW_COUNT]; // row scroll times in seconds
    int info_row_text_width[INFODISPLAY_ROW_COUNT]; // cached text width
    unsigned long info_row_color[INFODISPLAY_ROW_COUNT]; // text color for each row
    unsigned long info_row_bkg_color[INFODISPLAY_ROW_COUNT]; // background color for each row
    time_t info_row_last_update[INFODISPLAY_ROW_COUNT]; // last time update
} INFODISPLAY;


// creates and initializes a new infodisplay
extern INFODISPLAY * infodisplay_create(int width, int height,
                                        int offs_r, int bits_r,
                                        int offs_g, int bits_g,
                                        int offs_b, int bits_b,
                                        int offs_a, int bits_a,
                                        const char *ttf_filename);
// closes infodisplay and frees its memory
extern void infodisplay_close(INFODISPLAY *disp);

// row=[-1..INFODISPLAY_ROW_COUNT] progress=[0..1]
extern void infodisplay_set_progress(INFODISPLAY *disp, int row, float progress, unsigned long color);
// row=[0..INFODISPLAY_ROW_COUNT[, color and bkg_color as 0xAARRGGBB (AA not used for now)
extern void infodisplay_set_row_color(INFODISPLAY *disp, int row, unsigned long color, unsigned long bkg_color);
// row=[0..INFODISPLAY_ROW_COUNT[, text in UTF8
extern void infodisplay_set_row_text(INFODISPLAY *disp, int row, int type, const char *text);
// sets row icon
extern void infodisplay_set_row_icon(INFODISPLAY *disp, int row, INFODISPLAY_ICON icon);
// shorthand for formatting given row to given times in [h:]mm:ss.0 / [h:]mm:ss.0 format
extern void infodisplay_set_row_times(INFODISPLAY *disp, int row, int time1_ms, int time2_ms);
// Renders the current display state to the backbuffer (disp->backbuf).
// If ret_req!=NULL, writes value to it where non-zero value means request for
// new refresh on next available frame (instead of waiting for outside event).
extern void infodisplay_update(INFODISPLAY *disp, int *ret_req);

#ifdef __cplusplus
}
#endif

#endif // !INFODISPLAY_H_INCLUDED
