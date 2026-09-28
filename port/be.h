/* Frontend API of the NLarn web port (RVIP R-frontend, W0).
 * wcurses.c composes the curses screen and routes it to panes; a frontend
 * (be_web.c) shows each pane as a grid of cells. Colours come from the game:
 * every cell carries its RGB foreground and background. */
#ifndef NLARN_BE_H
#define NLARN_BE_H
#include <stdint.h>

/* Panes. Stage 1: everything goes to P_SCREEN (the whole curses screen);
 * the others are reserved for routing stdscr regions and panels later. */
enum { P_SCREEN, P_MAP, P_STATUS, P_MSG, P_POP, NPANES };

/* cell attribute bits sent with each cell */
#define BE_BOLD 1
#define BE_UNDERLINE 2
#define BE_BLINK 4

void be_init(int pane, int cols, int rows);
void be_put(int pane, int y, int x, uint32_t ch, uint32_t fg, uint32_t bg, int attr);
void be_cursor(int pane, int y, int x);       /* y < 0: hidden */
void be_extent(int pane, int cols, int rows); /* cells in use (text panes) */
void be_flush(void);
int be_getkey(int timeout_ms);                /* -1 blocks; returns -1 on timeout */
void be_sleep(int ms);
void be_bell(void);

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
