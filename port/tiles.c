/* Map cell -> Amiga Larn tile (web/tiles.png, port/tilemap.h by
 * port/mktiles.py), decided from the game's data (RVIP W0), never from the
 * characters on screen.
 *
 * tiles_paint() runs at the end of display_paint_screen(): for every map
 * cell it follows the same rules as the text map (visible truth, else the
 * player's memory; monsters in sight; spheres; the player) and hands the
 * tile id to the curses shim with wc_settile(). The shim sends it along
 * with the cell only while the character there is still the one the map
 * pass drew, so animations, the targeting cursor and pop-ups stay text. */
#include <curses.h>
#include <glib.h>
#include <stdlib.h>

#include "display.h"
#include "extdefs.h"
#include "fov.h"
#include "game.h"
#include "items.h"
#include "map.h"
#include "monsters.h"
#include "player.h"
#include "spheres.h"
#include "tiles.h"
#include "tilemap.h"
#include "be.h"

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define AT(a, i) ((i) >= 0 && (i) < N(a) ? (a)[i] : -1)

/* last tile of the item seen on a cell, so a remembered pile keeps its
 * picture (the player's memory keeps only the item type); not saved */
static short seen_item[MAP_MAX][MAP_MAX_Y][MAP_MAX_X];

int tiles_item(item *it)
{
    int id = (int)it->id;
    switch (it->type)
    {
    case IT_AMULET: return AT(amulet_flavour_tile, nlarn->amulet_material_mapping[id]);
    case IT_RING: return AT(ring_flavour_tile, nlarn->ring_material_mapping[id]);
    case IT_AMMO: return AT(ammo_tile, id);
    case IT_ARMOUR: return AT(armour_tile, id);
    case IT_BOOK: return AT(book_tile, id);
    case IT_CONTAINER: return AT(container_tile, id);
    case IT_GEM: return AT(gem_tile, id);
    case IT_POTION: return AT(potion_tile, id);
    case IT_SCROLL: return AT(scroll_tile, id);
    case IT_WEAPON: return AT(weapon_tile, id);
    default: return AT(it_tile, (int)it->type);
    }
}

static bool is_wall(map *m, position pos)
{
    if (!pos_valid(pos)) return false;
    sobject_t so = map_sobject_at(m, pos);
    return map_tiletype_at(m, pos) == LT_WALL || so == LS_CLOSEDDOOR || so == LS_OPENDOOR;
}

static bool is_mountain(map *m, position pos)
{
    return pos_valid(pos) && map_tiletype_at(m, pos) == LT_MOUNTAIN;
}

static int neighbours(map *m, position pos, bool (*same)(map *, position))
{
    return (same(m, pos_move(pos, GD_NORTH)) ? 1 : 0) | (same(m, pos_move(pos, GD_EAST)) ? 2 : 0)
        | (same(m, pos_move(pos, GD_SOUTH)) ? 4 : 0) | (same(m, pos_move(pos, GD_WEST)) ? 8 : 0);
}

static int terrain(map *m, position pos, map_tile_t t)
{
    int v = AT(lt_tile, (int)t);
    if (v == -2) return wall_tile[neighbours(m, pos, is_wall)];
    if (v == -3) return mountain_tile[neighbours(m, pos, is_mountain)];
    return v;
}

/* the item map_get_tile() shows for a pile: a gem, else gold, else the top one */
static item *shown_item(inventory *inv)
{
    if (inv_length_filtered(inv, item_filter_gems) > 0)
        return inv_get_filtered(inv, 0, item_filter_gems);
    if (inv_length_filtered(inv, item_filter_gold) > 0)
        return inv_get_filtered(inv, 0, item_filter_gold);
    return inv_get(inv, inv_length(inv) - 1);
}

static int cell(player *p, map *m, position pos)
{
    const int z = Z(pos), y = Y(pos), x = X(pos);
    if (game_fullvis(nlarn) || fov_get(p->fv, pos))
    {
        /* the truth, in map_get_tile()'s order */
        inventory **inv = map_ilist_at(m, pos);
        if (map_sobject_at(m, pos))
            return AT(so_tile, (int)map_sobject_at(m, pos));
        if (inv_length(*inv) > 0)
        {
            item *it = shown_item(*inv);
            int t = tiles_item(it);
            seen_item[z][y][x] = (short)((int)it->type << 10 | (t + 1));
            return t;
        }
        seen_item[z][y][x] = 0;
        if (map_trap_at(m, pos) && (game_fullvis(nlarn) || player_memory_of(p, pos).trap))
            return AT(trap_tile, (int)map_trap_at(m, pos));
        return terrain(m, pos, map_tiletype_at(m, pos));
    }

    /* the player's memory, dimmed where the text map greys it */
    player_tile_memory *mem = &player_memory_of(p, pos);
    if (mem->sobject)
        return AT(so_tile, (int)map_sobject_at(m, pos));
    if (mem->item)
    {
        int s = seen_item[z][y][x];
        if (s && (s >> 10) == (int)mem->item) return (s & 0x3ff) - 1;
        return AT(it_tile, (int)mem->item);
    }
    if (mem->trap)
        return AT(trap_tile, (int)map_trap_at(m, pos));
    if (mem->type == LT_NONE) return -1;
    int t = terrain(m, pos, mem->type);
    return t < 0 ? t : t | TILE_DIM;
}

