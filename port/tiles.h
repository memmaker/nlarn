/* Map tiles of the NLarn web port (port/tiles.c, RVIP W0: C picks the tile). */
#ifndef NLARN_TILES_H
#define NLARN_TILES_H
struct player;
struct item;

#define TILE_DIM 0x8000   /* tile flag: remembered, not in view (drawn dimmed) */

void tiles_paint(struct player *p);   /* end of display_paint_screen() */
int tiles_item(struct item *it);
void tiles_visible(struct player *p);   /* Visible window (be_vis) */
#endif
