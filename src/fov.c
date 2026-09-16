/*
 * fov.c
 * Copyright (C) 2009-2026 Joachim de Groot <jdegroot@web.de>
 *
 * NLarn is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NLarn is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <string.h>
#include "fov.h"
#include "game.h"
#include "map.h"
#include "extdefs.h"
#include "position.h"

/* a slope, represented as the exact fraction y/x (never reduced to a
   float) so that all comparisons below are exact integer arithmetic */
typedef struct
{
    int y;
    int x;
} fov_slope;

static void fov_calculate_octant(fov *fv, map *m, position center,
                                 bool infravision, int radius,
                                 int xx, int xy, int yx, int yy,
                                 int x, fov_slope top, fov_slope bottom);

static gint fov_visible_monster_sort(gconstpointer a, gconstpointer b, gpointer center);

struct fov
{
    /* the actual field of vision */
    guchar data[MAP_MAX_Y][MAP_MAX_X];

    /* the center of the fov */
    position center;

    /* The list of visible monsters - it's a hash as fields may get visited
       twice, which means that monsters may get added to the list multiple
       times. The hash overwrites duplicate values. */
    GHashTable *mlist;
};

fov *fov_new()
{
    fov *new_fov = g_new0(fov, 1);
    new_fov->center = pos_invalid;
    new_fov->mlist = g_hash_table_new(g_direct_hash, g_direct_equal);

    return new_fov;
}
/* This and the function fov_calculate_octant() implement Adam Milazzo's
 * improved recursive shadowcasting algorithm (see
 * http://www.adammil.net/blog/v125_Roguelike_Vision_Algorithms.html),
 * which fixes several correctness issues (e.g. asymmetric visibility,
 * missing wall tiles at shadow boundaries) present in the "classic"
 * recursive shadowcasting algorithm this used to be based on. Slopes are
 * kept as exact y/x fractions and compared via cross-multiplication
 * instead of floats, avoiding precision-related artifacts too.
 */
void fov_calculate(fov *fv, map *m, position pos, int radius, bool infravision)
{
    const int mult[4][8] =
    {
        { 1,  0,  0, -1, -1,  0,  0,  1 },
        { 0,  1, -1,  0,  0, -1,  1,  0 },
        { 0,  1,  1,  0,  0, -1, -1,  0 },
        { 1,  0,  0,  1, -1,  0,  0, -1 }
    };

    /* reset the entire fov to unseen */
    fov_reset(fv);

    /* set the center of the fov */
    fv->center = pos;

    /* determine which fields are visible */
    for (int octant = 0; octant < 8; octant++)
    {
        fov_calculate_octant(fv, m, pos, infravision, radius,
                             mult[0][octant], mult[1][octant],
                             mult[2][octant], mult[3][octant],
                             1, (fov_slope){ 1, 1 }, (fov_slope){ 0, 1 });
    }

    fov_set(fv, pos, true, infravision, true);
}

bool fov_get(const fov *fv, position pos)
{
    g_assert (fv != NULL);
    g_assert (pos_valid(pos));

    return fv->data[Y(pos)][X(pos)];
}

void fov_set(fov *fv, position pos, guchar visible,
             bool infravision, bool check_monster)
{
    g_assert (fv != NULL);
    g_assert (pos_valid(pos));

    fv->data[Y(pos)][X(pos)] = visible;
    monster *mon;

    /* If advised to do so, check if there is a monster at that
       position. Must not be an unknown mimic or invisible. */
    if (check_monster
        && ((mon = map_get_monster_at(game_map(nlarn, Z(pos)), pos)))
        && !monster_unknown(mon)
        && (!monster_flags(mon, INVISIBLE) || infravision))
    {
        /* found a visible monster -> add it to the list */
        g_hash_table_insert(fv->mlist, mon, 0);
    }
}

void fov_reset(fov *fv)
{
    g_assert (fv != NULL);

    /* set fov_data to false */
    memset(fv->data, 0, MAP_MAX_Y * MAP_MAX_X * sizeof(guchar));

    /* set the center to an invalid position */
    fv->center = pos_invalid;

    /* clean list of visible monsters */
    g_hash_table_remove_all(fv->mlist);
}

monster *fov_get_closest_monster(fov *fv)
{
    monster *closest_monster = NULL;

    if (g_hash_table_size(fv->mlist) > 0)
    {
        /* get the list of all visible monsters */
        GList *mlist = g_hash_table_get_keys(fv->mlist);

        /* sort the monsters list by distance */
        mlist = g_list_sort_with_data(mlist, fov_visible_monster_sort,
                                      &fv->center);

        /* get the first element in the list */
        closest_monster = mlist->data;

        g_list_free(mlist);
    }

    return closest_monster;
}

GList *fov_get_visible_monsters(fov *fv)
{
    GList *mlist = NULL;

    if (g_hash_table_size(fv->mlist) != 0)
    {
        /* get a GList of all visible monster all */
        mlist = g_hash_table_get_keys(fv->mlist);

        /* sort the list of monster by distance */
        mlist = g_list_sort_with_data(mlist, fov_visible_monster_sort,
                                      &fv->center);
    }

    return mlist;
}

void fov_free(fov *fv)
{
    g_assert (fv != NULL);

    /* free the allocated memory */
    g_hash_table_destroy(fv->mlist);
    g_free(fv);
}

/* this > y/x */
static inline bool fov_slope_greater(fov_slope s, int y, int x)
{
    return s.y * x > s.x * y;
}

/* this >= y/x */
static inline bool fov_slope_greater_or_equal(fov_slope s, int y, int x)
{
    return s.y * x >= s.x * y;
}

/* this < y/x */
static inline bool fov_slope_less(fov_slope s, int y, int x)
{
    return s.y * x < s.x * y;
}

