/* In-memory curses (+ panels) for the NLarn web port (RVIP R-frontend).
 * Found before the system curses via -Iport; only what NLarn uses, wide
 * character API included. wcurses.c composes stdscr and the panel stack
 * into one frame on doupdate() and routes it to the frontend panes
 * (port/be.h). NLarn's strings are UTF-8, cells hold Unicode code points. */
#ifndef NLARN_WCURSES_H
#define NLARN_WCURSES_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#define NCURSES_VERSION "wcurses"   /* NLarn takes the ncurses code paths */

typedef uint32_t chtype;
typedef uint32_t attr_t;
typedef uint32_t mmask_t;

#define ERR (-1)
#define OK 0
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* chtype: character in bits 0-7 (ACS index with A_ALTCHARSET),
 * colour pair in bits 8-15, attributes above */
#define A_CHARTEXT 0x000000ffu
#define A_COLOR 0x0000ff00u
#define A_NORMAL 0u
#define A_STANDOUT 0x00010000u
#define A_UNDERLINE 0x00020000u
#define A_REVERSE 0x00040000u
#define A_BLINK 0x00080000u
#define A_DIM 0x00100000u
#define A_BOLD 0x00200000u
#define A_ALTCHARSET 0x00400000u
#define A_ITALIC 0x00800000u
#define A_ATTRIBUTES 0xffffff00u
#define WA_NORMAL A_NORMAL
#define WA_STANDOUT A_STANDOUT
#define WA_UNDERLINE A_UNDERLINE
#define WA_REVERSE A_REVERSE
#define WA_BLINK A_BLINK
#define WA_DIM A_DIM
#define WA_BOLD A_BOLD
#define COLOR_PAIR(n) ((attr_t)(((n) & 0xff) << 8))
#define PAIR_NUMBER(a) ((int)(((a) & A_COLOR) >> 8))

#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7
#define COLORS 256
#define COLOR_PAIRS 256

/* line drawing: indexes into wcurses.c's Unicode table */
#define ACS_ULCORNER (A_ALTCHARSET | 'l')
#define ACS_LLCORNER (A_ALTCHARSET | 'm')
#define ACS_URCORNER (A_ALTCHARSET | 'k')
#define ACS_LRCORNER (A_ALTCHARSET | 'j')
#define ACS_HLINE (A_ALTCHARSET | 'q')
#define ACS_VLINE (A_ALTCHARSET | 'x')
#define ACS_LTEE (A_ALTCHARSET | 't')
#define ACS_RTEE (A_ALTCHARSET | 'u')
#define ACS_BTEE (A_ALTCHARSET | 'v')
#define ACS_TTEE (A_ALTCHARSET | 'w')
#define ACS_PLUS (A_ALTCHARSET | 'n')
#define ACS_CKBOARD (A_ALTCHARSET | 'a')
#define ACS_BLOCK (A_ALTCHARSET | '0')
#define ACS_BULLET (A_ALTCHARSET | '~')

/* keys (ncurses values) */
#define KEY_CODE_YES 0400
#define KEY_BREAK 0401
#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_LEFT 0404
#define KEY_RIGHT 0405
#define KEY_HOME 0406
#define KEY_BACKSPACE 0407
#define KEY_F0 0410
#define KEY_F(n) (KEY_F0 + (n))
#define KEY_DC 0512
#define KEY_IC 0513
#define KEY_NPAGE 0522
#define KEY_PPAGE 0523
#define KEY_ENTER 0527
#define KEY_A1 0534
#define KEY_A3 0535
#define KEY_B2 0536
#define KEY_C1 0537
#define KEY_C3 0540
#define KEY_BTAB 0541
#define KEY_END 0550
#define KEY_MOUSE 0631
#define KEY_RESIZE 0632
/* ncurses has no codes for these keypad keys; NLarn only compares them */
#define KEY_A2 0700
#define KEY_B1 0701
#define KEY_B3 0702
#define KEY_C2 0703

