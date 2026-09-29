/* Frontend API of the NLarn web port (RVIP R-frontend, W0).
 * wcurses.c composes the curses screen and routes it to panes; a frontend
 * (be_web.c) shows each pane as a grid of cells. Colours come from the game:
 * every cell carries its RGB foreground and background. */
#ifndef NLARN_BE_H
#define NLARN_BE_H
#include <stdint.h>

/* Panes (RVIP W0/W4), routed by wcurses.c: stdscr regions go to P_MAP
 * (the 67x17 map) and P_STATUS (right column + the two lines under the map);
 * windows the game registers with wc_pane() fill P_MSG (message history)
 * and P_INV (inventory); the visible panels (pop-ups) are composed into
 * P_POP. The Visible list is a text string (be_vis). */
enum { P_MAP, P_STATUS, P_MSG, P_INV, P_POP, NPANES };

/* cell attribute bits sent with each cell */
#define BE_BOLD 1
#define BE_UNDERLINE 2
#define BE_BLINK 4
#define BE_REVERSE 8   /* text panes: standout (the page shows the map's bg) */

void be_init(int pane, int cols, int rows);
/* tile: -1 = text; else a tile id of web/tiles.png (port/tilemap.h),
 * | TILE_DIM (0x8000, port/tiles.h) for a remembered cell drawn dimmed */
void be_put(int pane, int y, int x, uint32_t ch, uint32_t fg, uint32_t bg, int attr, int tile); /* P_MAP only */
void be_cursor(int pane, int y, int x);       /* y < 0: hidden */
/* text panes (P_STATUS, P_MSG, P_INV, P_POP; RVIP W0 rule 6): row y as
 * trimmed text, reverse-video cells between \x01 and \x02, cell colours
 * as runs "\x05#rrggbb" (bold: "\x05*#rrggbb"; own background: "/#rrggbb"
 * appended) ... "\x06"; the row's colour
 * ("" = default) and icon tile (-1 none); and the rows in use */
void be_line(int pane, int y, const char *text, const char *css, int tile);
void be_rows(int pane, int rows);
void be_flush(void);
void be_popup(int rows, int cols, int y0, int x0); /* rows 0: closed; y0/x0: screen origin (mouse) */
void be_popbg(uint32_t rgb); /* the pop-up pane's own background (the window's colour) */
int be_icons(void);                           /* a tile set is shown (Inventory rows get icons) */
void be_hero(int y, int x, int level);        /* the player's map cell: the page centres on it */
void be_prompt(const char *s);                /* prompt line over the map (newest message) */
void be_vis(const char *s);                   /* Visible list: lines "M|I<glyph><name>\t<css>\t<tile>" */
void be_end(void);                            /* main() returned: the page offers a new game */
extern int web_at_cmd;                        /* the game waits for a command (autosave, prompt line) */
int be_getkey(int timeout_ms);                /* -1 blocks; returns -1 on timeout */
void be_sleep(int ms);
void be_bell(void);
void be_sound(const char *event);            /* sound event (SOUND() in inc/display.h) */

/* keys from the frontend: plain codes are characters / curses KEY_*,
 * mouse events are BE_MOUSE | button << 16 | y << 8 | x */
#define BE_MOUSE 0x40000000
#define BE_MOUSE_B1_DOWN 1
#define BE_MOUSE_B1_UP 2
#define BE_MOUSE_B3_DOWN 3
#define BE_MOUSE_WHEEL_UP 4
#define BE_MOUSE_WHEEL_DOWN 5
#define BE_MOUSE_MOVE 6

void web_sync_files(void);                    /* IDBFS write-back */
#endif
