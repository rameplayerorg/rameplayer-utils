
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <bcm_host.h>

#include "debug.h"
#include "input.h"
#include "infodisplay.h"


#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_PATCH 1

#define TTF_DEFAULT_FILENAME "/usr/share/fonts/TTF/ramefbcp.ttf"

#define SLEEP_MILLISECONDS_PER_FRAME 25

// Scale input video to rect with this aspect ratio:
#define VID_ASPECT_W 16
#define VID_ASPECT_H 9


static int s_alive = 1;

static const char *s_ttf_filename = NULL;

static INFODISPLAY_ICON s_current_status = INFODISPLAY_ICON_NONE;


static void print_fb_info(struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo)
{
#ifdef DEBUG_SUPPORT
    if (!g_debug_info)
        return;

    dbg_printf("*** fb_fix_screeninfo ***\n");
    dbg_printf("id: %s\n", finfo->id); // is it safe to assume there's 0-terminator?
    dbg_printf("smem_start: %08x\n", (unsigned int)finfo->smem_start);
    dbg_printf("smem_len: %u\n", finfo->smem_len);
    dbg_printf("type: %u\n", finfo->type);
    dbg_printf("type_aux: %u\n", finfo->type_aux);
    dbg_printf("visual: %u\n", finfo->visual);
    dbg_printf("xpanstep: %u\n", finfo->xpanstep);
    dbg_printf("ypanstep: %u\n", finfo->ypanstep);
    dbg_printf("ywrapstep: %u\n", finfo->ywrapstep);
    dbg_printf("line_length: %u\n", finfo->line_length);
    dbg_printf("mmio_start: %08x\n", (unsigned int)finfo->mmio_start);
    dbg_printf("mmio_len: %u\n", finfo->mmio_len);
    dbg_printf("accel: %u\n", finfo->accel);
    dbg_printf("capabilities: %04x\n", finfo->capabilities);
    dbg_printf("*** fb_var_screeninfo ***\n");
    dbg_printf("xres: %u\n", vinfo->xres);
    dbg_printf("yres: %u\n", vinfo->yres);
    dbg_printf("xres_virtual: %u\n", vinfo->xres_virtual);
    dbg_printf("yres_virtual: %u\n", vinfo->yres_virtual);
    dbg_printf("xoffset: %u\n", vinfo->xoffset);
    dbg_printf("yoffset: %u\n", vinfo->yoffset);
    dbg_printf("bits_per_pixel: %u\n", vinfo->bits_per_pixel);
    dbg_printf("grayscale: %u\n", vinfo->grayscale);
    dbg_printf("red.offset,length,msb_right: %u,%u,%u\n", vinfo->red.offset, vinfo->red.length, vinfo->red.msb_right);
    dbg_printf("green.offset,length,msb_right: %u,%u,%u\n", vinfo->green.offset, vinfo->green.length, vinfo->green.msb_right);
    dbg_printf("blue.offset,length,msb_right: %u,%u,%u\n", vinfo->blue.offset, vinfo->blue.length, vinfo->blue.msb_right);
    dbg_printf("transp.offset,length,msb_right: %u,%u,%u\n", vinfo->transp.offset, vinfo->transp.length, vinfo->transp.msb_right);
    dbg_printf("nonstd: %u\n", vinfo->nonstd);
    dbg_printf("activate: %u\n", vinfo->activate);
    dbg_printf("height: %u\n", vinfo->height);
    dbg_printf("width: %u\n", vinfo->width);
    dbg_printf("accel_flags: %08x\n", vinfo->accel_flags);
    dbg_printf("pixclock: %u\n", vinfo->pixclock);
    dbg_printf("left_margin: %u\n", vinfo->left_margin);
    dbg_printf("right_margin: %u\n", vinfo->right_margin);
    dbg_printf("upper_margin: %u\n", vinfo->upper_margin);
    dbg_printf("lower_margin: %u\n", vinfo->lower_margin);
    dbg_printf("hsync_len: %u\n", vinfo->hsync_len);
    dbg_printf("vsync_len: %u\n", vinfo->vsync_len);
    dbg_printf("sync: %u\n", vinfo->sync);
    dbg_printf("vmode: %u\n", vinfo->vmode);
    dbg_printf("rotate: %u\n", vinfo->rotate);
    dbg_printf("colorspace: %u\n", vinfo->colorspace);
    dbg_printf("---\n");
#endif
}


