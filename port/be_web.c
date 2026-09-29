/* Web frontend of the NLarn port (RVIP R7): hands pane cells to the page
 * (web/nlarn.js) and takes keys from it. Blocking input and naps use
 * Asyncify (emscripten_sleep). */
#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

#include "be.h"
#include "extdefs.h"   /* nlarn */
#include "game.h"

typedef struct {
    int cols, rows;
    uint32_t *cells;       /* ch, fg, bg | attr << 24, tile (-1 none) per cell */
    int y0, y1;            /* dirty rows [y0, y1) */
    int cy, cx;            /* cursor, cy < 0: hidden */
} pane;

static pane panes[NPANES];

EM_JS(void, js_init, (int p, int cols, int rows), {
    Module.nl.init(p, cols, rows);
});
EM_JS(void, js_draw, (int p, uint32_t *cells, int cols, int y0, int y1, int cy, int cx), {
    Module.nl.draw(p, cells >> 2, cols, y0, y1, cy, cx);
});
EM_JS(void, js_line, (int p, int y, const char *s, const char *css, int tile), {
    Module.nl.line(p, y, UTF8ToString(s), UTF8ToString(css), tile);
});
EM_JS(void, js_rows, (int p, int n), {
    Module.nl.rows(p, n);
});
EM_JS(void, js_cursor, (int p, int y, int x), {
    Module.nl.cursor(p, y, x);
});
EM_JS(int, js_key, (int at_cmd), {
    return Module.nl.key(at_cmd);
});
EM_JS(void, js_popup, (int rows, int cols, int y0, int x0), {
    Module.nl.popup(rows, cols, y0, x0);
});
EM_JS(void, js_popbg, (int rgb), {
    Module.nl.popbg(rgb);
});
EM_JS(int, js_icons, (void), {
    return Module.nl.icons();
});
EM_JS(void, js_hero, (int y, int x, int lvl), {
    Module.nl.hero(y, x, lvl);
});
EM_JS(void, js_prompt, (const char *s), {
    Module.nl.prompt(UTF8ToString(s));
});
EM_JS(void, js_vis, (const char *s), {
    Module.nl.vis(UTF8ToString(s));
});
EM_JS(int, js_want_save, (void), {
    return Module.nl.wantSave();
});
EM_JS(void, js_end, (void), {
    Module.nl.end();
});

int web_at_cmd = 0;       /* set by mainloop() around its command read */
int web_autosaving = 0;   /* game_save() without its "Saving...." pop-up */
EM_JS(void, js_bell, (void), {
    if (Module.nl.bell) Module.nl.bell();
});
EM_JS(void, js_sync, (void), {
    if (Module.nl.sync) Module.nl.sync();
});

static void alloc_pane(int p, int cols, int rows)
{
    pane *q = &panes[p];
    free(q->cells);
    q->cols = cols;
    q->rows = rows;
    q->cells = calloc((size_t)cols * (size_t)rows * 4, sizeof(uint32_t));
    q->y0 = 0;
    q->y1 = rows;
    q->cy = -1;
}

/* only the map is a cell grid (canvas); text panes are lines (RVIP W0 rule 6) */
void be_init(int p, int cols, int rows)
{
    if (p == P_MAP) alloc_pane(p, cols, rows);
    js_init(p, cols, rows);
}

void be_popup(int rows, int cols, int y0, int x0)
{
    if (rows <= 0 || cols <= 0) rows = cols = 0;
    js_popup(rows, cols, y0, x0);
}

void be_popbg(uint32_t rgb) { js_popbg((int)(rgb & 0xffffff)); }

void be_line(int p, int y, const char *s, const char *css, int tile) { js_line(p, y, s, css, tile); }
void be_rows(int p, int n) { js_rows(p, n); }
int be_icons(void) { return js_icons(); }
void be_hero(int y, int x, int level) { js_hero(y, x, level); }
void be_prompt(const char *s)
{
    static char *last = NULL;
    if (last && !strcmp(last, s)) return;
    free(last);
    last = strdup(s);
    js_prompt(s);
}
void be_vis(const char *s)
{
    static char *last = NULL;
    if (last && !strcmp(last, s)) return;
    free(last);
    last = strdup(s);
    js_vis(s);
}
void be_end(void) { js_end(); }

/* autosave (RVIP W5): only while the game waits for a command */
static void autosave(void)
{
    if (!nlarn || !nlarn->p || nlarn->p->hp <= 0) return;
    web_autosaving = 1;
    game_save(nlarn);
    web_autosaving = 0;
}

void be_put(int p, int y, int x, uint32_t ch, uint32_t fg, uint32_t bg, int attr, int tile)
{
    pane *q = &panes[p];
    if (p != P_MAP || !q->cells || y < 0 || x < 0 || y >= q->rows || x >= q->cols) return;
    uint32_t *c = &q->cells[(y * q->cols + x) * 4];
    c[0] = ch;
    c[1] = fg;
    c[2] = bg | (uint32_t)attr << 24;
    c[3] = (uint32_t)tile;
    if (y < q->y0) q->y0 = y;
    if (y + 1 > q->y1) q->y1 = y + 1;
}

void be_cursor(int p, int y, int x)
{
    pane *q = &panes[p];
    if (p != P_MAP) { js_cursor(p, y, x); return; }   /* a text pane's cursor (wcurses.c sends changes) */
    if (q->cy != y || q->cx != x)
    {
        /* redraw the rows of the old and the new cursor */
        if (q->cy >= 0) { if (q->cy < q->y0) q->y0 = q->cy; if (q->cy + 1 > q->y1) q->y1 = q->cy + 1; }
        if (y >= 0) { if (y < q->y0) q->y0 = y; if (y + 1 > q->y1) q->y1 = y + 1; }
    }
    q->cy = y;
    q->cx = x;
}

void be_flush(void)
{
    for (int p = 0; p < NPANES; p++)
    {
        pane *q = &panes[p];
        if (!q->cells || q->y0 >= q->y1) continue;
        js_draw(p, q->cells, q->cols, q->y0, q->y1, q->cy, q->cx);
        q->y0 = q->rows;
        q->y1 = 0;
    }
}

int be_getkey(int timeout_ms)
{
    be_flush();
    int waited = 0;
    for (;;)
    {
        int k = js_key(web_at_cmd);
        if (k >= 0) return k;
        if (web_at_cmd && js_want_save()) autosave();
        if (timeout_ms >= 0 && waited >= timeout_ms) return -1;
        emscripten_sleep(10);
        waited += 10;
    }
}

void be_sleep(int ms)
{
    be_flush();
    emscripten_sleep(ms > 0 ? ms : 0);
}

void be_bell(void) { js_bell(); }

/* RVIP 6b: SOUND("event") in the game (inc/display.h); the page picks a sample */
EM_JS(void, js_sound, (const char *e), {
    if (Module.nl.sound) Module.nl.sound(UTF8ToString(e));
});
void be_sound(const char *event) { js_sound(event); }

void web_sync_files(void) { js_sync(); }
