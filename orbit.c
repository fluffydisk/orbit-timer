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
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x+w-11, y);
    cairo_line_to(cr, x+w, y+11);
    cairo_line_to(cr, x+w, y+h);
    cairo_line_to(cr, x+11, y+h);
    cairo_line_to(cr, x, y+h-11);
    cairo_close_path(cr);
    color(cr, bg);
    cairo_fill_preserve(cr);
    color(cr, 0x37516a);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    label(cr, value, x+w/2.0, y+h/2.0+5, 14, ink, true, false);
}
static double layout_scale(int width, int height) {
    return fmin(width / (double)WIDTH, height / (double)HEIGHT);
}
static void orbit_arc(cairo_t *cr, double radius, double angle, double length) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, 210, 196, radius, angle, angle + length);
    cairo_stroke(cr);
}

static void dial(cairo_t *cr, const Timer *t, unsigned accent) {
    const double fraction = t->left / t->duration;
    const double elapsed = t->duration - t->left;
    const double sweep = 2*PI/72;
    cairo_pattern_t *halo = cairo_pattern_create_radial(210,196,10,210,196,144);
    cairo_pattern_add_color_stop_rgba(halo, 0, 0.10, 0.23, 0.33, 0.67);
    cairo_pattern_add_color_stop_rgba(halo, 0.75, 0.08, 0.14, 0.25, 0.35);
    cairo_pattern_add_color_stop_rgba(halo, 1, 0.03, 0.05, 0.09, 0);
    cairo_set_source(cr, halo);
    cairo_paint(cr);
    cairo_pattern_destroy(halo);

    cairo_set_line_width(cr, 1);
    color(cr, 0x304057);
    orbit_arc(cr, 132, 0, 2*PI);
    orbit_arc(cr, 98, 0, 2*PI);
    for (int i = 0; i < 72; ++i) {
        double angle = -PI/2 + i*sweep;
        double active = fmin(0.68, 72*fraction - i);
        cairo_set_line_width(cr, i % 6 == 0 ? 11 : 7);
        color(cr, 0x263450);
        orbit_arc(cr, 113, angle, sweep*0.68);
        if (active > 0) {
            color(cr, accent);
            orbit_arc(cr, 113, angle, sweep*active);
        }
    }
    for (int i = 0; i < 3; ++i) {
        double phase = elapsed*(i == 1 ? -1.1 : 0.75) + i*2.1 - PI/2;
        if (i == 1) color(cr, 0xb69cff);
        else color(cr, accent);
        cairo_set_line_width(cr, i == 0 ? 3 : 2);
        orbit_arc(cr, i == 1 ? 125 : 137, phase, i == 0 ? 0.43 : 0.13);
    }
}

static void draw(cairo_t *cr, const Timer *t, int width, int height) {
    const unsigned accent = t->done ? 0xff8a65 : t->running ? 0x64ffe0 : 0x77cfff;
    const unsigned text = 0xe8fbff, quiet = 0xa5b8d0;
    const int remaining = (int)ceil(t->left);
    char digits[16];
    if (remaining >= 3600)
        snprintf(digits, sizeof digits, "%02d:%02d:%02d",
                 remaining / 3600, (remaining / 60) % 60, remaining % 60);
    else
        snprintf(digits, sizeof digits, "%02d:%02d", remaining / 60, remaining % 60);
    cairo_save(cr);
    color(cr, 0x080e1c);
    cairo_paint(cr);
    const double scale = layout_scale(width, height);
    cairo_translate(cr, (width - WIDTH*scale)/2, (height - HEIGHT*scale)/2);
    cairo_scale(cr, scale, scale);
    color(cr, 0x17243a);
    cairo_set_line_width(cr, 1);
    for (int x = 18; x < WIDTH; x += 24) {
        cairo_move_to(cr, x, 59);
        cairo_line_to(cr, x, 323);
    }
    for (int y = 59; y <= 323; y += 24) {
        cairo_move_to(cr, 18, y);
        cairo_line_to(cr, 402, y);
    }
    cairo_stroke(cr);
    label(cr, "ORBIT TIMER", 104, 38, 17, text, true, true);
    label(cr, "3010", 354, 38, 16, accent, true, true);
    dial(cr, t, accent);
    label(cr, digits, 210, 211, remaining >= 3600 ? 34 : 53, text, true, true);
    label(cr, t->done ? "TIME'S UP" : t->running ? "COUNTING DOWN" :
          t->left < t->duration ? "PAUSED" : "READY", 210, 242, 12,
          accent, true, true);
    const unsigned edit = t->running ? 0x71809e : text;
    button(cr, "-1 min", 40, 337, 76, 34, 0x101e33, edit);
    button(cr, "+1 min", 128, 337, 76, 34, 0x101e33, edit);
    button(cr, "-10 s", 216, 337, 76, 34, 0x101e33, edit);
    button(cr, "+10 s", 304, 337, 76, 34, 0x101e33, edit);
    button(cr, "1 min", 40, 383, 105, 34, 0x101e33, edit);
    button(cr, "5 min", 158, 383, 104, 34, 0x101e33, edit);
    button(cr, "25 min", 275, 383, 105, 34, 0x101e33, edit);
    button(cr, "Reset", 40, 436, 105, 44, 0x182842, text);
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
static int preview(Timer timer, const char *state);

int main(int argc, char **argv) {
    Timer timer = {.duration = 300};
    reset(&timer);
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test();
    if ((argc == 2 || argc == 3) && !strcmp(argv[1], "--preview"))
        return preview(timer, argc == 3 ? argv[2] : "ready");
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fputs("Orbit Timer needs an X11 or XWayland desktop session.\n", stderr);
        return 1;
    }
    int screen = DefaultScreen(display), width = WIDTH, height = HEIGHT;
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                         0, 0, width, height, 0, 0, 0x101525);
    XStoreName(display, window, "Orbit Timer 3010");
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


static int preview(Timer timer, const char *state) {
    if (!strcmp(state, "running")) {
        timer.running = true;
        timer.left = 186;
    } else if (!strcmp(state, "finished")) {
        timer.done = true;
        timer.left = 0;
    } else if (strcmp(state, "ready")) {
        return 2;
    }
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
    cairo_t *cr = cairo_create(surface);
    draw(cr, &timer, WIDTH, HEIGHT);
    cairo_status_t status = cairo_surface_write_to_png(surface, "orbit-preview.png");
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return status != CAIRO_STATUS_SUCCESS;
}
