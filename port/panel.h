/* Panels for the NLarn web port's in-memory curses (see curses.h). */
#ifndef NLARN_WPANEL_H
#define NLARN_WPANEL_H
#include <curses.h>

typedef struct panel {
    WINDOW *win;
    bool hidden;
    struct panel *above, *below;
} PANEL;

PANEL *new_panel(WINDOW *w);
int del_panel(PANEL *p);
int hide_panel(PANEL *p);
int show_panel(PANEL *p);
int top_panel(PANEL *p);
int bottom_panel(PANEL *p);
int move_panel(PANEL *p, int y, int x);
int replace_panel(PANEL *p, WINDOW *w);
bool panel_hidden(const PANEL *p);
WINDOW *panel_window(const PANEL *p);
void update_panels(void);
#endif
