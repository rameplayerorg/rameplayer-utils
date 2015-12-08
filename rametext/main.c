#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <locale.h>

#include <vc_dispmanx.h>
#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <VG/openvg.h>
#include <VG/vgu.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

typedef struct {
	uint32_t screen_width;
	uint32_t screen_height;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	VGfloat normalized_screen[9];
	FT_Library freetype;
} state_t;

static void init_egl(state_t *s)
{
	static EGL_DISPMANX_WINDOW_T nativewindow;
	static const EGLint attribute_list[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	int32_t success = 0;
	EGLBoolean result;
	EGLint num_config;
	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	EGLConfig config;

	// get an EGL display connection
	s->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(s->display != EGL_NO_DISPLAY);

	// initialize the EGL display connection
	result = eglInitialize(s->display, NULL, NULL);
	assert(EGL_FALSE != result);

	// bind OpenVG API
	eglBindAPI(EGL_OPENVG_API);

	// get an appropriate EGL frame buffer configuration
	result = eglChooseConfig(s->display, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);

	// create an EGL rendering context
	s->context = eglCreateContext(s->display, config, EGL_NO_CONTEXT, NULL);
	assert(s->context != EGL_NO_CONTEXT);

	// create an EGL window surface
	success = graphics_get_display_size(0 /* LCD */ , &s->screen_width,
					    &s->screen_height);
	assert(success >= 0);

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = s->screen_width;
	dst_rect.height = s->screen_height;

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = s->screen_width << 16;
	src_rect.height = s->screen_height << 16;

	dispman_display = vc_dispmanx_display_open(0 /* LCD */ );
	dispman_update = vc_dispmanx_update_start(0);

	dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display, 0 /*layer */ , &dst_rect, 0 /*src */ ,
						  &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha */ , 0 /*clamp */ ,
						  0 /*transform */ );

	nativewindow.element = dispman_element;
	nativewindow.width = s->screen_width;
	nativewindow.height = s->screen_height;
	vc_dispmanx_update_submit_sync(dispman_update);

	s->surface = eglCreateWindowSurface(s->display, config, &nativewindow, NULL);
	assert(s->surface != EGL_NO_SURFACE);

	// preserve the buffers on swap
	result = eglSurfaceAttrib(s->display, s->surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
	assert(EGL_FALSE != result);

	// connect the context to the surface
	result = eglMakeCurrent(s->display, s->surface, s->surface, s->context);
	assert(EGL_FALSE != result);

	// freetype
	assert(!FT_Init_FreeType(&s->freetype));
}

static void fini_egl(state_t *s)
{
	FT_Done_FreeType(s->freetype);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(s->display, s->surface);
	eglMakeCurrent(s->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(s->display, s->surface);
	eglDestroyContext(s->display, s->context);
	eglTerminate(s->display);
}

static void setfill(float color[4])
{
	VGPaint fillPaint = vgCreatePaint();
	vgSetParameteri(fillPaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	vgSetParameterfv(fillPaint, VG_PAINT_COLOR, 4, color);
	vgSetPaint(fillPaint, VG_FILL_PATH);
	vgDestroyPaint(fillPaint);
}

static void setstroke(float color[4], float width)
{
	VGPaint strokePaint = vgCreatePaint();
	vgSetParameteri(strokePaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	vgSetParameterfv(strokePaint, VG_PAINT_COLOR, 4, color);
	vgSetPaint(strokePaint, VG_STROKE_PATH);
	vgSetf(VG_STROKE_LINE_WIDTH, width);
	vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
	vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);
	vgDestroyPaint(strokePaint);
}

static VGfloat float_from_26_6(FT_Pos x)
{
	return (VGfloat)x / 64.0f;
}

struct path_data {
	int n_segs, n_coords;
	VGubyte segs[256];
	VGfloat coords[4*1024];
};

static int vg_move_to(const FT_Vector *to, void *user)
{
	struct path_data *p = user;
	p->segs[p->n_segs++] = VG_MOVE_TO;
	p->coords[p->n_coords++] = float_from_26_6(to->x);
	p->coords[p->n_coords++] = float_from_26_6(to->y);
	return 0;
}

static int vg_line_to(const FT_Vector *to, void *user)
{
	struct path_data *p = user;
	p->segs[p->n_segs++] = VG_LINE_TO;
	p->coords[p->n_coords++] = float_from_26_6(to->x);
	p->coords[p->n_coords++] = float_from_26_6(to->y);
	return 0;
}

static int vg_conic_to(const FT_Vector *control, const FT_Vector *to, void *user)
{
	struct path_data *p = user;
	p->segs[p->n_segs++] = VG_QUAD_TO;
	p->coords[p->n_coords++] = float_from_26_6(control->x);
	p->coords[p->n_coords++] = float_from_26_6(control->y);
	p->coords[p->n_coords++] = float_from_26_6(to->x);
	p->coords[p->n_coords++] = float_from_26_6(to->y);
	return 0;
}

static int vg_cubic_to(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *user)
{
	struct path_data *p = user;
	p->segs[p->n_segs++] = VG_CUBIC_TO;
	p->coords[p->n_coords++] = float_from_26_6(c1->x);
	p->coords[p->n_coords++] = float_from_26_6(c1->y);
	p->coords[p->n_coords++] = float_from_26_6(c2->x);
	p->coords[p->n_coords++] = float_from_26_6(c2->y);
	p->coords[p->n_coords++] = float_from_26_6(to->x);
	p->coords[p->n_coords++] = float_from_26_6(to->y);
	return 0;
}

static const FT_Outline_Funcs outline_funcs = {
	.move_to = vg_move_to,
	.line_to = vg_line_to,
	.conic_to = vg_conic_to,
	.cubic_to = vg_cubic_to,
};

#define LM_SIZE 12

typedef struct fontdata {
	VGfloat height, ascender, descender;
	VGFont font;
	FT_Face face;
	uint8_t *loaded_map[1<<(16-LM_SIZE)];
} fontdata_t;

static fontdata_t *font_load(state_t *s, const char *filename, int face_index)
{
	fontdata_t *f = calloc(1, sizeof(fontdata_t));
	if (!f) return 0;

	f->font = vgCreateFont(0);
	assert(f->font != VG_INVALID_HANDLE);
	assert(!FT_New_Face(s->freetype, filename, face_index, &f->face));
	assert(!FT_Set_Pixel_Sizes(f->face, 0, 128));

	f->height = float_from_26_6(f->face->size->metrics.height);
	f->ascender = float_from_26_6(f->face->size->metrics.ascender);
	f->descender = float_from_26_6(f->face->size->metrics.descender);
	return f;
}

static void font_unload(fontdata_t *f)
{
	int i;

	for (i = 0; i < sizeof(f->loaded_map)/sizeof(f->loaded_map[0]); i++)
		free(f->loaded_map);
	FT_Done_Face(f->face);
	vgDestroyFont(f->font);
}

static void font_load_glyph(fontdata_t *f, int index)
{
	static struct path_data pd;
	VGPath path;
	FT_Outline *outline;
	int mapa, mapb, mapc;

	if (index > 1<<16) return;

	mapa = index >> LM_SIZE;
	mapb = index & ((1<<LM_SIZE)-1);
	mapc = 1 << (index & 7);
	if (!f->loaded_map[mapa]) f->loaded_map[mapa] = calloc(1, sizeof(uint8_t[1<<LM_SIZE]));
	if (f->loaded_map[mapa][mapb] & mapc) return;
	f->loaded_map[mapa][mapb] |= mapc;

	FT_Load_Glyph(f->face, index, FT_LOAD_NO_BITMAP|FT_LOAD_NO_HINTING);

	outline = &f->face->glyph->outline;
	path = VG_INVALID_HANDLE;
	if (outline->n_contours != 0) {
		path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
				    1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
		assert(path != VG_INVALID_HANDLE);
		pd.n_segs = pd.n_coords = 0;
		FT_Outline_Decompose(outline, &outline_funcs, (void *) &pd);
		vgAppendPathData(path, pd.n_segs, pd.segs, pd.coords);
	}

	VGfloat glyphOrigin[2] = {0.0f, 0.0f};
	VGfloat escapement[2] = {
		float_from_26_6(f->face->glyph->advance.x),
		float_from_26_6(f->face->glyph->advance.y),
	};
	vgSetGlyphToPath(f->font, index, path, 0, glyphOrigin, escapement);
}

static void font_get_text_extent(fontdata_t *f, const wchar_t *text, size_t len, VGfloat *w, VGfloat *h)
{
	FT_Vector kerning;
	VGfloat x = 0, y = 0;
	int i, prev = 0, cur;

	for (i = 0; i < len; i++) {
		cur = FT_Get_Char_Index(f->face, text[i]);
		if (cur) {
			if (prev && FT_Get_Kerning(f->face, prev, cur, FT_KERNING_DEFAULT, &kerning) == 0) {
				x += float_from_26_6(kerning.x);
				y += float_from_26_6(kerning.y);
			}
			FT_Load_Glyph(f->face, cur, FT_LOAD_DEFAULT);
			x += float_from_26_6(f->face->glyph->advance.x);
			y += float_from_26_6(f->face->glyph->advance.y);
		}
		prev = cur;
	}
	y -= f->height;
	*w = x;
	*h = -y;
}

static void font_draw_text(fontdata_t *f, const wchar_t *text, size_t len, VGfloat x, VGfloat y, VGbitfield paint_mode)
{
	VGuint glyphs[256];
	VGfloat adjx[256], adjy[256];
	VGfloat origin[2] = {x, y+(f->height-f->ascender-f->descender)/2};
	FT_Vector kerning;
	int i, prev = 0, cur;

	for (i = 0; i < len; i++) {
		cur = glyphs[i] = FT_Get_Char_Index(f->face, text[i]);
		if (cur) {
			font_load_glyph(f, cur);
			if (i && FT_Get_Kerning(f->face, prev, cur, FT_KERNING_DEFAULT, &kerning) == 0) {
				x += float_from_26_6(kerning.x);
				y += float_from_26_6(kerning.y);
			}
		}
		prev = cur;
	}
	vgSetfv(VG_GLYPH_ORIGIN, 2, origin);
	vgDrawGlyphs(f->font, len, glyphs, adjx, adjy, paint_mode, VG_FALSE);
}

static void draw(state_t *s, const char *text)
{
	static VGfloat white[4] = { 1.0, 1.0, 1.0, 1.0 };
	fontdata_t *f;
	wchar_t wcs[256];
	VGfloat w, h;
	size_t n;
	int i;

	f = font_load(s, "/usr/share/fonts/TTF/FreeSerif.ttf", 0);
	vgClear(0, 0, s->screen_width, s->screen_height);

	setfill(white);
	//setstroke(white, 5);

	n = mbstowcs(wcs, text, sizeof wcs / sizeof wcs[0]);
	if (n != (size_t)-1) {
		font_get_text_extent(f, wcs, n, &w, &h);
		font_draw_text(f, wcs, n, (s->screen_width-w)/2, (s->screen_height-h)/2, VG_FILL_PATH);
	}
	font_unload(f);
}

static state_t state;

static void sig_handler(int sig)
{
	state_t *s = &state;
	eglTerminate(s->display);
	exit(1);
}

int main(int argc, char **argv)
{
	state_t *s = &state;
	const char *text = "Hello world!";

	signal(SIGINT, sig_handler);
	setlocale(LC_ALL, "C.utf-8");

	if (argc > 1) text = argv[1];

	bcm_host_init();
	init_egl(s);

	draw(s, text);

	vgFlush();
	eglSwapBuffers(s->display, s->surface);

	while (1)
		pause();

	return 1;
}
