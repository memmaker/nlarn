/* In-memory curses + panels for the NLarn web port (RVIP R-frontend).
 *
 * Windows are cell grids. doupdate() resolves each cell's colour pair and
 * attributes to RGB and sends the cells that changed to the frontend panes
 * (be.h): stdscr regions to the map and status panes (route()), windows
 * registered with wc_pane() to their own panes (messages, inventory), and
 * the visible panels, composed bottom to top into their bounding box, to
 * the pop-up pane (RVIP W0). */
#include <curses.h>
#include <panel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "be.h"

/* screen regions of src/display.c (see route()) */
#define MAP_W 67              /* MAP_MAX_X */
#define MAP_H 17              /* MAP_MAX_Y */
#define SEG2 (MAP_W - 21)     /* the HP/MP, XP segment of the lines under the map */
#define ST_W 46               /* Status pane: the widest segment */
#define ST_H (6 + MAP_H)
void wc_unpane(WINDOW *w);
static void text_reset(int p, int rows);

WINDOW *stdscr = NULL, *curscr = NULL;
int LINES = 25, COLS = 90, ESCDELAY = 0;

static bool ended = false;
static int cursor_vis = 0;
static WINDOW *cursor_win = NULL;      /* the window refreshed last */
static PANEL *bottom = NULL, *top = NULL;

/* what the frontend shows of stdscr (resolved colours) */
typedef struct { uint32_t ch, fg, bg; int attr, tile; } sent_cell;
/* tiles of stdscr cells, each with the cell it was set for (port/tiles.c) */
typedef struct { wc_cell c; int tile; } tile_cell;
static tile_cell *tiles = NULL;
static sent_cell *shown = NULL;
static sent_cell st_cell[ST_H * ST_W];   /* the Status pane (stacked segments) */
/* pop-up pane: the visible panels composed into their bounding box */
static wc_cell *popc = NULL;
static bool *popcov = NULL;
static sent_cell *pop_cells = NULL;
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
        | ((a & A_BLINK) ? BE_BLINK : 0) | ((a & (A_REVERSE | A_STANDOUT)) ? BE_REVERSE : 0);
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
    wc_unpane(w);
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
    shown = calloc((size_t)LINES * (size_t)COLS, sizeof *shown);
    popc = calloc((size_t)LINES * (size_t)COLS, sizeof *popc);
    popcov = calloc((size_t)LINES * (size_t)COLS, sizeof *popcov);
    pop_cells = calloc((size_t)LINES * (size_t)COLS, sizeof *pop_cells);
    tiles = calloc((size_t)LINES * (size_t)COLS, sizeof *tiles);
    for (int i = 0; i < LINES * COLS; i++) tiles[i].tile = -1;
    be_init(P_MAP, MAP_W, MAP_H);
    be_init(P_STATUS, ST_W, ST_H);
    text_reset(P_STATUS, ST_H);
    for (int i = 0; i < ST_H * ST_W; i++) st_cell[i] = (sent_cell){ ' ', 0, 0, 0, -1 };
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

/* stdscr regions (src/display.c): the map is rows 0..16 x cols 0..66
 * (MAP_MAX_Y x MAP_MAX_X), its border row 17 / col 67; the right status
 * column starts at col 68; two status lines at rows 18-19 in three
 * segments (name / level at col 0, HP MP / XP at col 46, turn / dungeon
 * level at col 68); messages from row 20 (not routed: P_MSG comes from
 * the game's history window). The Status pane stacks them: rows 0-1 the
 * first segment, 2-3 the second, 4-5 the third, 6.. the right column. */

static bool region(int y, int x, int *p, int *py, int *px)
{
    if (y < MAP_H && x < MAP_W) { *p = P_MAP; *py = y; *px = x; return true; }
    if (y < MAP_H && x > MAP_W) { *p = P_STATUS; *py = 6 + y; *px = x - MAP_W - 1; return true; }
    if (y == MAP_H + 1 || y == MAP_H + 2)
    {
        int r = y - MAP_H - 1;
        *p = P_STATUS;
        if (x < SEG2) { *py = r; *px = x; }
        else if (x <= MAP_W) { *py = 2 + r; *px = x - SEG2; }
        else { *py = 4 + r; *px = x - MAP_W - 1; }
        return *px < ST_W;
    }
    return false;
}