static void translate_input_line(INFODISPLAY *infodisplay, int *video_enabled, const char *line)
{
    switch (line[0])
    {
        case 'X':
        {
            // text to row number [1..INFODISPLAY_ROW_COUNT].
            // e.g. "X1:Please wait..."
            int rown = line[1] - '1';
            if (rown >= 0 && rown < INFODISPLAY_ROW_COUNT &&
                line[2] == ':')
                infodisplay_set_row_text(infodisplay, rown, &line[3]);
        }
        break;

        case 'P':
        {
            // progress bar, value is [0..1000], e.g. "P:567"
            int value = -1;
            if (line[1] == ':' && line[2] >= '0' && line[2] <= '9')
                value = atoi(&line[2]);
            if (value >= 0)
                infodisplay_set_progress(infodisplay, value / 1000.0f);
        }
        break;

        case 'S':
        {
            // set status (icon), value from INFODISPLAY_ICON, e.g. "S:4"
            int value = -1;
            if (line[1] == ':')
                value = line[2] - '0';
            if (value >= INFODISPLAY_ICON_NONE &&
                value < INFODISPLAY_ICON_COUNT)
            {
                s_current_status = (INFODISPLAY_ICON)value;
                infodisplay_set_row_icon(infodisplay,
                                         INFODISPLAY_ROW_COUNT - 1,
                                         (INFODISPLAY_ICON)value);
            }
        }
        break;

        case 'V':
        {
            // enable or disable video cloning (framebuffer copy)
            // "V:1" (enable) or "V:0" (disable)
            int value = -1;
            if (line[1] == ':')
                value = line[2] - '0';
            if (value == 0 || value == 1)
                *video_enabled = value;
        }
        break;

        case 'T':
        {
            // set times (playing & total), values in milliseconds
            // e.g. "T:5100,90000" (comma and second value are optional)
            // negative value is accepted only for the first value
            int t1 = 0, t2 = -1, comma = 0;
            char tmp[32];
            tmp[31] = 0;
            if (line[1] != ':')
                break;
            strncpy(tmp, &line[2], 31);
            while (tmp[comma] != ',' && tmp[comma] != 0)
                ++comma;
            if (tmp[comma] == ',')
                t2 = atoi(&tmp[comma + 1]);
            tmp[comma] = 0;
            t1 = atoi(tmp);
            infodisplay_set_row_times(infodisplay, INFODISPLAY_ROW_COUNT - 1, t1, t2);
        }
        break;
    }
}


