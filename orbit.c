#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo-xlib.h>
#include <math.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PI 3.14159265358979323846
enum { WIDTH = 420, HEIGHT = 520, MAX_SECONDS = 359999 };

typedef struct {
    int duration;
    double left, deadline;
    bool running, done;
} Timer;

static double now(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec + time.tv_nsec / 1e9;
}
static void reset(Timer *t) {
    t->left = t->duration;
    t->running = t->done = false;
}
static void toggle(Timer *t, double time) {
    if (t->running) {
        t->left = fmax(0, t->deadline - time);
        t->running = false;
    } else {
        if (t->left <= 0) reset(t);
        t->deadline = time + t->left;
        t->running = true;
    }
}
static bool tick(Timer *t, double time) {
    if (!t->running) return false;
    t->left = fmax(0, t->deadline - time);
    if (t->left > 0) return false;
    t->running = false;
    t->done = true;
    return true;
}
static void adjust(Timer *t, int seconds, bool preset) {
    if (t->running) return;
    int value = preset ? seconds : t->duration + seconds;
    t->duration = value < 1 ? 1 : value > MAX_SECONDS ? MAX_SECONDS : value;
    reset(t);
}
static void color(cairo_t *cr, unsigned hex) {
    cairo_set_source_rgb(cr, (hex >> 16) / 255.0, ((hex >> 8) & 255) / 255.0,
                         (hex & 255) / 255.0);
}
static void label(cairo_t *cr, const char *value, double x, double y,
                  double size, unsigned ink, bool bold, bool mono) {
    cairo_text_extents_t bounds;
    cairo_select_font_face(cr, mono ? "monospace" : "sans",
                           CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD :
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, value, &bounds);
    cairo_move_to(cr, x - bounds.x_bearing - bounds.width / 2, y);
    color(cr, ink);
    cairo_show_text(cr, value);
}
static void button(cairo_t *cr, const char *value, int x, int y,
                   int w, int h, unsigned bg, unsigned ink) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-10, y+10, 10, -PI/2, 0);
    cairo_arc(cr, x+w-10, y+h-10, 10, 0, PI/2);
    cairo_arc(cr, x+10, y+h-10, 10, PI/2, PI);
    cairo_arc(cr, x+10, y+10, 10, PI, 3*PI/2);
    cairo_close_path(cr);
    color(cr, bg);
    cairo_fill(cr);
    label(cr, value, x+w/2.0, y+h/2.0+5, 14, ink, true, false);
}
static double layout_scale(int width, int height) {
    return fmin(width / (double)WIDTH, height / (double)HEIGHT);
}
static void draw(cairo_t *cr, const Timer *t, int width, int height) {
    const unsigned accent = t->done ? 0xf6c562 : 0x74e8d1;
    const unsigned text = 0xeaf2ff, quiet = 0x9aa9c5;
    const int remaining = (int)ceil(t->left);
    char digits[16];
    if (remaining >= 3600)
        snprintf(digits, sizeof digits, "%02d:%02d:%02d",
                 remaining / 3600, (remaining / 60) % 60, remaining % 60);
    else
        snprintf(digits, sizeof digits, "%02d:%02d", remaining / 60, remaining % 60);
    cairo_save(cr);
    color(cr, 0x101525);
    cairo_paint(cr);
    const double scale = layout_scale(width, height);
    cairo_translate(cr, (width - WIDTH*scale)/2, (height - HEIGHT*scale)/2);
    cairo_scale(cr, scale, scale);
    label(cr, "ORBIT TIMER", 210, 43, 17, text, true, false);
    cairo_set_line_width(cr, 10);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    color(cr, 0x303951);
    cairo_arc(cr, 210, 200, 115, -PI/2, 3*PI/2);
    cairo_stroke(cr);
    if (t->left > 0) {
        color(cr, accent);
        cairo_arc_negative(cr, 210, 200, 115, -PI/2, -PI/2 - 2*PI*t->left/t->duration);
        cairo_stroke(cr);
    }
    label(cr, digits, 210, 215, remaining >= 3600 ? 38 : 54, text, true, true);
    label(cr, t->done ? "TIME'S UP" : t->running ? "COUNTING DOWN" :
          t->left < t->duration ? "PAUSED" : "READY", 210, 245, 12,
          t->done ? accent : quiet, true, false);
    const unsigned edit = t->running ? 0x71809e : text;
    button(cr, "-1 min", 40, 337, 76, 34, 0x222d44, edit);
    button(cr, "+1 min", 128, 337, 76, 34, 0x222d44, edit);
    button(cr, "-10 s", 216, 337, 76, 34, 0x222d44, edit);
    button(cr, "+10 s", 304, 337, 76, 34, 0x222d44, edit);
    button(cr, "1 min", 40, 383, 105, 34, 0x222d44, edit);
    button(cr, "5 min", 158, 383, 104, 34, 0x222d44, edit);
    button(cr, "25 min", 275, 383, 105, 34, 0x222d44, edit);
    button(cr, "Reset", 40, 436, 105, 44, 0x28354e, text);
    button(cr, t->running ? "Pause" : t->left > 0 && t->left < t->duration ?
           "Resume" : "Start", 158, 436, 222, 44, accent, 0x101525);
    label(cr, "Space  start / pause     R  reset", 210, 505, 12, quiet, false, false);
    cairo_restore(cr);
}
static void click(Timer *t, int x, int y) {
    if (y >= 436 && y < 480) {
        if (x >= 40 && x < 145) reset(t);
        if (x >= 158 && x < 380) toggle(t, now());
    }
    if (y >= 337 && y < 371) {
        const int values[] = {-60, 60, -10, 10};
        for (int i = 0; i < 4; ++i)
            if (x >= 40 + i*88 && x < 116 + i*88) adjust(t, values[i], false);
    }
    if (y >= 383 && y < 417) {
        if (x >= 40 && x < 145) adjust(t, 60, true);
        if (x >= 158 && x < 262) adjust(t, 300, true);
        if (x >= 275 && x < 380) adjust(t, 1500, true);
    }
}
static int self_test(void) {
    Timer t = {.duration = 5};
    reset(&t);
    toggle(&t, 10);
    if (!t.running || tick(&t, 12) || t.left != 3) return 1;
    toggle(&t, 12);
    if (t.running || tick(&t, 99) || t.left != 3) return 1;
    toggle(&t, 100);
    if (!tick(&t, 103) || !t.done || tick(&t, 104)) return 1;
    toggle(&t, 105);
    if (!t.running || t.done || t.deadline != 110) return 1;
    reset(&t);
    click(&t, 175, 397);
    if (t.duration != 300 || t.left != 300) return 1;
    adjust(&t, -1000, false);
    if (t.duration != 1) return 1;
    puts("Timer self-test passed");
    return 0;
}
int main(int argc, char **argv) {
    Timer timer = {.duration = 300};
    reset(&timer);
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test();
    if (argc == 2 && !strcmp(argv[1], "--preview")) {
        cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
        cairo_t *cr = cairo_create(surface);
        draw(cr, &timer, WIDTH, HEIGHT);
        cairo_status_t status = cairo_surface_write_to_png(surface, "orbit-preview.png");
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return status != CAIRO_STATUS_SUCCESS;
    }
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fputs("Orbit Timer needs an X11 or XWayland desktop session.\n", stderr);
        return 1;
    }
    int screen = DefaultScreen(display), width = WIDTH, height = HEIGHT;
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                         0, 0, width, height, 0, 0, 0x101525);
    XStoreName(display, window, "Orbit Timer");
    XClassHint class_hint = {.res_name = "orbit-timer", .res_class = "OrbitTimer"};
    XSetClassHint(display, window, &class_hint);
    XSelectInput(display, window, ExposureMask | StructureNotifyMask | ButtonPressMask | KeyPressMask);
    Atom close = XInternAtom(display, "WM_DELETE_WINDOW", False);
    Atom protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    XSetWMProtocols(display, window, &close, 1);
    XMapWindow(display, window);
    cairo_surface_t *surface = cairo_xlib_surface_create(display, window,
                                    DefaultVisual(display, screen), width, height);
    cairo_t *cr = cairo_create(surface);
    struct pollfd display_fd = {.fd = ConnectionNumber(display), .events = POLLIN};
    bool open = true, redraw = true;
    while (open) {
        while (XPending(display)) {
            if (tick(&timer, now())) XBell(display, 100);
            XEvent event;
            XNextEvent(display, &event);
            switch (event.type) {
            case ClientMessage:
                if (event.xclient.message_type == protocols &&
                    (Atom)event.xclient.data.l[0] == close) open = false;
                break;
            case ConfigureNotify:
                width = event.xconfigure.width;
                height = event.xconfigure.height;
                cairo_xlib_surface_set_size(surface, width, height);
                break;
            case ButtonPress:
                if (event.xbutton.button == Button1) {
                    double scale = layout_scale(width, height);
                    int x = (int)((event.xbutton.x - (width - WIDTH*scale)/2) / scale);
                    int y = (int)((event.xbutton.y - (height - HEIGHT*scale)/2) / scale);
                    click(&timer, x, y);
                }
                break;
            case KeyPress:
                switch (XLookupKeysym(&event.xkey, 0)) {
                case XK_space: toggle(&timer, now()); break;
                case XK_r: reset(&timer); break;
                case XK_Up: adjust(&timer, 60, false); break;
                case XK_Down: adjust(&timer, -60, false); break;
                case XK_Right: adjust(&timer, 1, false); break;
                case XK_Left: adjust(&timer, -1, false); break;
                case XK_Escape: open = false; break;
                }
                break;
            }
            redraw = true;
        }
        if (!open) break;
        if (timer.running) {
            if (tick(&timer, now())) XBell(display, 100);
            redraw = true;
        }
        if (redraw) {
            draw(cr, &timer, width, height);
            cairo_surface_flush(surface);
            XFlush(display);
            redraw = false;
        }
        poll(&display_fd, 1, timer.running ? 40 : -1);
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
