
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


#define TTF_FILENAME "/usr/share/fonts/TTF/ramefbcp.ttf"

#define SLEEP_MILLISECONDS_PER_FRAME 25

// Scale input video to rect with this aspect ratio:
#define VID_ASPECT_W 16
#define VID_ASPECT_H 9


static struct fb_var_screeninfo fbvinfo;
static struct fb_fix_screeninfo fbfinfo;

static int s_frame = 0, s_alive = 1;

static int s_width = 0, s_height = 0;

static INFODISPLAY *infodisplay = NULL;

static INPUT_CTX *inputctx;


static void print_fb_info(struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo)
{
#ifdef DEBUG_SUPPORT
    if (!g_debug_info)
        return;

    printf("*** fb_fix_screeninfo ***\n");
    printf("id: %s\n", finfo->id); // is it safe to assume there's 0-terminator?
    printf("smem_start: %08x\n", (unsigned int)finfo->smem_start);
    printf("smem_len: %u\n", finfo->smem_len);
    printf("type: %u\n", finfo->type);
    printf("type_aux: %u\n", finfo->type_aux);
    printf("visual: %u\n", finfo->visual);
    printf("xpanstep: %u\n", finfo->xpanstep);
    printf("ypanstep: %u\n", finfo->ypanstep);
    printf("ywrapstep: %u\n", finfo->ywrapstep);
    printf("line_length: %u\n", finfo->line_length);
    printf("mmio_start: %08x\n", (unsigned int)finfo->mmio_start);
    printf("mmio_len: %u\n", finfo->mmio_len);
    printf("accel: %u\n", finfo->accel);
    printf("capabilities: %04x\n", finfo->capabilities);
    printf("*** fb_var_screeninfo ***\n");
    printf("xres: %u\n", vinfo->xres);
    printf("yres: %u\n", vinfo->yres);
    printf("xres_virtual: %u\n", vinfo->xres_virtual);
    printf("yres_virtual: %u\n", vinfo->yres_virtual);
    printf("xoffset: %u\n", vinfo->xoffset);
    printf("yoffset: %u\n", vinfo->yoffset);
    printf("bits_per_pixel: %u\n", vinfo->bits_per_pixel);
    printf("grayscale: %u\n", vinfo->grayscale);
    printf("red.offset,length,msb_right: %u,%u,%u\n", vinfo->red.offset, vinfo->red.length, vinfo->red.msb_right);
    printf("green.offset,length,msb_right: %u,%u,%u\n", vinfo->green.offset, vinfo->green.length, vinfo->green.msb_right);
    printf("blue.offset,length,msb_right: %u,%u,%u\n", vinfo->blue.offset, vinfo->blue.length, vinfo->blue.msb_right);
    printf("transp.offset,length,msb_right: %u,%u,%u\n", vinfo->transp.offset, vinfo->transp.length, vinfo->transp.msb_right);
    printf("nonstd: %u\n", vinfo->nonstd);
    printf("activate: %u\n", vinfo->activate);
    printf("height: %u\n", vinfo->height);
    printf("width: %u\n", vinfo->width);
    printf("accel_flags: %08x\n", vinfo->accel_flags);
    printf("pixclock: %u\n", vinfo->pixclock);
    printf("left_margin: %u\n", vinfo->left_margin);
    printf("right_margin: %u\n", vinfo->right_margin);
    printf("upper_margin: %u\n", vinfo->upper_margin);
    printf("lower_margin: %u\n", vinfo->lower_margin);
    printf("hsync_len: %u\n", vinfo->hsync_len);
    printf("vsync_len: %u\n", vinfo->vsync_len);
    printf("sync: %u\n", vinfo->sync);
    printf("vmode: %u\n", vinfo->vmode);
    printf("rotate: %u\n", vinfo->rotate);
    printf("colorspace: %u\n", vinfo->colorspace);
    printf("---\n");
#endif
}