static int process()
{
    struct fb_var_screeninfo fbvinfo;
    struct fb_fix_screeninfo fbfinfo;
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_MODEINFO_T display_info;
    DISPMANX_RESOURCE_HANDLE_T screen_resource;
    //VC_IMAGE_TRANSFORM_T transform;
    uint32_t image_prt;
    VC_RECT_T rect1;
    int ret;
    int fbfd = 0;
    char *fbp = 0;

    int frame = 0;
    int screen_width = 0, screen_height = 0;
    int video_enabled = 0;
    int vid_w = 0, vid_h = 0;
    INFODISPLAY *infodisplay = NULL;
    INPUT_CTX *inputctx = NULL;


    bcm_host_init();

    display = vc_dispmanx_display_open(0);
    if (!display)
    {
        syslog(LOG_ERR, "Unable to open primary display");
        return EXIT_FAILURE;
    }
    ret = vc_dispmanx_display_get_info(display, &display_info);
    if (ret)
    {
        syslog(LOG_ERR, "Unable to get primary display information");
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "Primary display is %d x %d", display_info.width, display_info.height);


    fbfd = open("/dev/fb1", O_RDWR);
    if (fbfd == -1)
    {
        syslog(LOG_ERR, "Unable to open secondary display");
        return EXIT_FAILURE;
    }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fbfinfo))
    {
        syslog(LOG_ERR, "Unable to get secondary display information (f)");
        return EXIT_FAILURE;
    }
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &fbvinfo))
    {
        syslog(LOG_ERR, "Unable to get secondary display information (v)");
        return EXIT_FAILURE;
    }

    print_fb_info(&fbvinfo, &fbfinfo);

    syslog(LOG_INFO, "Second display is %d x %d %dbpp\n", fbvinfo.xres, fbvinfo.yres, fbvinfo.bits_per_pixel);

    vid_w = fbvinfo.xres;
    vid_h = fbvinfo.xres * VID_ASPECT_H / VID_ASPECT_W;
    if (vid_h > fbvinfo.yres)
        vid_h = fbvinfo.yres; // shouldn't happen...
    dbg_printf("vid_w,vid_h: %d,%d\n", vid_w, vid_h);
    screen_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, vid_w, vid_h, &image_prt);
    if (!screen_resource) {
        syslog(LOG_ERR, "Unable to create screen buffer");
        close(fbfd);
        vc_dispmanx_display_close(display);
        return EXIT_FAILURE;
    }

    fbp = (char *)mmap(0, fbfinfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbp <= 0)
    {
        syslog(LOG_ERR, "Unable to create memory mapping");
        close(fbfd);
        ret = vc_dispmanx_resource_delete(screen_resource);
        vc_dispmanx_display_close(display);
        return EXIT_FAILURE;
    }

    vc_dispmanx_rect_set(&rect1, 0, 0, vid_w, vid_h);

    memset(fbp, 0, fbfinfo.smem_len);

    //screen_width = fbvinfo.xres;
    screen_width = fbfinfo.line_length / (fbvinfo.bits_per_pixel / 8);
    screen_height = fbvinfo.yres;

    inputctx = input_create(fileno(stdin));

    // only 16bpp is supported for now:
    if (fbvinfo.bits_per_pixel == 16)
    {
        if (s_ttf_filename == NULL)
            s_ttf_filename = TTF_DEFAULT_FILENAME;
        infodisplay = infodisplay_create(screen_width, screen_height,
                                         fbvinfo.red.offset, fbvinfo.red.length,
                                         fbvinfo.green.offset, fbvinfo.green.length,
                                         fbvinfo.blue.offset, fbvinfo.blue.length,
                                         fbvinfo.transp.offset, fbvinfo.transp.length,
                                         s_ttf_filename);
    }
    else
    {
        const char *warnfmt = "Only 16bpp supported for bottom infodisplay for now (display bpp %d, int.pix size %d)";
        syslog(LOG_WARNING, warnfmt, fbvinfo.bits_per_pixel, sizeof(PIXEL));
        dbg_printf(warnfmt, fbvinfo.bits_per_pixel, sizeof(PIXEL));
    }

    while (s_alive)
    {
        int need_to_refresh_display = 0;
        const int LINESIZE = 256;
        char line[LINESIZE];

        if (video_enabled)
        {
            ret = vc_dispmanx_snapshot(display, screen_resource, 0);
            vc_dispmanx_resource_read_data(screen_resource, &rect1, fbp,
                                           fbvinfo.xres * fbvinfo.bits_per_pixel / 8);
        }

        if (inputctx != NULL)
        {
            int try_read_more;
            do {
                try_read_more = 0;
                int read_status = input_read_line(line, LINESIZE, inputctx);
                if (read_status == -1)
                {
                    fprintf(stderr, "EOF\n");
                    s_alive = 0;
                }
                else if (read_status > 0)
                {
                    dbg_printf("Line: %s\n", line);

                    translate_input_line(infodisplay, &video_enabled, line);
                    need_to_refresh_display = 1;
                    try_read_more = 1;
                }
            } while (try_read_more);
        }

        // always refresh display when we're in a state with animated icon:
        if (s_current_status == INFODISPLAY_ICON_BUFFERING ||
            s_current_status == INFODISPLAY_ICON_WAITING)
        {
            need_to_refresh_display = 1;
        }

        if (infodisplay != NULL && need_to_refresh_display)
        {
            //// hardcoded infodisplay update test:
            //infodisplay_set_progress(infodisplay, (float)frame * 40 / (1*60*1000+32*1000+100) );
            //infodisplay_set_row_text(infodisplay, 5, "xx_zz_nnn_r720P.mp4");
            //infodisplay_set_row_icon(infodisplay, 6, INFODISPLAY_ICON_PLAYING);
            //infodisplay_set_row_times(infodisplay, 6, frame * 40,
            //                          345*60*60*1000 + 45*60*1000+32*1000+100);

            infodisplay_update(infodisplay);

            if (video_enabled)
            {
                // update infodisplay backbuffer to bottom part of the screen
                // (upper part is cloned video preview)
                int start_byte_offset = vid_h * fbfinfo.line_length;
                memcpy((char *)fbp + start_byte_offset,
                       (char *)infodisplay->backbuf + start_byte_offset,
                       infodisplay->backbuf_size - start_byte_offset);
            } else {
                // update infodisplay to whole screen
                // (cloned video preview is disabled)
                memcpy((void *)fbp, (void *)infodisplay->backbuf,
                       infodisplay->backbuf_size);
            }
        }

        usleep(SLEEP_MILLISECONDS_PER_FRAME * 1000);
        ++frame;
    }

    infodisplay_close(infodisplay);
    input_close(inputctx);

    memset(fbp, 0, fbfinfo.smem_len);

    munmap(fbp, fbfinfo.smem_len);
    close(fbfd);
    ret = vc_dispmanx_resource_delete(screen_resource);
    vc_dispmanx_display_close(display);

    return EXIT_SUCCESS;
}