/* Text panes go out as whole lines (RVIP W0 rules 5, 6): each changed row
 * once, trimmed (no trailing blanks), with its colour and icon tile
 * (wc_rowattr), and the rows in use (be_rows: to the last non-blank row or
 * the cursor). Inside a row: reverse-video cells (highlights) between \x01 and \x02, each cell's colour as a run
 * "\x05#rrggbb" (bold: "\x05*#rrggbb"; a highlight's background other than
 * the pane's: "\x05#rrggbb/#rrggbb") up to "\x06"; a blank doesn't break a run. Colours are the game's, resolved here (resolve()). */
typedef struct { char **line; const char **css; int *tile; int n, rows, cy, cx; } text_pane;
static text_pane tp[NPANES];

static void text_reset(int p, int rows)
{
    text_pane *t = &tp[p];
    for (int y = 0; y < t->n; y++) free(t->line[y]);
    free(t->line); free(t->css); free(t->tile);
    t->n = rows;
    t->line = calloc((size_t)rows, sizeof *t->line);
    t->css = calloc((size_t)rows, sizeof *t->css);
    t->tile = malloc(sizeof(int) * (size_t)rows);
    for (int y = 0; y < rows; y++) t->tile[y] = -1;
    t->rows = -1;
    t->cy = -1;
}

void wc_rowattr(int p, int y, const char *css, int tile)
{
    text_pane *t = &tp[p];
    if (p < 0 || p >= NPANES || y < 0 || y >= t->n) return;
    t->css[y] = css && *css ? css : NULL;
    t->tile[y] = tile;
}

static bool cell_blank(const sent_cell *c) { return c->ch == ' ' && !(c->attr & BE_REVERSE); }

static int put_utf8(uint32_t c, char *o)
{
    if (c < 32) c = ' ';
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xc0 | c >> 6); o[1] = (char)(0x80 | (c & 63)); return 2; }
    if (c < 0x10000) { o[0] = (char)(0xe0 | c >> 12); o[1] = (char)(0x80 | ((c >> 6) & 63)); o[2] = (char)(0x80 | (c & 63)); return 3; }
    o[0] = (char)(0xf0 | c >> 18); o[1] = (char)(0x80 | ((c >> 12) & 63));
    o[2] = (char)(0x80 | ((c >> 6) & 63)); o[3] = (char)(0x80 | (c & 63)); return 4;
}

/* one pane row as a line (buf: 20 bytes per cell + 4) */
static void row_text(const sent_cell *r, int w, uint32_t base_bg, char *buf)
{
    int end = w, n = 0, so = 0;
    char cr[24] = "", run[24];
    while (end > 0 && cell_blank(&r[end - 1])) end--;
    for (int x = 0; x < end; x++)
    {
        const sent_cell *c = &r[x];
        int s = (c->attr & BE_REVERSE) != 0;
        if (c->ch == ' ' && !s) strcpy(run, cr);       /* a blank doesn't break a run */
        else if ((c->bg & 0xffffff) == base_bg || s)
            snprintf(run, sizeof run, "\x05%s#%06x", (c->attr & BE_BOLD) ? "*" : "", (unsigned)(c->fg & 0xffffff));
        else   /* a highlight: its own background */
            snprintf(run, sizeof run, "\x05%s#%06x/#%06x", (c->attr & BE_BOLD) ? "*" : "",
                     (unsigned)(c->fg & 0xffffff), (unsigned)(c->bg & 0xffffff));
        if (s != so || strcmp(run, cr))
        {
            if (*cr) buf[n++] = 6;
            if (s != so) buf[n++] = (so = s) ? 1 : 2;
            n += sprintf(buf + n, "%s", run);
            strcpy(cr, run);
        }
        n += put_utf8(c->ch, buf + n);
    }
    if (*cr) buf[n++] = 6;
    if (so) buf[n++] = 2;
    buf[n] = 0;
}