/* mouse */
#define BUTTON1_RELEASED 0x001u
#define BUTTON1_PRESSED 0x002u
#define BUTTON1_CLICKED 0x004u
#define BUTTON3_RELEASED 0x040u
#define BUTTON3_PRESSED 0x080u
#define BUTTON3_CLICKED 0x100u
#define BUTTON4_PRESSED 0x200u
#define BUTTON5_PRESSED 0x400u
#define REPORT_MOUSE_POSITION 0x800u
#define BUTTON1_MOVED REPORT_MOUSE_POSITION
#define ALL_MOUSE_EVENTS 0x7ffu
typedef struct {
    short id;
    int x, y, z;
    mmask_t bstate;
} MEVENT;

typedef struct {
    attr_t attr;
    wchar_t chars[5];
    int ext_color;
} cchar_t;

typedef struct wc_cell {
    uint32_t ch;   /* Unicode code point */
    attr_t attr;   /* attributes + colour pair */
} wc_cell;

typedef struct _win_st {
    int begy, begx, maxy, maxx, cury, curx;
    attr_t attr, bkgd;
    int delay;          /* wtimeout: -1 blocks */
    bool touched;       /* changed since the last compose */
    wc_cell *c;
} WINDOW;

extern WINDOW *stdscr, *curscr;
extern int LINES, COLS, ESCDELAY;

WINDOW *initscr(void);
int endwin(void);
bool isendwin(void);
WINDOW *newwin(int rows, int cols, int y, int x);
int delwin(WINDOW *w);
int mvwin(WINDOW *w, int y, int x);
int resize_term(int rows, int cols);

int wmove(WINDOW *w, int y, int x);
int waddch(WINDOW *w, chtype ch);
int wadd_wch(WINDOW *w, const cchar_t *wch);
int waddnstr(WINDOW *w, const char *s, int n);
int waddstr(WINDOW *w, const char *s);
int wprintw(WINDOW *w, const char *fmt, ...);
int vw_printw(WINDOW *w, const char *fmt, va_list ap);
int mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...);
int mvprintw(int y, int x, const char *fmt, ...);
int printw(const char *fmt, ...);
int wchgat(WINDOW *w, int n, attr_t attr, short pair, const void *opts);
int mvwchgat(WINDOW *w, int y, int x, int n, attr_t attr, short pair, const void *opts);
int wclrtoeol(WINDOW *w);
int wclrtobot(WINDOW *w);
int werase(WINDOW *w);
int wclear(WINDOW *w);
int wbkgd(WINDOW *w, chtype ch);
int wborder(WINDOW *w, chtype ls, chtype rs, chtype ts, chtype bs,
            chtype tl, chtype tr, chtype bl, chtype br);
int box(WINDOW *w, chtype v, chtype h);
int whline(WINDOW *w, chtype ch, int n);
int wvline(WINDOW *w, chtype ch, int n);
int mvwhline(WINDOW *w, int y, int x, chtype ch, int n);
int mvwvline(WINDOW *w, int y, int x, chtype ch, int n);
int wattron(WINDOW *w, int a);
int wattroff(WINDOW *w, int a);
int wattrset(WINDOW *w, int a);
int touchwin(WINDOW *w);
int wnoutrefresh(WINDOW *w);
int wrefresh(WINDOW *w);
int doupdate(void);
int wgetch(WINDOW *w);
int ungetch(int ch);
void wtimeout(WINDOW *w, int delay);
int nodelay(WINDOW *w, bool bf);
int getmouse(MEVENT *ev);
mmask_t mousemask(mmask_t newmask, mmask_t *oldmask);
int mouseinterval(int ms);
int setcchar(cchar_t *wcval, const wchar_t *wch, attr_t attrs, short pair, const void *opts);
int curs_set(int visibility);
int napms(int ms);
int beep(void);
int flash(void);
int start_color(void);
bool has_colors(void);
bool can_change_color(void);
int init_color(short color, short r, short g, short b);
int init_pair(short pair, short fg, short bg);
int set_escdelay(int ms);
/* RVIP: tile for a stdscr cell (-1 none), valid while the cell keeps the
 * character and attributes it has now (port/tiles.c) */
