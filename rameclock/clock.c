#include "rameutil.h"

#include <time.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <wchar.h>
#include <mosquitto.h>
#include <string.h>

#include "VG/openvg.h"
#include "VG/vgu.h"
#include "EGL/egl.h"
#include "GLES/gl.h"
#include "bcm_host.h"

static VGPaint create_paint(const VGfloat rgba[4])
{
	VGPaint paint = vgCreatePaint();
	vgSetParameteri(paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	vgSetParameterfv(paint, VG_PAINT_COLOR, 4, rgba);
	return paint;
}

static VGPaint create_gradient_paint(state_t *s)
{
	VGfloat lgcoord[4] = { 0, 0, s->screen_width, s->screen_height };
	VGfloat stops[] = {
		/* s   R    G    B    A  */
		0.0,  .20, .20, .20, 1.0,
		0.4,  .65, .65, .65, 1.0,
		0.45, .75, .75, .75, 1.0,
		0.6,  .65, .65, .65, 1.0,
		1.0,  .20, .20, .20, 1.0,
	};

	VGPaint paint = vgCreatePaint();
	vgSetParameteri(paint, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
	vgSetParameterfv(paint, VG_PAINT_LINEAR_GRADIENT, 4, lgcoord);

	//vgSetParameteri(paint, VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_REPEAT);
	vgSetParameteri(paint, VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_REFLECT);
	vgSetParameteri(paint, VG_PAINT_COLOR_RAMP_PREMULTIPLIED, VG_FALSE);
	vgSetParameterfv(paint, VG_PAINT_COLOR_RAMP_STOPS, sizeof(stops)/sizeof(stops[0]), stops);

	return paint;
}

static VGPath create_big_hand(VGfloat size)
{
	VGfloat pts[] = {
#if 0
		0,		0,
		-0.035,		0.10,
		0,		0.85*size,
		+0.035,		0.10,
		0,		0
#else
		-0.01,		-0.1,
		-0.035,		0.85*size-0.1,
		0,		0.85*size,
		+0.035,		0.85*size-0.1,
		+0.01,		-0.1,
		-0.01,		-0.1,
#endif
	};
	VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	vguPolygon(path, pts, sizeof(pts)/sizeof(pts[0])/2, VG_FALSE);
	return path;
}

static VGPath create_small_hand(VGfloat size)
{
	VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);

	VGfloat end = -0.18, r = 0.03;
	vguLine(path, .0, .85 * size, 0., end);
	vguRect(path, -0.01, end, 2*0.01, 2*r);
	vguEllipse(path, .0, .0, 2*r, 2*r);

	return path;
}

static VGPath create_rect(VGfloat x, VGfloat y, VGfloat w, VGfloat h)
{
	VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	vguRect(path, x, y, w, h);
	return path;
}

static VGPath create_tick(VGfloat a, VGfloat b, VGfloat w)
{
	return create_rect(-w/2, a, w, b-a);
}

static const VGfloat rgba_white[4]  = { 1.0, 1.0, 1.0, 1.0 };
static const VGfloat rgba_red[4]    = { 1.0,   0,   0, 1.0 };
static const VGfloat rgba_black[4]  = {   0,   0,   0, 1.0 };
static const VGfloat rgba_silver[4] = { 0.3, 0.3, 0.3, 1.0 };
static const VGfloat rgba_alert[4]  = { 1.0,   0,   0, 0.4 };

static VGImage load_ppm(const char *filename, int *logo_w, int *logo_h)
{
	FILE *fp = NULL;
	char *buf = NULL;
	VGImage image = VG_INVALID_HANDLE;
	unsigned int w, h, d;
	int r;

	fp = fopen(filename, "rb");
	if (!fp) goto err;

	if (fscanf(fp, "P6\n%u %u\n%u\n", &w, &h, &d) != 3) goto err;
	if (d != 255) goto err;

	buf = malloc(w*h*4);
	if (!buf) goto err;

	for (r = 0; r < w*h*4; r += 4) {
		buf[r+0] = fgetc(fp);
		buf[r+1] = fgetc(fp);
		buf[r+2] = fgetc(fp);
		buf[r+3] = 0xff;
	}

	image = vgCreateImage(VG_sABGR_8888, w, h, VG_IMAGE_QUALITY_BETTER);
	vgImageSubData(image, buf+w*4*(h-1), -w*4, VG_sABGR_8888, 0, 0, w, h);
	*logo_w = w;
	*logo_h = h;

err:
	if (fp) fclose(fp);
	free(buf);
	return image;
}

#define TIME_BUF_SIZE 9

static void update_time(struct timeval *tv, struct tm *tm, wchar_t *buf) {
	gettimeofday(tv, 0);
	localtime_r(&tv->tv_sec, tm);

	//tm->tm_hour = 7;
	//tm->tm_min = 20;

	wcsftime(buf, TIME_BUF_SIZE, L"%T", tm);
}

static void on_connect(struct mosquitto *mosq, void *data, int status) {
	if (!status) mosquitto_subscribe(mosq, NULL, "rame/clock/alert", 1);
}

static void on_message(struct mosquitto *mosq, void *data, const struct mosquitto_message *msg) {
	if (msg->payloadlen) memcpy(data, msg->payload, 1);
}

int main(int argc, char **argv)
{
	float fps = 25.0;
	state_t state = {0}, *s = &state;
	struct timeval tv;
	struct tm tm;
	int i;

	VGPath background_rect;
	VGPaint black_paint, gradient_paint;

	int analog = 1;
	VGfloat analog_h = 0;
	VGfloat analog_size;
	int logo_w, logo_h;
	VGImage logo = VG_INVALID_HANDLE;
	VGPath mega_tick, big_tick, small_tick, hour_hand, minute_hand, second_hand;
	VGPaint red_paint, silver_paint;

	int digital = 0;
	fontdata_t *font;
	wchar_t time_buf[TIME_BUF_SIZE];
	VGfloat digital_w;
	VGfloat digital_h = 0;

	struct mosquitto *mosq = NULL;
	time_t last_reconn_attempt = 0;
	char alert_state = '0';
	VGPaint alert_paint;

	int opt;
	static const struct option long_options[] = {
		{"broker", required_argument, NULL, 'b'},
		{"display", required_argument, NULL, 'd'},
		{"logo", required_argument, NULL, 'l'},
		{NULL, 0, NULL, 0}
	};

	bcm_host_init();
	init_egl(s);

	while ((opt = getopt_long(argc, argv, "b:d:l:", long_options, NULL)) != -1) {
		switch (opt) {
		case 'b':
			alert_paint = create_paint(rgba_alert);

			mosquitto_lib_init();
			mosq = mosquitto_new(NULL, true, &alert_state);
			mosquitto_connect_callback_set(mosq, on_connect);
			mosquitto_message_callback_set(mosq, on_message);
			mosquitto_connect(mosq, optarg, 1883, 60);

			break;
		case 'd':
			if (!strcmp(optarg, "combined")) digital = 1;
			else if (!strcmp(optarg, "digital")) {
				analog = 0;
				digital = 1;
			}
			break;
		case 'l':
			logo = load_ppm(optarg, &logo_w, &logo_h);
		}
	}

	// set up screen ratio
	glViewport(0, 0, (GLsizei) s->screen_width, (GLsizei) s->screen_height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float ratio = (float)s->screen_width / (float)s->screen_height;
	glFrustumf(-ratio, ratio, -1.0f, 1.0f, 1.0f, 10.0f);

	gradient_paint = create_gradient_paint(s);
	black_paint = create_paint(rgba_black);
	background_rect = create_rect(0, 0, s->screen_width, s->screen_height);

	if (digital) {
		font = font_load_default(s, analog && ratio > 1 ? s->screen_height / 4 : s->screen_width / 5);
		update_time(&tv, &tm, time_buf);
		font_get_text_extent(font, time_buf, &digital_w, &digital_h);
	}

	if (analog) {
		analog_h = s->screen_height - digital_h;
		analog_size = analog_h < s->screen_width ? analog_h : s->screen_width;

		/* create standard translation (center screen) */
		vgTranslate(s->screen_width / 2.0, digital_h + analog_h / 2.0);
		vgScale(analog_size / 2.0, analog_size / 2.0);
		vgGetMatrix(s->normalized_screen);

		vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
		vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);

		silver_paint = create_paint(rgba_silver);
		red_paint = create_paint(rgba_red);

		mega_tick = create_tick(.71, .91, 0.04);
		big_tick = create_tick(.8, .9, 0.04);
		small_tick = create_tick(.825, .875, 0.01);

		hour_hand = create_big_hand(0.5);
		minute_hand = create_big_hand(0.9);
		second_hand = create_small_hand(0.95);

		assert(vgGetError() == VG_NO_ERROR);
	}

	while (1) {
		update_time(&tv, &tm, time_buf);

		if (mosq && mosquitto_loop(mosq, 0, 1) && tv.tv_sec - last_reconn_attempt > 15) {
			alert_state = '0';
			mosquitto_reconnect_async(mosq);
			last_reconn_attempt = tv.tv_sec;
		}

		/* clear */
		vgLoadIdentity();
		vgSetPaint(gradient_paint, VG_FILL_PATH);
		vgDrawPath(background_rect, VG_STROKE_PATH | VG_FILL_PATH);

		/* logo */
		if (analog && logo != VG_INVALID_HANDLE) {
			vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
			vgLoadIdentity();
			vgTranslate(s->screen_width / 2, digital_h + analog_h / 2 - analog_size / 4);
			vgScale(0.3, 0.3);
			vgTranslate(-logo_w/2, -logo_h/2);
			vgDrawImage(logo);
			vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
		}

		if (alert_state == '1' || (alert_state == '2' && tv.tv_usec >= 500000)) {
			vgLoadIdentity();
			vgSetPaint(alert_paint, VG_FILL_PATH);
			vgDrawPath(background_rect, VG_STROKE_PATH | VG_FILL_PATH);
		}

		if (analog) {
			/* clock face */
			vgLoadMatrix(s->normalized_screen);
			vgSetPaint(black_paint, VG_FILL_PATH);
			//vgSetPaint(black_paint, VG_STROKE_PATH);
			vgSetPaint(silver_paint, VG_STROKE_PATH);
			vgSetf(VG_STROKE_LINE_WIDTH, 0.008);
			for (i = 0; i < 60; i++) {
				if (i % 15 == 0) {
					vgDrawPath(mega_tick, VG_FILL_PATH | VG_STROKE_PATH);
				} else if (i % 5 == 0) {
					vgDrawPath(big_tick, VG_FILL_PATH);
				} else {
					vgDrawPath(small_tick, VG_FILL_PATH);
				}
				vgRotate(360/60.0);
			}

			vgSetf(VG_STROKE_LINE_WIDTH, 0.008);
			vgSetPaint(silver_paint, VG_STROKE_PATH);
			vgSetPaint(black_paint, VG_FILL_PATH);

			vgLoadMatrix(s->normalized_screen);
			vgRotate(-360.0/12.0 * (tm.tm_min/60.0 + tm.tm_hour));
			vgDrawPath(hour_hand, VG_STROKE_PATH | VG_FILL_PATH);

			vgLoadMatrix(s->normalized_screen);
			vgRotate(-360.0/60.0 * (tm.tm_sec/60.0 + tm.tm_min));
			vgDrawPath(minute_hand, VG_STROKE_PATH | VG_FILL_PATH);

			vgSetPaint(red_paint, VG_STROKE_PATH);
			vgSetPaint(red_paint, VG_FILL_PATH);
			vgSetf(VG_STROKE_LINE_WIDTH, 0.01);
			vgLoadMatrix(s->normalized_screen);
			vgRotate(-360.0/60.0 * tm.tm_sec);
			vgDrawPath(second_hand, VG_STROKE_PATH | VG_FILL_PATH);
		}

		if (digital) {
			vgSetPaint(black_paint, VG_FILL_PATH);
			font_get_text_extent(font, time_buf, &digital_w, &digital_h);
			font_draw_text(font, time_buf, (s->screen_width - digital_w) / 2.0, (s->screen_height - analog_h - digital_h) / 2.0, VG_FILL_PATH);
		}

		assert(vgGetError() == VG_NO_ERROR);
		eglSwapBuffers(s->display, s->surface);
		assert(eglGetError() == EGL_SUCCESS);

		usleep(1000000/fps);
		//usleep(50 * 1000);
	}

	fini_egl(s);
	return 0;
}
