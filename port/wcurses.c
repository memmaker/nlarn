/* In-memory curses + panels for the NLarn web port (RVIP R-frontend).
 *
 * Windows are cell grids. doupdate() composes stdscr and the visible panels
 * (bottom to top) into one frame, resolves each cell's colour pair and
 * attributes to RGB, and routes the cells that changed to the frontend
 * panes (be.h). Stage 1 routes the whole frame to P_SCREEN; route() is the
 * place where stdscr regions (map, status, messages) and panels (pop-ups)
 * get their own panes later (RVIP W0). */
#include <curses.h>
#include <panel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "be.h"

WINDOW *stdscr = NULL, *curscr = NULL;
int LINES = 25, COLS = 90, ESCDELAY = 0;

static bool ended = false;
static int cursor_vis = 0;
static WINDOW *cursor_win = NULL;      /* the window refreshed last */
static PANEL *bottom = NULL, *top = NULL;

/* composed frame, and what the frontend shows (resolved colours) */
typedef struct { uint32_t ch, fg, bg; int attr; } sent_cell;
static wc_cell *frame = NULL;
static sent_cell *shown = NULL;
static bool full_redraw = true;

/* ------------------------------------------------------------ colours */

static uint32_t palette[256];
static struct { short fg, bg; } pairs[256];

static void palette_init(void)
{
    static const uint32_t base[16] = {
        0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
        0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff,
    };
    static const int lvl[6] = { 0, 95, 135, 175, 215, 255 };
    for (int i = 0; i < 16; i++) palette[i] = base[i];
    for (int i = 0; i < 216; i++)
        palette[16 + i] = (uint32_t)(lvl[i / 36] << 16 | lvl[(i / 6) % 6] << 8 | lvl[i % 6]);
    for (int i = 0; i < 24; i++)
    {
        uint32_t g = (uint32_t)(8 + 10 * i);
        palette[232 + i] = g << 16 | g << 8 | g;
    }
    for (int i = 0; i < 256; i++) { pairs[i].fg = COLOR_WHITE; pairs[i].bg = COLOR_BLACK; }
}

int start_color(void) { return OK; }
bool has_colors(void) { return TRUE; }
bool can_change_color(void) { return TRUE; }

int init_color(short color, short r, short g, short b)
{
    if (color < 0 || color > 255) return ERR;
#define C8(v) ((uint32_t)(((v) < 0 ? 0 : (v) > 1000 ? 1000 : (v)) * 255 / 1000))
    palette[color] = C8(r) << 16 | C8(g) << 8 | C8(b);
#undef C8
    full_redraw = true;
    return OK;
}

int init_pair(short pair, short fg, short bg)
{
    if (pair < 1 || pair > 255) return ERR;
    pairs[pair].fg = fg < 0 ? COLOR_WHITE : fg;
    pairs[pair].bg = bg < 0 ? COLOR_BLACK : bg;
    full_redraw = true;
    return OK;
}

static void resolve(attr_t a, uint32_t *fg, uint32_t *bg, int *battr)
{
    int p = PAIR_NUMBER(a);
    int f = pairs[p].fg, b = pairs[p].bg;
    if ((a & A_BOLD) && f < 8) f += 8;
    if (a & (A_REVERSE | A_STANDOUT)) { int t = f; f = b; b = t; }
    *fg = palette[f & 255];
    *bg = palette[b & 255];
    if (a & A_DIM)
        *fg = (*fg >> 1) & 0x7f7f7f;
    *battr = ((a & A_BOLD) ? BE_BOLD : 0) | ((a & A_UNDERLINE) ? BE_UNDERLINE : 0)
        | ((a & A_BLINK) ? BE_BLINK : 0);
}

/* ------------------------------------------------------------ windows */

static uint32_t acs(uint32_t c)
{
    switch (c)
    {
    case 'l': return 0x250c;
    case 'm': return 0x2514;
    case 'k': return 0x2510;
    case 'j': return 0x2518;
    case 'q': return 0x2500;
    case 'x': return 0x2502;
    case 't': return 0x251c;
    case 'u': return 0x2524;
    case 'v': return 0x2534;
    case 'w': return 0x252c;
    case 'n': return 0x253c;
    case 'a': return 0x2592;
    case '0': return 0x2588;
    case '~': return 0x00b7;
    default: return c;
    }
}

static wc_cell blank(WINDOW *w)
{
    wc_cell c = { ' ', w->bkgd & A_ATTRIBUTES };
    return c;
}