void wc_settile(int y, int x, int tile);
/* RVIP W4: a window (not a panel) that is a whole pane of its own (be.h
 * P_MSG, P_INV); sent on doupdate() as trimmed lines (be_line) when
 * changed. wc_rowattr: a text pane row's colour ("" = default) and icon
 * tile (-1 none). */
void wc_pane(WINDOW *w, int pane);
void wc_rowattr(int pane, int y, const char *css, int tile);
uint32_t wc_rgb(int colour);   /* palette colour -> 0xRRGGBB */

#define getmaxx(w) ((w) ? (w)->maxx : ERR)
#define getmaxy(w) ((w) ? (w)->maxy : ERR)
#define getbegx(w) ((w) ? (w)->begx : ERR)
#define getbegy(w) ((w) ? (w)->begy : ERR)
#define getcurx(w) ((w) ? (w)->curx : ERR)
#define getcury(w) ((w) ? (w)->cury : ERR)
#define getmaxyx(w, y, x) ((y) = getmaxy(w), (x) = getmaxx(w))
#define getbegyx(w, y, x) ((y) = getbegy(w), (x) = getbegx(w))
#define getyx(w, y, x) ((y) = getcury(w), (x) = getcurx(w))

#define mvwaddch(w, y, x, ch) (wmove(w, y, x) == ERR ? ERR : waddch(w, ch))
#define mvwaddstr(w, y, x, s) (wmove(w, y, x) == ERR ? ERR : waddstr(w, s))
#define mvwaddnstr(w, y, x, s, n) (wmove(w, y, x) == ERR ? ERR : waddnstr(w, s, n))
#define mvwadd_wch(w, y, x, c) (wmove(w, y, x) == ERR ? ERR : wadd_wch(w, c))
#define move(y, x) wmove(stdscr, y, x)
#define addch(ch) waddch(stdscr, ch)
#define mvaddch(y, x, ch) mvwaddch(stdscr, y, x, ch)
#define addstr(s) waddstr(stdscr, s)
#define mvaddstr(y, x, s) mvwaddstr(stdscr, y, x, s)
#define add_wch(c) wadd_wch(stdscr, c)
#define mvadd_wch(y, x, c) mvwadd_wch(stdscr, y, x, c)
#define clear() wclear(stdscr)
#define erase() werase(stdscr)
#define clrtoeol() wclrtoeol(stdscr)
#define clrtobot() wclrtobot(stdscr)
#define refresh() wrefresh(stdscr)
#define getch() wgetch(stdscr)
#define attron(a) wattron(stdscr, a)
#define attroff(a) wattroff(stdscr, a)
#define attrset(a) wattrset(stdscr, a)
#define bkgd(ch) wbkgd(stdscr, ch)
#define hline(ch, n) whline(stdscr, ch, n)
#define vline(ch, n) wvline(stdscr, ch, n)
#define mvhline(y, x, ch, n) mvwhline(stdscr, y, x, ch, n)
#define mvvline(y, x, ch, n) mvwvline(stdscr, y, x, ch, n)
#define chgat(n, a, p, o) wchgat(stdscr, n, a, p, o)
#define mvchgat(y, x, n, a, p, o) mvwchgat(stdscr, y, x, n, a, p, o)
#define timeout(d) wtimeout(stdscr, d)
#define keypad(w, b) OK
#define meta(w, b) OK
#define raw() OK
#define noraw() OK
#define cbreak() OK
#define nocbreak() OK
#define noecho() OK
#define echo() OK
#define nonl() OK
#define nl() OK
#define intrflush(w, b) OK
#define leaveok(w, b) OK
#define use_default_colors() OK
#define flushinp() OK

#endif