static void line_to_infodisplay(INFODISPLAY *infodisplay, const char *line)
{
    switch (line[0])
    {
        case 'X':
        {
            // text row, e.g. "X1:Please wait..."
            int rown = line[1] - '1';
            if (rown >= 0 && rown < INFODISPLAY_ROW_COUNT &&
                line[2] == ':')
                infodisplay_set_textrow(infodisplay, rown, &line[3]);
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
                infodisplay_set_status(infodisplay, (INFODISPLAY_ICON)value);
        }
        break;

        case 'T':
        {
            // set times (playing & total), values in milliseconds
            // e.g. "T:5100,90000" (comma and second value are optional)
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
            infodisplay_set_times(infodisplay, t1, t2);
        }
        break;

    }
}


static int process()
{
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_MODEINFO_T display_info;
    DISPMANX_RESOURCE_HANDLE_T screen_resource;
    VC_IMAGE_TRANSFORM_T transform;
    uint32_t image_prt;
    VC_RECT_T rect1;
    int ret;
    int fbfd = 0;
    char *fbp = 0;
    int vid_w, vid_h;


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

    //s_width = fbvinfo.xres;
    s_width = fbfinfo.line_length / (fbvinfo.bits_per_pixel / 8);
    s_height = fbvinfo.yres;

    inputctx = input_create(fileno(stdin));

    // only 16bpp is supported for now:
    if (fbvinfo.bits_per_pixel == 16)
    {
        infodisplay = infodisplay_create(s_width, s_height - vid_h,
                                         fbvinfo.red.offset, fbvinfo.red.length,
                                         fbvinfo.green.offset, fbvinfo.green.length,
                                         fbvinfo.blue.offset, fbvinfo.blue.length,
                                         fbvinfo.transp.offset, fbvinfo.transp.length,
                                         TTF_FILENAME);
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
        int x, y, read_status;

        ret = vc_dispmanx_snapshot(display, screen_resource, 0);
        vc_dispmanx_resource_read_data(screen_resource, &rect1, fbp,
                                       fbvinfo.xres * fbvinfo.bits_per_pixel / 8);

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

                    line_to_infodisplay(infodisplay, line);
                    need_to_refresh_display = 1;
                    try_read_more = 1;
                }
            } while (try_read_more);
        }

        if (infodisplay != NULL && need_to_refresh_display)
        {
            /*
            // hardcoded infodisplay update test:
            infodisplay_set_progress(infodisplay, (float)s_frame * 40 / (1*60*1000+32*1000+100) );
            infodisplay_set_textrow(infodisplay, 0, "xx_zz_nnn_r720P.mp4");
            infodisplay_set_status(infodisplay, INFODISPLAY_ICON_PLAYING);
            infodisplay_set_times(infodisplay, s_frame * 40,
                                  345*60*60*1000 + 45*60*1000+32*1000+100);
            */
            infodisplay_update(infodisplay);
            // update infodisplay backbuffer to bottom part of the screen
            memcpy(fbp + vid_h * fbfinfo.line_length,
                   infodisplay->backbuf, infodisplay->backbuf_size);
        }

        usleep(SLEEP_MILLISECONDS_PER_FRAME * 1000);
        ++s_frame;
    }

    infodisplay_close(infodisplay);

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
        #ifdef DEBUG_SUPPORT
        if (argv[a] != NULL && strcmp(argv[a], "-d") == 0)
            g_debug_info = 1;
        #endif

        if (argv[a] != NULL &&
            (strcmp(argv[a], "-h") == 0 ||
             strcmp(argv[a], "-?") == 0 ||
             strcmp(argv[a], "--help") == 0))
        {
            printf("Usage:\n"
                   "  -d  Output debug info to stdout. "
                       #ifdef DEBUG_SUPPORT
                       "(available)\n"
                       #else
                       "(NOT available)\n"
                       #endif
                   "  -h  This usage info.\n");
        }
    }

    return process();
}
