#include "rameutil.h"

#include <assert.h>
#include <wchar.h>

#include <vc_dispmanx.h>
#include <bcm_host.h>
#include <GLES2/gl2.h>

#include FT_STROKER_H

void init_egl(state_t *s)
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

void fini_egl(state_t *s)
{
	FT_Done_FreeType(s->freetype);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(s->display, s->surface);
	eglMakeCurrent(s->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(s->display, s->surface);
	eglDestroyContext(s->display, s->context);
	eglTerminate(s->display);
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

fontdata_t *font_load(state_t *s, const char *filename, int face_index, unsigned int size)
{
	fontdata_t *f = calloc(1, sizeof(fontdata_t));
	if (!f) return 0;

	f->font = vgCreateFont(0);
	assert(f->font != VG_INVALID_HANDLE);
	assert(!FT_New_Face(s->freetype, filename, face_index, &f->face));
	assert(!FT_Set_Pixel_Sizes(f->face, 0, size));

	f->height = float_from_26_6(f->face->size->metrics.height);
	f->ascender = float_from_26_6(f->face->size->metrics.ascender);
	f->descender = float_from_26_6(f->face->size->metrics.descender);
	return f;
}

fontdata_t *font_load_default(state_t *s, unsigned int size) {
	return font_load(s, "/usr/share/fonts/TTF/FreeSerif.ttf", 0, size);
}

void font_unload(fontdata_t *f)
{
	int i;

	for (i = 0; i < sizeof(f->loaded_map)/sizeof(f->loaded_map[0]); i++)
		free(f->loaded_map[i]);
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

void font_get_text_extent(fontdata_t *f, const wchar_t *text, VGfloat *w, VGfloat *h)
{
	FT_Vector kerning;
	VGfloat x = 0, y = 0;
	int i, prev = 0, len = wcslen(text), cur;

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

void font_draw_text(fontdata_t *f, const wchar_t *text, VGfloat x, VGfloat y, VGbitfield paint_mode)
{
	VGuint glyphs[256];
	VGfloat adjx[256], adjy[256];
	VGfloat origin[2] = {x, y+(f->height-f->ascender-f->descender)/2};
	FT_Vector kerning;
	int i, prev = 0, len = wcslen(text), cur;

	for (i = 0; i < len; i++) {
		glyphs[i] = cur = FT_Get_Char_Index(f->face, text[i]);
		adjx[i] = 0.0f;
		adjy[i] = 0.0f;
		if (cur) {
			font_load_glyph(f, cur);
			if (i && FT_Get_Kerning(f->face, prev, cur, FT_KERNING_DEFAULT, &kerning) == 0) {
				adjx[i-1] = float_from_26_6(kerning.x);
				adjy[i-1] = float_from_26_6(kerning.y);
			}
		}
		prev = cur;
	}
	vgSetfv(VG_GLYPH_ORIGIN, 2, origin);
	vgDrawGlyphs(f->font, len, glyphs, adjx, adjy, paint_mode, VG_FALSE);
}