/* send a pane's changed rows and its rows in use */
static void send_text(int p, const sent_cell *cells, int w, int h)
{
    text_pane *t = &tp[p];
    char *buf = malloc((size_t)w * 28 + 4);
    int used = 0;
    /* the pane's own background (a pop-up's box colour) is the page's; other
       backgrounds go into the runs */
    uint32_t base_bg = p == P_POP && w * h > 0 ? cells[0].bg & 0xffffff : 0;
    if (h > t->n) h = t->n;
    for (int y = 0; y < h; y++)
    {
        const sent_cell *r = &cells[y * w];
        row_text(r, w, base_bg, buf);
        if (*buf || t->tile[y] >= 0) used = y + 1;
        const char *css = t->css[y] ? t->css[y] : "";
        /* the stored line carries the colour and tile too, so any change resends */
        size_t ln = strlen(buf);
        char *key = malloc(ln + strlen(css) + 16);
        sprintf(key, "%s\x1f%s\x1f%d", buf, css, t->tile[y]);
        if (!t->line[y] || strcmp(t->line[y], key))
        {
            free(t->line[y]);
            t->line[y] = key;
            be_line(p, y, buf, css, t->tile[y]);
        }
        else free(key);
    }
    if (t->cy >= used) used = t->cy + 1;
    if (used != t->rows) be_rows(p, t->rows = used);
    free(buf);
}

/* the cursor (one place): on the map (targeting) or in a text pane (text entry) */
static void cursor(int p, int y, int x)
{
    for (int q = 0; q < NPANES; q++)
    {
        int cy = q == p ? y : -1, cx = q == p ? x : -1;
        if (q != P_MAP) { if (tp[q].cy == cy && tp[q].cx == cx) continue; tp[q].cy = cy; tp[q].cx = cx; }
        be_cursor(q, cy, cx);
    }
}

static void route(int y, int x, const sent_cell *c)
{
    int p, py, px;
    if (!region(y, x, &p, &py, &px)) return;
    if (p == P_MAP) be_put(p, py, px, c->ch, c->fg, c->bg, c->attr, c->tile);
    else st_cell[py * ST_W + px] = *c;
}

void wc_settile(int y, int x, int tile)
{
    if (!stdscr || !tiles || y < 0 || x < 0 || y >= stdscr->maxy || x >= stdscr->maxx
            || y >= LINES || x >= COLS) return;
    tile_cell *t = &tiles[y * COLS + x];
    t->c = stdscr->c[y * stdscr->maxx + x];
    t->tile = tile;
}

static int tile_at(int y, int x)
{
    const tile_cell *t = &tiles[y * COLS + x];
    const wc_cell *c = &stdscr->c[y * stdscr->maxx + x];
    if (t->tile < 0) return -1;
    return t->c.ch == c->ch && t->c.attr == c->attr ? t->tile : -1;
}

uint32_t wc_rgb(int colour) { return palette[colour & 255]; }

/* windows that are panes of their own (P_MSG, P_INV) */
typedef struct { WINDOW *w; sent_cell *cells; bool full; } pane_win;
static pane_win pwins[NPANES];

void wc_pane(WINDOW *w, int pane)
{
    pane_win *q = &pwins[pane];
    q->w = w;
    free(q->cells);
    q->cells = calloc((size_t)w->maxy * (size_t)w->maxx, sizeof *q->cells);
    q->full = true;
    w->touched = true;
    be_init(pane, w->maxx, w->maxy);
    text_reset(pane, w->maxy);
}

static void pane_flush(int p)
{
    pane_win *q = &pwins[p];
    WINDOW *w = q->w;
    if (!w || (!w->touched && !q->full && !full_redraw)) return;
    for (int i = 0; i < w->maxy * w->maxx; i++)
    {
        sent_cell *s = &q->cells[i];
        s->ch = w->c[i].ch;
        resolve(w->c[i].attr, &s->fg, &s->bg, &s->attr);
        s->tile = -1;
    }
    send_text(p, q->cells, w->maxx, w->maxy);
    q->full = false;
    w->touched = false;
}

void wc_unpane(WINDOW *w)
{
    for (int p = 0; p < NPANES; p++)
        if (pwins[p].w == w) pwins[p].w = NULL;
}

/* pop-ups: the visible panels, composed into their bounding box */
static int pop_y0, pop_x0, pop_h, pop_w;

