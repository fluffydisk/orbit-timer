#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo-xlib.h>
#include <math.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "chess.h"
#include <string.h>
#include <time.h>

#define PI 3.14159265358979323846
enum { WIDTH = 420, HEIGHT = 520, MAX_SECONDS = 359999 };

typedef struct {
    int duration;
    double left, deadline;
    bool running, done, repeat, stopwatch, muted;
    unsigned cycles, lap_count;
    double laps[3];
} Timer;

static double now(void) {
    struct timespec time;
    clock_gettime(CLOCK_BOOTTIME, &time);
    return time.tv_sec + time.tv_nsec / 1e9;
}
static void reset(Timer *t) {
    t->left = t->stopwatch ? 0 : t->duration;
    t->cycles = t->lap_count = 0;
    t->running = t->done = false;
}
static void toggle(Timer *t, double time) {
    if (t->running) {
        t->left = t->stopwatch ? fmax(0, time - t->deadline) : fmax(0, t->deadline - time);
        t->running = false;
    } else {
        if (!t->stopwatch && t->left <= 0) reset(t);
        t->deadline = time + (t->stopwatch ? -t->left : t->left);
        t->running = true;
    }
}
static bool tick(Timer *t, double time) {
    if (!t->running) return false;
    if (t->stopwatch) {
        t->left = fmin(MAX_SECONDS, fmax(0, time - t->deadline));
        if (t->left >= MAX_SECONDS) t->running = false;
        return false;
    }
    t->left = fmax(0, t->deadline - time);
    if (t->left > 0) return false;
    if (t->repeat) {
        double cycles = floor((time - t->deadline) / t->duration) + 1;
        t->cycles += (unsigned)cycles;
        t->deadline += cycles * t->duration;
        t->left = t->deadline - time;
    } else {
        t->running = false;
        t->done = true;
    }
    return true;
}
static void adjust(Timer *t, int seconds, bool preset) {
    if (t->running || t->stopwatch) return;
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
    const double fraction = t->stopwatch ? fmod(t->left,60)/60 : t->left/t->duration;
    const double elapsed = t->stopwatch ? t->left : t->duration - t->left;
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

static void draw(cairo_t *cr, const Timer *t, int width, int height, const char *entry, bool pinned) {
    const unsigned accent = t->done ? 0xff8a65 : t->running ? 0x64ffe0 : 0x77cfff;
    const unsigned text = 0xe8fbff, quiet = 0xa5b8d0;
    const int remaining = (int)(t->stopwatch ? floor(t->left) : ceil(t->left));
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
    label(cr, t->done ? "TIME'S UP" : t->running ? (t->stopwatch ? "STOPWATCH" : "COUNTING DOWN") :
          (t->stopwatch ? t->left>0 : t->left<t->duration) ? "PAUSED" : "READY", 210, 242, 12,
          accent, true, true);
    if (t->stopwatch) {
        char precision[8];
        snprintf(precision,sizeof precision,".%02d",(int)(fmod(t->left,1)*100));
        label(cr,precision,210,267,15,quiet,true,true);
        char lap[32];
        for (unsigned i=0; i<3 && i<t->lap_count; ++i) {
            snprintf(lap,sizeof lap,"Lap %u  %.2fs",t->lap_count-i,t->laps[i]);
            label(cr,lap,210,349+i*23,13,quiet,false,true);
        }
        button(cr,"Lap",275,383,105,34,0x101e33,text);
    } else if (entry) {
        label(cr,entry[0]?entry:"HHMMSS",210,356,24,accent,true,true);
        label(cr,"Type HHMMSS   Enter to set   Esc to cancel",210,400,12,text,false,true);
    } else {
        button(cr,"Set duration",40,337,116,34,0x101e33,text);
        button(cr,"+1 min",165,337,100,34,0x101e33,text);
        button(cr,t->repeat?"Repeat: on":"Repeat: off",274,337,106,34,0x101e33,t->repeat?accent:text);
        button(cr,"1 min",40,383,105,34,0x101e33,text);
        button(cr,"5 min",158,383,104,34,0x101e33,text);
        button(cr,"25 min",275,383,105,34,0x101e33,text);
        if (t->cycles) {
            char cycle[32];
            snprintf(cycle,sizeof cycle,"Rounds completed: %u",t->cycles);
            label(cr,cycle,210,268,11,quiet,false,true);
        }
    }
    button(cr, "Reset", 40, 436, 105, 44, 0x182842, text);
    button(cr, t->running ? "Pause" : t->left > 0 && (t->stopwatch || t->left < t->duration) ?
           "Resume" : "Start", 158, 436, 222, 44, accent, 0x101525);
    button(cr,t->stopwatch?"Timer":"Stopwatch",40,489,105,24,0x101e33,text);
    button(cr,t->muted?"Sound: off":"Sound: on",158,489,104,24,0x101e33,text);
    button(cr,pinned?"Pinned":"Pin window",275,489,105,24,0x101e33,text);
    cairo_restore(cr);
}
static void add_minute(Timer *t) {
    if (t->running) {
        double extra = fmin(60, MAX_SECONDS-t->duration);
        t->duration += (int)extra;
        t->deadline += extra;
        t->left += extra;
    } else if (!t->done && t->left<t->duration) {
        double extra = fmin(60, MAX_SECONDS-t->duration);
        t->duration += (int)extra;
        t->left += extra;
    } else {
        adjust(t,60,false);
    }
}
static void lap(Timer *t) {
    if (!t->stopwatch || !t->running) return;
    t->laps[2]=t->laps[1];
    t->laps[1]=t->laps[0];
    t->laps[0]=t->left;
    ++t->lap_count;
}
static int seconds_from_entry(const char *entry) {
    int value=atoi(entry);
    int minutes=(value/100)%100, seconds=value%100;
    if (strlen(entry)<=2) return seconds;
    if (minutes>=60 || seconds>=60) return 0;
    return value/10000*3600+minutes*60+seconds;
}
static void request_pin(Display *display, Window window, int screen, bool pin) {
    XEvent event={0};
    event.xclient.type=ClientMessage;
    event.xclient.window=window;
    event.xclient.message_type=XInternAtom(display,"_NET_WM_STATE",False);
    event.xclient.format=32;
    event.xclient.data.l[0]=pin;
    event.xclient.data.l[1]=XInternAtom(display,"_NET_WM_STATE_ABOVE",False);
    event.xclient.data.l[3]=1;
    XSendEvent(display,RootWindow(display,screen),False,
               SubstructureRedirectMask|SubstructureNotifyMask,&event);
}
static void chess_draw(cairo_t *cr, const Chess *g, int width, int height, int promotion) {
    static const char *pieces[]={"","♟","♞","♝","♜","♛","♚"};
    color(cr,0x080e1c);
    cairo_paint(cr);
    double scale=layout_scale(width,height);
    cairo_translate(cr,(width-WIDTH*scale)/2,(height-HEIGHT*scale)/2);
    cairo_scale(cr,scale,scale);
    label(cr,"ORBIT CHESS",210,44,23,0x77cfff,true,true);
    for (int i=0; i<64; ++i) {
        int x=42+(i%8)*42, y=86+(i/8)*42;
        unsigned square=(i%8+i/8)%2?0x304968:0x5b7591;
        if (i==g->from || i==g->to) square=0x497f80;
        if (i==g->selected) square=0x5dbdb1;
        color(cr,square);
        cairo_rectangle(cr,x,y,42,42);
        cairo_fill(cr);
        int piece=g->board[i];
        label(cr,pieces[abs(piece)],x+21,y+32,31,piece>0?0xf4fcff:0x07111d,false,false);
    }
    bool check=checked(g,g->side), can_move=chess_has_move(g);
    const char *status=promotion>=0?"Promote with Q, R, B or N":
        !can_move?(check?(g->side>0?"Checkmate · Black wins":"Checkmate · White wins"):"Draw · Stalemate"):
        g->halfmoves>=150?"Draw · 75-move rule":
        g->side>0?(check?"White to move · Check":"White to move"):
                  (check?"Black to move · Check":"Black to move");
    label(cr,status,210,454,15,0xe8fbff,true,false);
    label(cr,"Esc  timer     Ctrl+R  new game",210,487,12,0xa5b8d0,false,true);
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
    adjust(&t, 300, true);
    if (t.duration != 300 || t.left != 300) return 1;
    adjust(&t, -1000, false);
    if (t.duration != 1) return 1;
    t=(Timer){.duration=5,.repeat=true};
    reset(&t);
    toggle(&t,10);
    if (!tick(&t,21) || t.cycles!=2 || !t.running || t.left!=4) return 1;
    t=(Timer){.stopwatch=true};
    reset(&t);
    toggle(&t,10);
    tick(&t,13);
    lap(&t);
    toggle(&t,14);
    if (t.left!=4 || t.lap_count!=1 || t.laps[0]!=3) return 1;
    if (seconds_from_entry("011503")!=4503 || seconds_from_entry("006003") ||
        !chess_test()) return 1;
    puts("Timer and chess self-tests passed");
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
    cairo_surface_t *frame = NULL;
    int frame_width=0, frame_height=0;
    Chess game;
    chess_reset(&game);
    Timer alternate={.duration=300,.stopwatch=true};
    bool open = true, redraw = true, chess=false, pinned=false, editing=false;
    int promotion=-1;
    char entry[7]="";
    unsigned entry_len=0;
    while (open) {
        while (XPending(display)) {
            if (tick(&timer, now()) && !timer.muted) XBell(display, 100);
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
                    if (chess) {
                        if (promotion>=0 || game.halfmoves>=150 || x<42 || x>=378 || y<86 || y>=422)
                            break;
                        int square=(y-86)/42*8+(x-42)/42;
                        if (game.board[square]*game.side>0) game.selected=square;
                        else if (game.selected>=0) {
                            Chess probe=game;
                            if (!chess_move(&probe,game.selected,square,QUEEN)) break;
                            if (abs(game.board[game.selected])==PAWN && (square/8==0 || square/8==7))
                                promotion=square;
                            else game=probe;
                        }
                        break;
                    }
                    if (y>=489 && y<513) {
                        if (x>=40 && x<145) {
                            Timer previous=timer;
                            timer=alternate;
                            alternate=previous;
                            timer.muted=previous.muted;
                            editing=false;
                        } else if (x>=158 && x<262) timer.muted=!timer.muted;
                        else if (x>=275 && x<380) {
                            pinned=!pinned;
                            request_pin(display,window,screen,pinned);
                        }
                    } else if (editing) {
                        break;
                    } else if (y>=436 && y<480) {
                        if (x>=40 && x<145) reset(&timer);
                        if (x>=158 && x<380) toggle(&timer,now());
                    } else if (y>=337 && y<371 && !timer.stopwatch) {
                        if (x>=40 && x<156 && !timer.running) {
                            editing=true;
                            entry[0]=0;
                            entry_len=0;
                        }
                        if (x>=165 && x<265) add_minute(&timer);
                        if (x>=274 && x<380) timer.repeat=!timer.repeat;
                    } else if (y>=383 && y<417) {
                        if (timer.stopwatch) {
                            if (x>=275 && x<380) lap(&timer);
                        } else {
                            if (x>=40 && x<145) adjust(&timer,60,true);
                            if (x>=158 && x<262) adjust(&timer,300,true);
                            if (x>=275 && x<380) adjust(&timer,1500,true);
                        }
                    }
                }
                break;
            case KeyPress: {
                KeySym key=XLookupKeysym(&event.xkey,0);
                bool ctrl=(event.xkey.state&ControlMask)!=0;
                if (ctrl && (event.xkey.state&ShiftMask) && key==XK_c) {
                    chess=!chess;
                    break;
                }
                if (chess) {
                    if (key==XK_Escape) chess=false;
                    else if (ctrl && key==XK_r) {
                        chess_reset(&game);
                        promotion=-1;
                    } else if (promotion>=0) {
                        int piece=key==XK_q?QUEEN:key==XK_r?ROOK:key==XK_b?BISHOP:key==XK_n?KNIGHT:0;
                        if (piece && chess_move(&game,game.selected,promotion,piece)) promotion=-1;
                    }
                    break;
                }
                if (editing) {
                    KeySym typed=XLookupKeysym(&event.xkey, (event.xkey.state&ShiftMask) ? 1 : 0);
                    if (typed>=XK_KP_0 && typed<=XK_KP_9) typed=typed-XK_KP_0+XK_0;
                    if (typed>=XK_0 && typed<=XK_9 && entry_len<6) {
                        entry[entry_len++]=(char)('0'+typed-XK_0);
                        entry[entry_len]=0;
                    } else if (key==XK_BackSpace && entry_len) entry[--entry_len]=0;
                    else if (key==XK_Escape) editing=false;
                    else if (key==XK_Return || key==XK_KP_Enter) {
                        int seconds=seconds_from_entry(entry);
                        if (seconds>0) {
                            adjust(&timer,seconds,true);
                            editing=false;
                        } else {
                            XBell(display,40);
                        }
                    }
                    break;
                }
                switch (key) {
                case XK_space: toggle(&timer, now()); break;
                case XK_r: reset(&timer); break;
                case XK_l: lap(&timer); break;
                case XK_m: timer.muted=!timer.muted; break;
                case XK_t:
                    if (!timer.stopwatch && !timer.running) {
                        editing=true;
                        entry[0]=0;
                        entry_len=0;
                    }
                    break;
                case XK_Up: adjust(&timer, 60, false); break;
                case XK_Down: adjust(&timer, -60, false); break;
                case XK_Right: adjust(&timer, 1, false); break;
                case XK_Left: adjust(&timer, -1, false); break;
                case XK_Escape: open = false; break;
                }
                break;
            }
            }
            redraw = true;
        }
        if (!open) break;
        bool alarm=tick(&alternate,now());
        if (timer.running) {
            alarm=tick(&timer,now()) || alarm;
            redraw = true;
        }
        if (alarm && !timer.muted) XBell(display,100);
        if (redraw) {
            if (!frame || width!=frame_width || height!=frame_height) {
                if (frame) cairo_surface_destroy(frame);
                frame=cairo_image_surface_create(CAIRO_FORMAT_RGB24,width,height);
                frame_width=width;
                frame_height=height;
            }
            cairo_t *paint=cairo_create(frame);
            if (chess) chess_draw(paint,&game,width,height,promotion);
            else draw(paint,&timer,width,height,editing?entry:NULL,pinned);
            cairo_status_t status=cairo_status(paint);
            cairo_destroy(paint);
            if (status==CAIRO_STATUS_SUCCESS) {
                cairo_set_operator(cr,CAIRO_OPERATOR_SOURCE);
                cairo_set_source_surface(cr,frame,0,0);
                cairo_paint(cr);
                cairo_set_source_rgb(cr,0,0,0);
                cairo_surface_flush(surface);
                XFlush(display);
            } else {
                fprintf(stderr,"Orbit Timer could not draw a frame: %s\n",cairo_status_to_string(status));
            }
            redraw = false;
        }
        poll(&display_fd, 1, timer.running ? 40 : alternate.running ? 100 : -1);
    }
    if (frame) cairo_surface_destroy(frame);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}


static int preview(Timer timer, const char *state) {
    const bool wide = !strcmp(state,"wide");
    const int width=wide?1342:WIDTH, height=wide?718:HEIGHT;
    if (!strcmp(state, "running") || wide) {
        timer.running = true;
        timer.left = 186;
    } else if (!strcmp(state, "finished")) {
        timer.done = true;
        timer.left = 0;
    } else if (strcmp(state, "ready") && strcmp(state,"chess")) {
        return 2;
    }
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    if (!strcmp(state,"chess")) {
        Chess g;
        chess_reset(&g);
        chess_draw(cr,&g,WIDTH,HEIGHT,-1);
    } else draw(cr, &timer, width, height, NULL, false);
    cairo_status_t status = cairo_surface_write_to_png(surface, "orbit-preview.png");
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return status != CAIRO_STATUS_SUCCESS;
}