static int monster_tile(monster *mo)
{
    inventory **inv = monster_inv(mo);
    if (monster_unknown(mo) && inv_length(*inv) > 0)
        return tiles_item(inv_get(*inv, 0));      /* a mimic shows its disguise */
    return AT(mon_tile, (int)monster_type(mo));
}

void tiles_paint(player *p)
{
    map *m = game_map(nlarn, Z(p->pos));
    position pos = pos_invalid;
    Z(pos) = Z(p->pos);
    for (Y(pos) = 0; Y(pos) < MAP_MAX_Y; Y(pos)++)
        for (X(pos) = 0; X(pos) < MAP_MAX_X; X(pos)++)
            wc_settile(Y(pos), X(pos), cell(p, m, pos));

    /* monsters shown (as in display_paint_screen) */
    for (Y(pos) = 0; Y(pos) < MAP_MAX_Y; Y(pos)++)
        for (X(pos) = 0; X(pos) < MAP_MAX_X; X(pos)++)
        {
            monster *mo = map_get_monster_at(m, pos);
            if (mo && (game_fullvis(nlarn) || player_effect(p, ET_DETECT_MONSTER)
                        || monster_in_sight(mo)))
            {
                position mp = monster_pos(mo);
                wc_settile(Y(mp), X(mp), monster_tile(mo));
            }
        }

    for (guint i = 0; i < nlarn->spheres->len; i++)
    {
        sphere *s = g_ptr_array_index(nlarn->spheres, i);
        if (Z(s->pos) == Z(p->pos) && (game_fullvis(nlarn) || fov_get(p->fv, s->pos)))
            wc_settile(Y(s->pos), X(s->pos), SPHERE_TILE);
    }

    wc_settile(Y(p->pos), X(p->pos), PLAYER_TILE);

    be_hero(Y(p->pos), X(p->pos), Z(p->pos));
    tiles_visible(p);
}

/* Visible window (RVIP W4): monsters the map shows (nearest first), then
 * the items lying in view; glyph, name, colour and tile from the game. */
static int dist(position a, position b)
{
    int dx = abs(X(a) - X(b)), dy = abs(Y(a) - Y(b));
    return dx > dy ? dx : dy;
}

static void add_line(GString *s, char kind, gunichar glyph, const char *name, colour_t c, int tile)
{
    gchar g[8];
    g[g_unichar_to_utf8(glyph, g)] = 0;
    uint32_t rgb = wc_rgb((int)c);
    g_string_append_printf(s, "%c%s%s\t#%06x\t%d\n", kind, g, name, (unsigned)rgb, tile);
}

void tiles_visible(player *p)
{
    map *m = game_map(nlarn, Z(p->pos));
    position pos = pos_invalid;
    Z(pos) = Z(p->pos);
    GString *s = g_string_new(NULL);
    GPtrArray *mons = g_ptr_array_new();
    if (!player_effect(p, ET_BLINDNESS))
        for (Y(pos) = 0; Y(pos) < MAP_MAX_Y; Y(pos)++)
            for (X(pos) = 0; X(pos) < MAP_MAX_X; X(pos)++)
            {
                monster *mo = map_get_monster_at(m, pos);
                if (mo && (game_fullvis(nlarn) || player_effect(p, ET_DETECT_MONSTER)
                            || monster_in_sight(mo)))
                    g_ptr_array_add(mons, mo);
            }
    /* nearest first (few monsters: insertion sort) */
    for (guint i = 1; i < mons->len; i++)
        for (guint j = i; j > 0 && dist(monster_pos(g_ptr_array_index(mons, j)), p->pos)
                < dist(monster_pos(g_ptr_array_index(mons, j - 1)), p->pos); j--)
        {
            gpointer t = mons->pdata[j]; mons->pdata[j] = mons->pdata[j - 1]; mons->pdata[j - 1] = t;
        }
    for (guint i = 0; i < mons->len && s->len < 6000; i++)
    {
        monster *mo = g_ptr_array_index(mons, i);
        if (monster_unknown(mo)) continue;   /* a mimic passes for an item */
        add_line(s, 'M', monster_glyph(mo), monster_get_name(mo), monster_color(mo), monster_tile(mo));
    }
    g_ptr_array_free(mons, TRUE);

    for (Y(pos) = 0; Y(pos) < MAP_MAX_Y; Y(pos)++)
        for (X(pos) = 0; X(pos) < MAP_MAX_X; X(pos)++)
        {
            if (!(game_fullvis(nlarn) || fov_get(p->fv, pos))) continue;
            inventory **inv = map_ilist_at(m, pos);
            for (guint i = 0; i < inv_length(*inv) && s->len < 6000; i++)
            {
                item *it = inv_get(*inv, i);
                gchar *d = item_describe_gc(it, player_item_known(p, it), false, false, GC_NOM);
                add_line(s, 'I', item_glyph(it->type), d, item_colour(it), tiles_item(it));
                g_free(d);
            }
        }
    be_vis(s->str);
    g_string_free(s, TRUE);
}