WINDOW *newwin(int rows, int cols, int y, int x)
{
    if (rows <= 0) rows = LINES - y;
    if (cols <= 0) cols = COLS - x;
    if (rows <= 0 || cols <= 0) return NULL;
    WINDOW *w = calloc(1, sizeof *w);
    w->begy = y;
    w->begx = x;
    w->maxy = rows;
    w->maxx = cols;
    w->delay = -1;
    w->c = malloc(sizeof(wc_cell) * (size_t)rows * (size_t)cols);
    werase(w);
    return w;
}

int delwin(WINDOW *w)
{
    if (!w) return ERR;
    if (cursor_win == w) cursor_win = NULL;
    free(w->c);
    free(w);
    full_redraw = true;
    return OK;
}

int mvwin(WINDOW *w, int y, int x)
{
    if (!w) return ERR;
    w->begy = y;
    w->begx = x;
    w->touched = true;
    return OK;
}

int resize_term(int rows, int cols)
{
    (void)rows;
    (void)cols;
    return OK; /* fixed size: the frontend scales */
}

WINDOW *initscr(void)
{
    if (stdscr) return stdscr;
    palette_init();
    stdscr = newwin(LINES, COLS, 0, 0);
    curscr = stdscr;
    frame = calloc((size_t)LINES * (size_t)COLS, sizeof *frame);
    shown = calloc((size_t)LINES * (size_t)COLS, sizeof *shown);
    be_init(P_SCREEN, COLS, LINES);
    return stdscr;
}

int endwin(void)
{
    ended = true;
    return OK;
}

bool isendwin(void) { return ended; }

int wmove(WINDOW *w, int y, int x)
{
    if (!w || y < 0 || x < 0 || y >= w->maxy || x >= w->maxx) return ERR;
    w->cury = y;
    w->curx = x;
    return OK;
}

/* the cell's attributes: explicit colour wins, else the window's, else
 * the background's */
static attr_t merge_attr(WINDOW *w, attr_t a)
{
    a = (a & A_ATTRIBUTES & ~A_ALTCHARSET) | (w->attr & ~A_COLOR);
    if (!(a & A_COLOR)) a |= w->attr & A_COLOR;
    if (!(a & A_COLOR)) a |= w->bkgd & A_COLOR;
    return a;
}

static int put(WINDOW *w, uint32_t ch, attr_t a)
{
    if (w->cury >= w->maxy) return ERR;
    if (ch == '\n')
    {
        wclrtoeol(w);
        w->curx = 0;
        if (w->cury + 1 >= w->maxy) return ERR;
        w->cury++;
        return OK;
    }
    if (ch == '\t')
    {
        int n = 8 - (w->curx % 8);
        while (n-- > 0)
            if (put(w, ' ', a) == ERR) return ERR;
        return OK;
    }
    if (ch == '\b')
    {
        if (w->curx > 0) w->curx--;
        return OK;
    }
    if (ch < 32 || ch == 127) ch = ' ';
    wc_cell *c = &w->c[w->cury * w->maxx + w->curx];
    c->ch = ch;
    c->attr = merge_attr(w, a);
    w->touched = true;
    if (++w->curx >= w->maxx)
    {
        if (w->cury + 1 >= w->maxy)
        {
            w->curx = w->maxx - 1;
            return ERR;
        }
        w->curx = 0;
        w->cury++;
    }
    return OK;
}

int waddch(WINDOW *w, chtype ch)
{
    if (!w) return ERR;
    uint32_t c = ch & A_CHARTEXT;
    if (ch & A_ALTCHARSET) c = acs(c);
    return put(w, c, ch & A_ATTRIBUTES);
}

int wadd_wch(WINDOW *w, const cchar_t *wch)
{
    if (!w || !wch) return ERR;
    return put(w, (uint32_t)wch->chars[0], wch->attr);
}

int setcchar(cchar_t *wcval, const wchar_t *wch, attr_t attrs, short pair, const void *opts)
{
    (void)opts;
    memset(wcval, 0, sizeof *wcval);
    wcval->chars[0] = wch ? wch[0] : L' ';
    wcval->attr = (attrs & ~A_COLOR) | COLOR_PAIR(pair);
    wcval->ext_color = pair;
    return OK;
}

/* UTF-8 decoder (NLarn's text is UTF-8 whatever the libc locale is) */
static const char *utf8(const char *s, const char *end, uint32_t *out)
{
    unsigned char c = (unsigned char)*s++;
    int n = c < 0x80 ? 0 : c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    uint32_t r = n == 0 ? c : n == 1 ? (c & 0x1fu) : n == 2 ? (c & 0x0fu) : (c & 0x07u);
    while (n-- > 0 && s < end && ((unsigned char)*s & 0xc0) == 0x80)
        r = (r << 6) | ((unsigned char)*s++ & 0x3fu);
    *out = r;
    return s;
}