static void pop_compose(void)
{
    int y0 = LINES, x0 = COLS, y1 = 0, x1 = 0;
    for (PANEL *p = bottom; p; p = p->above)
    {
        if (p->hidden) continue;
        WINDOW *w = p->win;
        if (w->begy < y0) y0 = w->begy;
        if (w->begx < x0) x0 = w->begx;
        if (w->begy + w->maxy > y1) y1 = w->begy + w->maxy;
        if (w->begx + w->maxx > x1) x1 = w->begx + w->maxx;
    }
    if (y0 < 0) y0 = 0;
    if (x0 < 0) x0 = 0;
    if (y1 > LINES) y1 = LINES;
    if (x1 > COLS) x1 = COLS;
    int h = y1 > y0 ? y1 - y0 : 0, w = x1 > x0 ? x1 - x0 : 0;
    if (h != pop_h || w != pop_w || y0 != pop_y0 || x0 != pop_x0)
    {
        pop_h = h; pop_w = w; pop_y0 = y0; pop_x0 = x0;
        be_popup(h, w, y0, x0);
        if (h && w) text_reset(P_POP, h);
    }
    if (!h || !w) return;
    memset(popcov, 0, sizeof(bool) * (size_t)LINES * (size_t)COLS);
    for (PANEL *p = bottom; p; p = p->above)
    {
        if (p->hidden) continue;
        WINDOW *win = p->win;
        for (int y = 0; y < win->maxy; y++)
            for (int x = 0; x < win->maxx; x++)
            {
                int sy = win->begy + y - y0, sx = win->begx + x - x0;
                if (sy < 0 || sx < 0 || sy >= h || sx >= w) continue;
                popc[sy * w + sx] = win->c[y * win->maxx + x];
                popcov[sy * w + sx] = true;
            }
        win->touched = false;
    }
    for (int i = 0; i < h * w; i++)
    {
        sent_cell s = { ' ', 0, 0, 0, -1 };
        if (popcov[i])
        {
            s.ch = popc[i].ch;
            resolve(popc[i].attr, &s.fg, &s.bg, &s.attr);
        }
        pop_cells[i] = s;
    }
}

static bool panel_shown(WINDOW *w)
{
    for (PANEL *p = bottom; p; p = p->above)
        if (p->win == w && !p->hidden) return true;
    return false;
}

int wnoutrefresh(WINDOW *w)
{
    if (w) cursor_win = w;
    return OK;
}

int doupdate(void)
{
    if (!stdscr) return ERR;
    /* stdscr regions: map and status */
    for (int y = 0; y < LINES && y < stdscr->maxy; y++)
        for (int x = 0; x < COLS && x < stdscr->maxx; x++)
        {
            const wc_cell *c = &stdscr->c[y * stdscr->maxx + x];
            sent_cell s;
            s.ch = c->ch;
            resolve(c->attr, &s.fg, &s.bg, &s.attr);
            s.tile = tile_at(y, x);
            sent_cell *o = &shown[y * COLS + x];
            if (full_redraw || memcmp(o, &s, sizeof s))
            {
                *o = s;
                route(y, x, &s);
            }
        }
    stdscr->touched = false;
    pop_compose();

    /* the cursor: in a pop-up, or on the map (targeting) */
    int cp = -1, cy = -1, cx = -1;
    WINDOW *cw = cursor_win ? cursor_win : stdscr;
    if (cursor_vis)
    {
        int sy = cw->begy + cw->cury, sx = cw->begx + cw->curx;
        if (cw != stdscr && panel_shown(cw) && pop_h)
        {
            cp = P_POP; cy = sy - pop_y0; cx = sx - pop_x0;
        }
        else if (cw == stdscr && sy < MAP_H && sx < MAP_W)
        {
            cp = P_MAP; cy = sy; cx = sx;
        }
    }
    cursor(cp, cy, cx);

    send_text(P_STATUS, st_cell, ST_W, ST_H);
    for (int p = 0; p < NPANES; p++) pane_flush(p);
    if (pop_h && pop_w) send_text(P_POP, pop_cells, pop_w, pop_h);
    full_redraw = false;
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
