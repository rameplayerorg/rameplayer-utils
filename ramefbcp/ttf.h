/*
  ttf.h: TTF rendering functions; based on SDL2_ttf but simplified.

  ---- Original license ----
  SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
  Copyright (C) 2001-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* This library is a wrapper around the excellent FreeType 2.0 library,
   available at:
    http://www.freetype.org/
*/

#ifndef _TTF_H
#define _TTF_H


/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif


/* Simple suface struct to replace SDL_Surface.
 * - assuming surface is always 8bpp (luminance or alpha/opacity)
 */
typedef struct _TTF_Surface
{
    int w, h;
    unsigned short pitch;
    void *pixels;
} TTF_Surface;

extern TTF_Surface * TTF_CreateSurface(int width, int height);
extern void TTF_FreeSurface(TTF_Surface *surface);
extern void TTF_ClearSurface(TTF_Surface *surface);

/* The version info tells which SDL_ttf this is based on. */

/* Printable format: "%d.%d.%d", MAJOR, MINOR, PATCHLEVEL
*/
#define TTF_MAJOR_VERSION   2
#define TTF_MINOR_VERSION   0
#define TTF_PATCHLEVEL      12

/* This macro can be used to fill a version structure with the compile-time
 * version of the ttf library.
 */
#define TTF_VERSION(X)                          \
{                                                   \
    (X)->major = TTF_MAJOR_VERSION;             \
    (X)->minor = TTF_MINOR_VERSION;             \
    (X)->patch = TTF_PATCHLEVEL;                \
}

/* The internal structure containing font information */
typedef struct _TTF_Font TTF_Font;

/* Initialize the TTF engine - returns 0 if successful, -1 on error */
extern int TTF_Init(void);

/* Open a font file and create a font of the specified point size.
 * Some .fon fonts will have several sizes embedded in the file, so the
 * point size becomes the index of choosing which size.  If the value
 * is too high, the last indexed size will be the default. */
extern TTF_Font * TTF_OpenFont(const char *file, int ptsize);
extern TTF_Font * TTF_OpenFontIndex(const char *file, int ptsize, long index);

/* Set and retrieve the font style */
#define TTF_STYLE_NORMAL        0x00
#define TTF_STYLE_BOLD          0x01
#define TTF_STYLE_ITALIC        0x02
/*#define TTF_STYLE_UNDERLINE     0x04 // support removed */
/*#define TTF_STYLE_STRIKETHROUGH 0x08 // support removed */
extern int TTF_GetFontStyle(const TTF_Font *font);
extern void TTF_SetFontStyle(TTF_Font *font, int style);
extern int TTF_GetFontOutline(const TTF_Font *font);
extern void TTF_SetFontOutline(TTF_Font *font, int outline);

/* Set and retrieve FreeType hinter settings */
#define TTF_HINTING_NORMAL    0
#define TTF_HINTING_LIGHT     1
#define TTF_HINTING_MONO      2
#define TTF_HINTING_NONE      3
extern int TTF_GetFontHinting(const TTF_Font *font);
extern void TTF_SetFontHinting(TTF_Font *font, int hinting);

/* Get the total height of the font - usually equal to point size */
extern int TTF_FontHeight(const TTF_Font *font);

/* Get the offset from the baseline to the top of the font
   This is a positive value, relative to the baseline.
 */
extern int TTF_FontAscent(const TTF_Font *font);

/* Get the offset from the baseline to the bottom of the font
   This is a negative value, relative to the baseline.
 */
extern int TTF_FontDescent(const TTF_Font *font);

/* Get the recommended spacing between lines of text for this font */
extern int TTF_FontLineSkip(const TTF_Font *font);

/* Get/Set whether or not kerning is allowed for this font */
extern int TTF_GetFontKerning(const TTF_Font *font);
extern void TTF_SetFontKerning(TTF_Font *font, int allowed);

/* Get the number of faces of the font */
extern long TTF_FontFaces(const TTF_Font *font);

/* Get the font face attributes, if any */
extern int TTF_FontFaceIsFixedWidth(const TTF_Font *font);
extern char * TTF_FontFaceFamilyName(const TTF_Font *font);
extern char * TTF_FontFaceStyleName(const TTF_Font *font);

/* Check wether a glyph is provided by the font or not */
extern int TTF_GlyphIsProvided(const TTF_Font *font, unsigned short ch);

/* Get the metrics (dimensions) of a glyph
   To understand what these metrics mean, here is a useful link:
    http://freetype.sourceforge.net/freetype2/docs/tutorial/step2.html
 */
extern int TTF_GlyphMetrics(TTF_Font *font, unsigned short ch,
                            int *minx, int *maxx,
                            int *miny, int *maxy, int *advance);

/* Get the dimensions of a rendered string of text */
extern int TTF_SizeText(TTF_Font *font, const char *text, int *w, int *h);
extern int TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h);

/* Create a surface and render the given text at fast quality.
   This function returns the new surface, or NULL if there was an error.
*/
extern TTF_Surface * TTF_RenderText_Solid(TTF_Font *font, const char *text);
extern TTF_Surface * TTF_RenderUTF8_Solid(TTF_Font *font, const char *text);

/* Create a surface and render the given glyph at fast quality.
   The glyph is rendered without any padding or centering in the X
   direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern TTF_Surface * TTF_RenderGlyph_Solid(TTF_Font *font, unsigned short ch);

/* Create a surface surface and render the given text at high quality.
   This function returns the new surface, or NULL if there was an error.
*/
extern TTF_Surface * TTF_RenderText_Shaded(TTF_Font *font, const char *text);
extern TTF_Surface * TTF_RenderUTF8_Shaded(TTF_Font *font, const char *text);

/* Render given text to an existing surface at high quality.
*/
extern void TTF_RenderUTF8_Shaded_Surface(TTF_Surface *dest,
                                          TTF_Font *font, const char *text);

/* Create a surface and render the given glyph at high quality.
   The glyph is rendered without any padding or centering in the X
   direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern TTF_Surface * TTF_RenderGlyph_Shaded(TTF_Font *font, unsigned short ch);


/* Close an opened font file */
extern void TTF_CloseFont(TTF_Font *font);

/* De-initialize the TTF engine */
extern void TTF_Quit(void);

/* Check if the TTF engine is initialized */
extern int TTF_WasInit(void);

/* Get the kerning size of two glyphs */
extern int TTF_GetFontKerningSize(TTF_Font *font, int prev_index, int index);

extern char * TTF_GetError(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* _TTF_H */