int waddnstr(WINDOW *w, const char *s, int n)
{
    if (!w || !s) return ERR;
    const char *end = s + (n < 0 ? (int)strlen(s) : (int)strnlen(s, (size_t)n));
    while (s < end)
    {
        uint32_t ch;
        s = utf8(s, end, &ch);
        if (put(w, ch, 0) == ERR) return ERR;
    }
    return OK;
}

int waddstr(WINDOW *w, const char *s) { return waddnstr(w, s, -1); }

int vw_printw(WINDOW *w, const char *fmt, va_list ap)
{
    char buf[4096];
    vsnprintf(buf, sizeof buf, fmt, ap);
    return waddstr(w, buf);
}

int wprintw(WINDOW *w, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vw_printw(w, fmt, ap);
    va_end(ap);
    return r;
}

int printw(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vw_printw(stdscr, fmt, ap);
    va_end(ap);
    return r;
}

int mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...)
{
    if (wmove(w, y, x) == ERR) return ERR;
    va_list ap;
    va_start(ap, fmt);
    int r = vw_printw(w, fmt, ap);
    va_end(ap);
    return r;
}

int mvprintw(int y, int x, const char *fmt, ...)
{
    if (wmove(stdscr, y, x) == ERR) return ERR;
    va_list ap;
    va_start(ap, fmt);
    int r = vw_printw(stdscr, fmt, ap);
    va_end(ap);
    return r;
}

int wchgat(WINDOW *w, int n, attr_t attr, short pair, const void *opts)
{
    (void)opts;
    if (!w) return ERR;
    wc_cell *row = &w->c[w->cury * w->maxx];
    for (int x = w->curx; x < w->maxx && (n < 0 || x < w->curx + n); x++)
        row[x].attr = (attr & ~A_COLOR) | COLOR_PAIR(pair);
    w->touched = true;
    return OK;
}

int mvwchgat(WINDOW *w, int y, int x, int n, attr_t attr, short pair, const void *opts)
{
    if (wmove(w, y, x) == ERR) return ERR;
    return wchgat(w, n, attr, pair, opts);
}

int wclrtoeol(WINDOW *w)
{
    if (!w) return ERR;
    for (int x = w->curx; x < w->maxx; x++) w->c[w->cury * w->maxx + x] = blank(w);
    w->touched = true;
    return OK;
}

int wclrtobot(WINDOW *w)
{
    if (!w) return ERR;
    wclrtoeol(w);
    for (int i = (w->cury + 1) * w->maxx; i < w->maxy * w->maxx; i++) w->c[i] = blank(w);
    return OK;
}

int werase(WINDOW *w)
{
    if (!w) return ERR;
    for (int i = 0; i < w->maxy * w->maxx; i++) w->c[i] = blank(w);
    w->cury = w->curx = 0;
    w->touched = true;
    return OK;
}

int wclear(WINDOW *w)
{
    werase(w);
    full_redraw = true;
    return OK;
}

int wbkgd(WINDOW *w, chtype ch)
{
    if (!w) return ERR;
    attr_t old = w->bkgd & A_COLOR, now = ch & A_COLOR;
    for (int i = 0; i < w->maxy * w->maxx; i++)
        if ((w->c[i].attr & A_COLOR) == old || !(w->c[i].attr & A_COLOR))
            w->c[i].attr = (w->c[i].attr & ~A_COLOR) | now;
    w->bkgd = ch & A_ATTRIBUTES;
    w->touched = true;
    return OK;
}

int wattron(WINDOW *w, int a)
{
    if (!w) return ERR;
    if (a & A_COLOR) w->attr &= ~A_COLOR;
    w->attr |= (attr_t)a;
    return OK;
}

int wattroff(WINDOW *w, int a)
{
    if (!w) return ERR;
    if (a & A_COLOR) w->attr &= ~A_COLOR;
    w->attr &= ~((attr_t)a & ~A_COLOR);
    return OK;
}

int wattrset(WINDOW *w, int a)
{
    if (!w) return ERR;
    w->attr = (attr_t)a;
    return OK;
}

static void cellset(WINDOW *w, int y, int x, chtype ch)
{
    if (y < 0 || x < 0 || y >= w->maxy || x >= w->maxx) return;
    uint32_t c = ch & A_CHARTEXT;
    if (ch & A_ALTCHARSET) c = acs(c);
    w->c[y * w->maxx + x].ch = c;
    w->c[y * w->maxx + x].attr = merge_attr(w, ch & A_ATTRIBUTES);
}

