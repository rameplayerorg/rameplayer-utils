#include <EGL/egl.h>
#include <VG/openvg.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct {
	uint32_t screen_width;
	uint32_t screen_height;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	VGfloat normalized_screen[9];
	FT_Library freetype;
} state_t;

void init_egl(state_t *s);
void fini_egl(state_t *s);

#define LM_SIZE 12

typedef struct fontdata {
	VGfloat height, ascender, descender;
	VGFont font;
	FT_Face face;
	uint8_t *loaded_map[1<<(16-LM_SIZE)];
} fontdata_t;

fontdata_t *font_load(state_t *s, const char *filename, int face_index, unsigned int size);
fontdata_t *font_load_default(state_t *s, unsigned int size);
void font_unload(fontdata_t *f);
void font_get_text_extent(fontdata_t *f, const wchar_t *text, VGfloat *w, VGfloat *h);
void font_draw_text(fontdata_t *f, const wchar_t *text, VGfloat x, VGfloat y, VGbitfield paint_mode);