static void catch_sig_quit(int signum)
{
    if (signum == SIGTERM ||
        signum == SIGABRT ||
        signum == SIGINT)
    {
        s_alive = 0;
    }
}

int main(int argc, char **argv)
{
    signal(SIGTERM, catch_sig_quit);
    signal(SIGABRT, catch_sig_quit);
    signal(SIGINT, catch_sig_quit);

    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("ramefbcp", LOG_NDELAY | LOG_PID, LOG_USER);

    for (int a = 1; a < argc; ++a)
    {
        if (argv[a] == NULL)
            continue;

        if (strcmp(argv[a], "-f") == 0)
        {
            if (a + 1 < argc && argv[a + 1] != NULL)
            {
                FILE *fp;
                s_ttf_filename = argv[a + 1];
                fp = fopen(s_ttf_filename, "rb");
                if (fp == NULL)
                {
                    const char *warnfmt = "Given font %s not found (trying default)\n";
                    syslog(LOG_WARNING, warnfmt, s_ttf_filename);
                    fprintf(stderr, warnfmt, s_ttf_filename);
                    s_ttf_filename = NULL;
                }
                else
                {
                    dbg_printf("Using font: %s\n", s_ttf_filename);
                    fclose(fp);
                }
                ++a;
                continue;
            }
        }

        #ifdef DEBUG_SUPPORT
        if (strcmp(argv[a], "-d") == 0)
            g_debug_info = 1;
        #endif

        if (strcmp(argv[a], "-h") == 0 ||
            strcmp(argv[a], "-?") == 0 ||
            strcmp(argv[a], "--help") == 0)
        {
            printf("ramefbcp version %d.%d.%d\n",
                   VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
            printf("Usage: (all parameters are optional)\n"
                   "  -f /path/font.ttf\n"
                   "     \t Use given font instead of built-in default.\n"
                   "     \t (default: " TTF_DEFAULT_FILENAME ")\n"
                   "  -d \t Output debug info to stdout. "
                       #ifdef DEBUG_SUPPORT
                       "(available)\n"
                       #else
                       "(NOT available)\n"
                       #endif
                   "  -h \t This usage info.\n");
            return EXIT_SUCCESS;
        }
    }

    return process();
}