int wborder(WINDOW *w, chtype ls, chtype rs, chtype ts, chtype bs,
            chtype tl, chtype tr, chtype bl, chtype br)
{
    if (!w) return ERR;
    if (!ls) ls = ACS_VLINE;
    if (!rs) rs = ACS_VLINE;
    if (!ts) ts = ACS_HLINE;
    if (!bs) bs = ACS_HLINE;
    if (!tl) tl = ACS_ULCORNER;
    if (!tr) tr = ACS_URCORNER;
    if (!bl) bl = ACS_LLCORNER;
    if (!br) br = ACS_LRCORNER;
    int X = w->maxx - 1, Y = w->maxy - 1;
    for (int x = 1; x < X; x++) { cellset(w, 0, x, ts); cellset(w, Y, x, bs); }
    for (int y = 1; y < Y; y++) { cellset(w, y, 0, ls); cellset(w, y, X, rs); }
    cellset(w, 0, 0, tl);
    cellset(w, 0, X, tr);
    cellset(w, Y, 0, bl);
    cellset(w, Y, X, br);
    w->touched = true;
    return OK;
}

int box(WINDOW *w, chtype v, chtype h) { return wborder(w, v, v, h, h, 0, 0, 0, 0); }

int whline(WINDOW *w, chtype ch, int n)
{
    if (!w) return ERR;
    if (!(ch & A_CHARTEXT)) ch |= ACS_HLINE;
    for (int i = 0; i < n; i++) cellset(w, w->cury, w->curx + i, ch);
    w->touched = true;
    return OK;
}

int wvline(WINDOW *w, chtype ch, int n)
{
    if (!w) return ERR;
    if (!(ch & A_CHARTEXT)) ch |= ACS_VLINE;
    for (int i = 0; i < n; i++) cellset(w, w->cury + i, w->curx, ch);
    w->touched = true;
    return OK;
}

int mvwhline(WINDOW *w, int y, int x, chtype ch, int n)
{
    if (wmove(w, y, x) == ERR) return ERR;
    return whline(w, ch, n);
}

int mvwvline(WINDOW *w, int y, int x, chtype ch, int n)
{
    if (wmove(w, y, x) == ERR) return ERR;
    return wvline(w, ch, n);
}

int touchwin(WINDOW *w)
{
    if (!w) return ERR;
    w->touched = true;
    return OK;
}

/* ------------------------------------------------------------- panels */

static void unlink_panel(PANEL *p)
{
    if (p->below) p->below->above = p->above; else if (bottom == p) bottom = p->above;
    if (p->above) p->above->below = p->below; else if (top == p) top = p->below;
    p->above = p->below = NULL;
}

static void link_top(PANEL *p)
{
    p->below = top;
    p->above = NULL;
    if (top) top->above = p;
    top = p;
    if (!bottom) bottom = p;
}

PANEL *new_panel(WINDOW *w)
{
    PANEL *p = calloc(1, sizeof *p);
    p->win = w;
    link_top(p);
    return p;
}

int del_panel(PANEL *p)
{
    if (!p) return ERR;
    unlink_panel(p);
    free(p);
    full_redraw = true;
    return OK;
}

int hide_panel(PANEL *p)
{
    if (!p) return ERR;
    p->hidden = true;
    return OK;
}

int show_panel(PANEL *p)
{
    if (!p) return ERR;
    p->hidden = false;
    unlink_panel(p);
    link_top(p);
    return OK;
}

int top_panel(PANEL *p) { return show_panel(p); }

int bottom_panel(PANEL *p)
{
    if (!p) return ERR;
    unlink_panel(p);
    p->above = bottom;
    if (bottom) bottom->below = p;
    bottom = p;
    if (!top) top = p;
    return OK;
}

int move_panel(PANEL *p, int y, int x) { return p ? mvwin(p->win, y, x) : ERR; }

int replace_panel(PANEL *p, WINDOW *w)
{
    if (!p) return ERR;
    p->win = w;
    return OK;
}

bool panel_hidden(const PANEL *p) { return p ? p->hidden : TRUE; }
WINDOW *panel_window(const PANEL *p) { return p ? p->win : NULL; }
void update_panels(void) { }

/* ------------------------------------------------------ compose + route */

static void blit(WINDOW *w)
{
    for (int y = 0; y < w->maxy; y++)
    {
        int sy = w->begy + y;
        if (sy < 0 || sy >= LINES) continue;
        for (int x = 0; x < w->maxx; x++)
        {
            int sx = w->begx + x;
            if (sx < 0 || sx >= COLS) continue;
            frame[sy * COLS + sx] = w->c[y * w->maxx + x];
        }
    }
    w->touched = false;
}

