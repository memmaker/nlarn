/*
 * pathfinding.c
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

#include "extdefs.h"
#include "pathfinding.h"
#include "player.h"
#include "spheres.h"

static path *path_new(position start, position goal);
static path_element *path_element_new(position pos);
static guint path_step_cost(map *m, const path_element* element,
    map_element_t map_elem, bool for_player);
static GPtrArray *path_get_neighbours(map *m, position pos,
    map_element_t element, bool for_player, position goal);

/* --- binary min-heap of open path_elements, keyed by g_score + h_score ---
   Using a heap here (instead of scanning a plain array for the lowest-cost
   element, as before) turns "pick the best open node" from an O(n) into an
   O(log n) operation. Decrease-key is implemented lazily: an element whose
   score improves is simply pushed again under its new key, and a stale
   duplicate is recognised and skipped via the in_closed flag when it is
   later popped. */
typedef struct
{
    guint32 key;
    path_element *el;
} path_heap_entry;

struct path_heap
{
    GArray *entries;
};

static path_heap *path_heap_new(void)
{
    path_heap *h = g_malloc0(sizeof(path_heap));
    h->entries = g_array_new(FALSE, FALSE, sizeof(path_heap_entry));

    return h;
}

static void path_heap_destroy(path_heap *h)
{
    g_array_free(h->entries, TRUE);
    g_free(h);
}

static inline void path_heap_swap(GArray *a, guint i, guint j)
{
    path_heap_entry tmp = g_array_index(a, path_heap_entry, i);
    g_array_index(a, path_heap_entry, i) = g_array_index(a, path_heap_entry, j);
    g_array_index(a, path_heap_entry, j) = tmp;
}

static void path_heap_push(path_heap *h, guint32 key, path_element *el)
{
    path_heap_entry entry = { key, el };
    g_array_append_val(h->entries, entry);

    guint idx = h->entries->len - 1;
    while (idx > 0)
    {
        guint parent = (idx - 1) / 2;

        if (g_array_index(h->entries, path_heap_entry, parent).key
                <= g_array_index(h->entries, path_heap_entry, idx).key)
            break;

        path_heap_swap(h->entries, idx, parent);
        idx = parent;
    }
}

static path_element *path_heap_pop_min(path_heap *h)
{
    guint len = h->entries->len;
    if (len == 0)
        return NULL;

    path_element *top = g_array_index(h->entries, path_heap_entry, 0).el;

    g_array_index(h->entries, path_heap_entry, 0) =
        g_array_index(h->entries, path_heap_entry, len - 1);
    g_array_set_size(h->entries, len - 1);

    guint idx = 0;
    guint size = h->entries->len;

    while (true)
    {
        guint left = 2 * idx + 1;
        guint right = 2 * idx + 2;
        guint smallest = idx;

        if (left < size && g_array_index(h->entries, path_heap_entry, left).key
                < g_array_index(h->entries, path_heap_entry, smallest).key)
            smallest = left;

        if (right < size && g_array_index(h->entries, path_heap_entry, right).key
                < g_array_index(h->entries, path_heap_entry, smallest).key)
            smallest = right;

        if (smallest == idx)
            break;

        path_heap_swap(h->entries, idx, smallest);
        idx = smallest;
    }

    return top;
}

path *path_find(map *m, position start, position goal, map_element_t element)
{
    g_assert(m != NULL);
    g_assert(pos_valid(start));
    g_assert(pos_valid(goal));
    g_assert(element < LE_MAX);

    /* if the starting position is on another map, fail for now */
    /* TODO: could be changed to support 3D path finding */
    if (Z(start) != Z(goal))
        return NULL;

    path *pt = path_new(start, goal);

    /* add start to the open heap and to the set of known nodes */
    path_element *curr = path_element_new(start);
    curr->g_score = 0;
    curr->h_score = pos_distance(start, goal);
    g_hash_table_insert(pt->nodes, GUINT_TO_POINTER(pos_val(start)), curr);
    path_heap_push(pt->open, curr->g_score + curr->h_score, curr);

    /* check if the path is being determined for the player */
    bool for_player = pos_identical(start, nlarn->p->pos);

    while ((curr = path_heap_pop_min(pt->open)) != NULL)
    {
        if (curr->in_closed)
        {
            /* stale duplicate left behind by a since-superseded, cheaper
               route to this node (see the "decrease-key" push below) */
            continue;
        }

        curr->in_closed = true;

        if (pos_identical(curr->pos, pt->goal))
        {
            /* arrived at goal - reconstruct path */
            do {
                /* don't need the starting point in the path */
                if (curr->parent != NULL)
                    g_queue_push_head(pt->path, curr);

                curr = curr->parent;
            } while (curr != NULL);

            return pt;
        }

        GPtrArray *neighbours = path_get_neighbours(m, curr->pos, element, for_player, pt->goal);

        while (neighbours->len)
        {
            path_element *next = g_ptr_array_remove_index_fast(neighbours,
                                                neighbours->len - 1);

            const guint32 next_g_score = curr->g_score
                + path_step_cost(m, next, element, for_player);

            path_element *known = g_hash_table_lookup(pt->nodes,
                    GUINT_TO_POINTER(pos_val(next->pos)));

            if (known == NULL)
            {
                /* first time this position is reached */
                next->g_score = next_g_score;
                next->h_score = pos_distance(next->pos, pt->goal);
                next->parent  = curr;

                g_hash_table_insert(pt->nodes,
                        GUINT_TO_POINTER(pos_val(next->pos)), next);
                path_heap_push(pt->open, next->g_score + next->h_score, next);
            }
            else
            {
                /* position already known; next was only needed to carry
                   its coordinates, the canonical node is "known" */
                g_free(next);

                if (!known->in_closed && next_g_score < known->g_score)
                {
                    known->g_score = next_g_score;
                    known->parent  = curr;
                    path_heap_push(pt->open, known->g_score + known->h_score, known);
                }
            }
        }

        g_ptr_array_free(neighbours, true);
    }

    /* could not find a path */
    path_destroy(pt);

    return NULL;
}