/* translate octant-local (x, y) coordinates to a map position, and report
   whether that position blocks light (out-of-map counts as blocking) */
static inline bool fov_octant_blocked(map *m, position center,
                                      int xx, int xy, int yx, int yy,
                                      int x, int y)
{
    int mx = X(center) + x * xx + y * xy;
    int my = Y(center) + x * yx + y * yy;

    if (mx < 0 || mx >= MAP_MAX_X || my < 0 || my >= MAP_MAX_Y)
        return true;

    position pos = { { mx, my, Z(center) } };
    return !map_pos_transparent(m, pos);
}

static inline void fov_octant_set_visible(fov *fv, position center, bool infravision,
                                          int xx, int xy, int yx, int yy, int x, int y)
{
    int mx = X(center) + x * xx + y * xy;
    int my = Y(center) + x * yx + y * yy;

    if (mx < 0 || mx >= MAP_MAX_X || my < 0 || my >= MAP_MAX_Y)
        return;

    position pos = { { mx, my, Z(center) } };
    fov_set(fv, pos, true, infravision, true);
}

static void fov_calculate_octant(fov *fv, map *m, position center,
                                 bool infravision, int radius,
                                 int xx, int xy, int yx, int yy,
                                 int x, fov_slope top, fov_slope bottom)
{
    int radius_squared = radius * radius;

    for (; x <= radius; x++)
    {
        /* find the y coordinate where the top vision line enters this
           column, nudging it by one where needed so that walls right at
           the boundary are neither missed nor wrongly included */
        int top_y;
        if (top.x == 1)
        {
            top_y = x;
        }
        else
        {
            top_y = ((x * 2 - 1) * top.y + top.x) / (top.x * 2);

            if (fov_octant_blocked(m, center, xx, xy, yx, yy, x, top_y))
            {
                if (fov_slope_greater_or_equal(top, top_y * 2 + 1, x * 2)
                        && !fov_octant_blocked(m, center, xx, xy, yx, yy, x, top_y + 1))
                    top_y++;
            }
            else
            {
                int ax = x * 2;
                if (fov_octant_blocked(m, center, xx, xy, yx, yy, x + 1, top_y + 1))
                    ax++;
                if (fov_slope_greater(top, top_y * 2 + 1, ax))
                    top_y++;
            }
        }

        /* likewise for the bottom vision line */
        int bottom_y;
        if (bottom.y == 0)
        {
            bottom_y = 0;
        }
        else
        {
            bottom_y = ((x * 2 - 1) * bottom.y + bottom.x) / (bottom.x * 2);

            if (fov_slope_greater_or_equal(bottom, bottom_y * 2 + 1, x * 2)
                    && fov_octant_blocked(m, center, xx, xy, yx, yy, x, bottom_y)
                    && !fov_octant_blocked(m, center, xx, xy, yx, yy, x, bottom_y + 1))
                bottom_y++;
        }

        int was_opaque = -1; /* 0 = false, 1 = true, -1 = n/a (first row) */

        for (int y = top_y; y >= bottom_y; y--)
        {
            if (x * x + y * y > radius_squared)
                continue; /* outside the circular range, but may still cast a shadow */

            bool is_opaque = fov_octant_blocked(m, center, xx, xy, yx, yy, x, y);

            /* a wall tile is always visible (we see its near face); a
               floor tile at the very edge of the swept region is only
               visible if the sweep actually reaches its center rather
               than just clipping its corner */
            bool is_visible = is_opaque
                || ((y != top_y || fov_slope_greater(top, y * 4 - 1, x * 4 + 1))
                    && (y != bottom_y || fov_slope_less(bottom, y * 4 + 1, x * 4 - 1)));

            if (is_visible)
                fov_octant_set_visible(fv, center, infravision, xx, xy, yx, yy, x, y);

            if (x == radius)
                continue;

            if (is_opaque)
            {
                if (was_opaque == 0)
                {
                    /* a wall starts here; the part of the beam still
                       visible above it gets its own scan, while this
                       call continues below to look for the shadow's end */
                    int nx = x * 2;
                    int ny = y * 2 + 1;

                    if (fov_octant_blocked(m, center, xx, xy, yx, yy, x, y + 1))
                        nx--;

                    if (fov_slope_greater(top, ny, nx))
                    {
                        if (y == bottom_y)
                        {
                            bottom.y = ny;
                            bottom.x = nx;
                            break;
                        }

                        fov_calculate_octant(fv, m, center, infravision, radius,
                                             xx, xy, yx, yy, x + 1, top,
                                             (fov_slope){ ny, nx });
                    }
                    else if (y == bottom_y)
                    {
                        return;
                    }
                }

                was_opaque = 1;
            }
            else
            {
                if (was_opaque > 0)
                {
                    /* the wall we were scanning past has ended; narrow
                       the top bound so the shadow it cast is excluded
                       from here on */
                    int nx = x * 2;
                    int ny = y * 2 + 1;

                    if (fov_octant_blocked(m, center, xx, xy, yx, yy, x + 1, y + 1))
                        nx++;

                    if (fov_slope_greater_or_equal(bottom, ny, nx))
                        return;

                    top.y = ny;
                    top.x = nx;
                }

                was_opaque = 0;
            }
        }

        if (was_opaque != 0)
            break;
    }
}

static gint fov_visible_monster_sort(gconstpointer a, gconstpointer b, gpointer center)
{
    monster *ma = (monster *) a;
    monster *mb = (monster *) b;

    int da = pos_distance(*(position *)center, monster_pos(ma));
    int db = pos_distance(*(position *)center, monster_pos(mb));

    if (da < db)
        return -1;

    if (da > db)
        return 1;

    return 0;
}