/* Stage 1: the whole screen is one pane. */
static void route(int y, int x, const sent_cell *c)
{
    be_put(P_SCREEN, y, x, c->ch, c->fg, c->bg, c->attr);
}

int wnoutrefresh(WINDOW *w)
{
    if (w) cursor_win = w;
    return OK;
}

int doupdate(void)
{
    if (!stdscr) return ERR;
    blit(stdscr);
    for (PANEL *p = bottom; p; p = p->above)
        if (!p->hidden) blit(p->win);
    for (int y = 0; y < LINES; y++)
        for (int x = 0; x < COLS; x++)
        {
            const wc_cell *c = &frame[y * COLS + x];
            sent_cell s;
            s.ch = c->ch;
            resolve(c->attr, &s.fg, &s.bg, &s.attr);
            sent_cell *o = &shown[y * COLS + x];
            if (full_redraw || memcmp(o, &s, sizeof s))
            {
                *o = s;
                route(y, x, &s);
            }
        }
    full_redraw = false;
    WINDOW *cw = cursor_win ? cursor_win : stdscr;
    if (cursor_vis)
        be_cursor(P_SCREEN, cw->begy + cw->cury, cw->begx + cw->curx);
    else
        be_cursor(P_SCREEN, -1, -1);
    be_flush();
    return OK;
}

int wrefresh(WINDOW *w)
{
    wnoutrefresh(w);
    return doupdate();
}

int curs_set(int visibility)
{
    int old = cursor_vis;
    cursor_vis = visibility;
    return old;
}

/* -------------------------------------------------------------- input */

#define QMAX 64
static int queue[QMAX];
static int qlen = 0;
static MEVENT mouse_cur;
static bool mouse_have = false;
static mmask_t mouse_mask = 0;

int ungetch(int ch)
{
    if (qlen >= QMAX) return ERR;
    memmove(queue + 1, queue, sizeof(int) * (size_t)qlen);
    queue[0] = ch;
    qlen++;
    return OK;
}

void wtimeout(WINDOW *w, int delay)
{
    if (w) w->delay = delay;
}

int nodelay(WINDOW *w, bool bf)
{
    if (!w) return ERR;
    w->delay = bf ? 0 : -1;
    return OK;
}

static mmask_t mouse_bits(int button)
{
    switch (button)
    {
    case BE_MOUSE_B1_DOWN: return BUTTON1_PRESSED;
    case BE_MOUSE_B1_UP: return BUTTON1_RELEASED;
    case BE_MOUSE_B3_DOWN: return BUTTON3_PRESSED;
    case BE_MOUSE_WHEEL_UP: return BUTTON4_PRESSED;
    case BE_MOUSE_WHEEL_DOWN: return BUTTON5_PRESSED;
    case BE_MOUSE_MOVE: return REPORT_MOUSE_POSITION;
    default: return 0;
    }
}

int wgetch(WINDOW *w)
{
    if (!w) w = stdscr;
    if (qlen > 0)
    {
        int ch = queue[0];
        memmove(queue, queue + 1, sizeof(int) * (size_t)--qlen);
        return ch;
    }
    /* like ncurses: a changed window is refreshed before reading */
    wrefresh(w);
    for (;;)
    {
        int k = be_getkey(w->delay);
        if (k < 0) return ERR;
        if (k & BE_MOUSE)
        {
            mmask_t b = mouse_bits((k >> 16) & 0xff);
            if (!(b & mouse_mask)) continue;
            memset(&mouse_cur, 0, sizeof mouse_cur);
            mouse_cur.x = k & 0xff;
            mouse_cur.y = (k >> 8) & 0xff;
            mouse_cur.bstate = b;
            mouse_have = true;
            return KEY_MOUSE;
        }
        return k;
    }
}

int getmouse(MEVENT *ev)
{
    if (!mouse_have || !ev) return ERR;
    *ev = mouse_cur;
    mouse_have = false;
    return OK;
}

mmask_t mousemask(mmask_t newmask, mmask_t *oldmask)
{
    if (oldmask) *oldmask = mouse_mask;
    mouse_mask = newmask;
    return newmask;
}

int mouseinterval(int ms)
{
    (void)ms;
    return 0;
}

int napms(int ms)
{
    be_sleep(ms);
    return OK;
}

int beep(void)
{
    be_bell();
    return OK;
}

int flash(void) { return OK; }

int set_escdelay(int ms)
{
    ESCDELAY = ms;
    return OK;
}
