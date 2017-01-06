#include "rameutil.h"

#include <signal.h>
#include <locale.h>

#include <bcm_host.h>
#include <EGL/egl.h>
#include <VG/openvg.h>

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

static void draw(state_t *s, const char *text)
{
	static VGfloat white[4] = { 1.0, 1.0, 1.0, 1.0 };
	fontdata_t *f;
	wchar_t wcs[256];
	VGfloat w, h;
	size_t n;
	int i;

	f = font_load(s, "FreeSerif", 0, 96);
	vgClear(0, 0, s->screen_width, s->screen_height);

	setfill(white);
	//setstroke(white, 5);

	n = mbstowcs(wcs, text, sizeof wcs / sizeof wcs[0]);
	if (n != (size_t)-1) {
		font_get_text_extent(f, wcs, &w, &h);
		font_draw_text(f, wcs, (s->screen_width-w)/2, (s->screen_height-h)/2, VG_FILL_PATH);
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
