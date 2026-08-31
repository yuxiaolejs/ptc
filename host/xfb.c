// xfb.c - minimal X11 host: fixed-size framebuffer + non-blocking key polling
// build: gcc xfb.c -o xfb -lX11 -lXext
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../gui/include/gui_config.h"
#define HOST_FB_SCALE 2
#define HOST_FB_W (RAW_FB_W * HOST_FB_SCALE)
#define HOST_FB_H (RAW_FB_H * HOST_FB_SCALE)

// ---- your ported program's entry points ----
static void app_render(uint32_t *fb, int w, int h, int pitch_px, uint64_t frame);
static void app_key(unsigned long keysym, int down);

uint32_t *gui_fb;
int pitch_px;

int main(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
        fprintf(stderr, "no display\n");
        return 1;
    }
    int scr = DefaultScreen(dpy);
    Visual *vis = DefaultVisual(dpy, scr);
    int depth = DefaultDepth(dpy, scr);

    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0,
                                     HOST_FB_W, HOST_FB_H, 0,
                                     BlackPixel(dpy, scr), BlackPixel(dpy, scr));
    XStoreName(dpy, win, "fb");

    // pin the size so the WM can't resize out from under a fixed-res buffer
    XSizeHints *sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = sh->max_width = HOST_FB_W;
    sh->min_height = sh->max_height = HOST_FB_H;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);

    // let the close button produce an event instead of killing the connection
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask | ExposureMask);
    XMapWindow(dpy, win);

    // real key-up events instead of synthetic release/press repeat pairs
    Bool supported;
    XkbSetDetectableAutoRepeat(dpy, True, &supported);

    GC gc = XCreateGC(dpy, win, 0, NULL);

    // --- shared-memory XImage: img->data is the framebuffer ---
    int use_shm = XShmQueryExtension(dpy);
    XShmSegmentInfo shminfo;
    XImage *img;
    if (use_shm)
    {
        img = XShmCreateImage(dpy, vis, depth, ZPixmap, NULL, &shminfo, HOST_FB_W, HOST_FB_H);
        shminfo.shmid = shmget(IPC_PRIVATE,
                               (size_t)img->bytes_per_line * img->height,
                               IPC_CREAT | 0600);
        shminfo.shmaddr = img->data = shmat(shminfo.shmid, NULL, 0);
        shminfo.readOnly = False;
        XShmAttach(dpy, &shminfo);
        XSync(dpy, False);
        shmctl(shminfo.shmid, IPC_RMID, NULL); // reclaimed when everyone detaches
    }
    else
    {
        char *buf = calloc(1, (size_t)HOST_FB_W * HOST_FB_H * 4);
        img = XCreateImage(dpy, vis, depth, ZPixmap, 0, buf, HOST_FB_W, HOST_FB_H, 32, 0);
    }

    printf("bpp=%d bytes_per_line=%d masks R=%08lx G=%08lx B=%08lx\n",
           img->bits_per_pixel, img->bytes_per_line,
           img->red_mask, img->green_mask, img->blue_mask);

    uint32_t *fb = (uint32_t *)img->data; // <-- hand this to your program
    gui_fb = fb;
    pitch_px = img->bytes_per_line / 4; // may exceed HOST_FB_W; don't assume

    uint64_t frame = 0;
    int running = 1;
    while (running)
    {
        while (XPending(dpy))
        { // non-blocking drain
            XEvent e;
            XNextEvent(dpy, &e);
            switch (e.type)
            {
            case KeyPress:
            case KeyRelease:
            {
                KeySym ks = XkbKeycodeToKeysym(dpy, e.xkey.keycode, 0, 0);
                app_key((unsigned long)ks, e.type == KeyPress);
                break;
            }
            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == wm_delete)
                    running = 0;
                break;
            }
        }

        app_render(fb, RAW_FB_W, RAW_FB_H, pitch_px, frame++);

        if (use_shm)
            XShmPutImage(dpy, win, gc, img, 0, 0, 0, 0, HOST_FB_W, HOST_FB_H, False);
        else
            XPutImage(dpy, win, gc, img, 0, 0, 0, 0, HOST_FB_W, HOST_FB_H);
        XFlush(dpy);

        struct timespec ts = {0, 16 * 1000 * 1000}; // crude ~60 Hz pacing
        nanosleep(&ts, NULL);
    }

    if (use_shm)
    {
        XShmDetach(dpy, &shminfo);
        shmdt(shminfo.shmaddr);
    }
    XCloseDisplay(dpy);
    return 0;
}

void render_put_pixel(int x, int y, uint32_t color)
{
    for (int dy = y * HOST_FB_SCALE; dy < (y + 1) * HOST_FB_SCALE; dy++)
        for (int dx = x * HOST_FB_SCALE; dx < (x + 1) * HOST_FB_SCALE; dx++)
            gui_fb[dy * pitch_px + dx] = color;
}

// ---- demo stubs: replace with your ported code ----
static void app_render(uint32_t *fb, int w, int h, int pitch_px, uint64_t frame)
{
    printf("frame %lu, width=%d height=%d pitch=%d\n", frame, w, h, pitch_px);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            render_put_pixel(x, y, ((x + frame) & 0xff) << 16 | (y & 0xff) << 8 | 0x40);
    gui_draw_char('A', 10, 10, 0x00ff00);
}

static void app_key(unsigned long keysym, int down)
{
    printf("key %s: %lu\n", down ? "down" : "up", keysym);
}