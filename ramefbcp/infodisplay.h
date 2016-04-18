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


typedef enum INFODISPLAY_ICON_ENUM
{
    INFODISPLAY_ICON_NONE = 0, // no icon, no reserved space
    INFODISPLAY_ICON_EMPTY, // no icon but reserved space
    INFODISPLAY_ICON_PLAYING,
    INFODISPLAY_ICON_PAUSED,
    INFODISPLAY_ICON_STOPPED,
    INFODISPLAY_ICON_BUFFERING,
    INFODISPLAY_ICON_WAITING,
    INFODISPLAY_ICON_COUNT //
} INFODISPLAY_ICON;

typedef struct _TTF_Font TTF_Font;
typedef struct _TTF_Surface TTF_Surface;

typedef struct _INFODISPLAY
{
    PIXEL *backbuf;
    int backbuf_size;
    int width, height;
    int progress_bar_row; // draw bar above this text row (affects text row y)
    int progress_bar_height;
    int row_height; // amount of pixels per text row
    // rgba offset & bit width inside pixels:
    unsigned char offs_r, bits_r, offs_g, bits_g, offs_b, bits_b, offs_a, bits_a;
    TTF_Font *font;
    TTF_Surface *textsurf; // text rendering work area
    float info_progress; // progress bar length, [0..1]
    char *info_rows[INFODISPLAY_ROW_COUNT]; // text row contents
    int info_row_mem[INFODISPLAY_ROW_COUNT]; // amount of bytes per row allocated
    INFODISPLAY_ICON info_row_icon[INFODISPLAY_ROW_COUNT]; // icon state for each row
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

// progress=[0..1]
extern void infodisplay_set_progress(INFODISPLAY *disp, float progress);
// row=[0..INFODISPLAY_ROW_COUNT[, text in UTF8
extern void infodisplay_set_row_text(INFODISPLAY *disp, int row, const char *text);
// sets row icon
extern void infodisplay_set_row_icon(INFODISPLAY *disp, int row, INFODISPLAY_ICON icon);
// shorthand for formatting given row to given times in [h:]mm:ss.0 / [h:]mm:ss.0 format
extern void infodisplay_set_row_times(INFODISPLAY *disp, int row, int time1_ms, int time2_ms);
// renders the current display state to the backbuffer (disp->backbuf)
extern void infodisplay_update(INFODISPLAY *disp);

#ifdef __cplusplus
}
#endif

#endif // !INFODISPLAY_H_INCLUDED