void path_destroy(path *path)
{
    g_assert(path != NULL);

    /* the nodes table owns exactly one path_element per position ever
       reached during the search (whether still open, closed, or a dead
       end) - free them all here */
    g_hash_table_destroy(path->nodes);
    path_heap_destroy(path->open);

    g_queue_free(path->path);
    g_free(path);
}

static void path_element_free(gpointer el)
{
    g_free(el);
}

static path *path_new(position start, position goal)
{
    g_assert(pos_valid(start));
    g_assert(pos_valid(goal));

    path *pt = g_malloc0(sizeof(path));

    pt->open  = path_heap_new();
    pt->nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                       NULL, path_element_free);
    pt->path  = g_queue_new();

    pt->start = start;
    pt->goal  = goal;

    return pt;
}

static path_element *path_element_new(position pos)
{
    g_assert(pos_valid(pos));

    path_element *lpe = g_malloc0(sizeof(path_element));
    lpe->pos = pos;

    return lpe;
}

/* calculate the cost of stepping into this new field */
static guint path_step_cost(map *m, const path_element* element,
    map_element_t map_elem, bool for_player)
{
    map_tile_t tt;
    guint32 step_cost = 1; /* at least 1 movement cost */

    /* get the tile type of the map tile */
    if (for_player)
    {
        tt = player_memory_of(nlarn->p, element->pos).type ;
    }
    else
    {
        tt = map_tiletype_at(m, element->pos);
    }

    /* penalize for traps known to the player */
    if (for_player && player_memory_of(nlarn->p, element->pos).trap)
    {
        const trap_t trap = map_trap_at(m, element->pos);
        /* especially ones that may cause detours */
        if (trap == TT_TELEPORT || trap == TT_TRAPDOOR)
            step_cost += 50;
        else
            step_cost += 10;
    }

    /* penalize fields occupied by monsters: always for monsters,
       for the player only if (s)he can see the monster */
    monster *mon = map_get_monster_at(m, element->pos);
    if (mon != NULL && (!for_player || monster_in_sight(mon)))
    {
        step_cost += 10;
    }

    /* penalize fields covered with water, fire or cloud */
    switch (tt)
    {
    case LT_WATER:
        if (map_elem == LE_SWIMMING_MONSTER || map_elem == LE_FLYING_MONSTER)
            break;
        /* else fall through */
    case LT_FIRE:
    case LT_CLOUD:
        step_cost += 50;
        break;
    default:
        break;
    }

    return step_cost;
}

static GPtrArray *path_get_neighbours(map *m, position pos,
                                      map_element_t element,
                                      bool for_player,
                                      position goal)
{
    GPtrArray *neighbours = g_ptr_array_new();

    for (direction dir = GD_NONE + 1; dir < GD_MAX; dir++)
    {
        if (dir == GD_CURR)
            continue;

        position new_pos = pos_move(pos, dir);

        if (!pos_valid(new_pos))
            continue;

        /* the goal tile is always reachable regardless of who occupies it */
        if (pos_identical(new_pos, goal))
        {
            g_ptr_array_add(neighbours, path_element_new(new_pos));
            continue;
        }

        /* block the player's position as an intermediate step */
        if (pos_identical(new_pos, nlarn->p->pos))
            continue;

        /* block positions occupied by spheres */
        bool blocked_by_sphere = false;
        for (guint i = 0; i < nlarn->spheres->len; i++)
        {
            sphere *s = g_ptr_array_index(nlarn->spheres, i);
            if (pos_identical(s->pos, new_pos))
            {
                blocked_by_sphere = true;
                break;
            }
        }
        if (blocked_by_sphere)
            continue;

        if ((for_player && mt_is_passable(player_memory_of(nlarn->p, new_pos).type))
                || (!for_player && monster_valid_dest(m, new_pos, element)))
        {
            g_ptr_array_add(neighbours, path_element_new(new_pos));
        }
    }

    return neighbours;
}
