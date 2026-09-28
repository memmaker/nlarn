/*
 * explore.c
 * Auto-explore ('X') and walking to the nearest known stairs ('<', '>').
 * Added by the RVIP web port.
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

#include <curses.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include "display.h"
#include "explore.h"
#include "extdefs.h"
#include "game.h"
#include "grammar.h"
#include "map.h"
#include "monsters.h"
#include "sobjects.h"

/* 0 = idle, 'X' = explore, '<' / '>' = walk to stairs */
static int mode = 0;

/* cells the player has stood on, per level (a frontier cell stays a
   target until then) */
static guint8 visited[MAP_MAX][MAP_MAX_Y][MAP_MAX_X];

/* the search treats visible monsters as walls unless this is set */
static bool through_monsters = false;

/* state of the step in progress */
static bool stepped;                /* a step was taken this turn */
static int step_z;
static position step_from;
static bool step_door;              /* the step opens a closed door */
static message_log_entry *log_last; /* newest log entry before the step */
static gchar *log_pending;          /* unflushed text from before the step */
static GPtrArray *quiet;            /* what peaceful monsters said during the step */

void explore_reset(void)
{
    memset(visited, 0, sizeof(visited));
    mode = 0;
}

void explore_visit(player *p)
{
    if (pos_valid(p->pos))
        visited[Z(p->pos)][Y(p->pos)][X(p->pos)] = 1;
}

bool explore_active(void)
{
    return mode != 0;
}

void explore_cancel(void)
{
    mode = 0;
}

/* the "known grid" test: what the player remembers */
static inline player_tile_memory *mem(player *p, position pos)
{
    return &player_memory_of(p, pos);
}

static bool known(player *p, position pos)
{
    return mem(p, pos)->type != LT_NONE;
}

/* may the walker step here? known, passable, no known trap, no harmful
   terrain, no visible monster; closed doors are opened on the way */
static bool walkable(player *p, position pos)
{
    player_tile_memory *m = mem(p, pos);

    if (m->type == LT_NONE || !mt_is_passable(m->type))
        return false;
    if (m->type == LT_FIRE || m->type == LT_CLOUD)
        return false;
    if (m->trap != TT_NONE)
        return false;
    if (!so_is_passable(m->sobject) && m->sobject != LS_CLOSEDDOOR)
        return false;

    monster *mon = map_get_monster_at(game_map(nlarn, Z(pos)), pos);
    if (!through_monsters && mon != NULL && monster_in_sight(mon))
        return false;

    return true;
}

static bool stair_target(sobject_t so, int m)
{
    /* the volcanic shafts skip levels: never walked to, only taken
       when standing on them */
    if (m == '>')
        return so == LS_STAIRSDOWN || so == LS_CAVERNS_ENTRY;
    else
        return so == LS_STAIRSUP || so == LS_CAVERNS_EXIT;
}

static bool is_target(player *p, position pos, int m)
{
    if (m != 'X')
        return stair_target(mem(p, pos)->sobject, m);

    if (visited[Z(pos)][Y(pos)][X(pos)])
        return false;

    /* an item the player has not stood on yet */
    if (mem(p, pos)->item != IT_NONE)
        return true;

    /* a frontier: next to unknown space */
    for (direction dir = GD_NONE + 1; dir < GD_MAX; dir++)
    {
        if (dir == GD_CURR)
            continue;
        position n = pos_move(pos, dir);
        if (pos_valid(n) && !known(p, n))
            return true;
    }

    return false;
}

/* cost of stepping onto a cell: stairs, fountains, altars etc. cost a
   little more, so that of two equally short routes the walker takes the
   one that does not cross them (standing on them prints a message, which
   would stop the walk) */
static int step_cost(player *p, position pos)
{
    sobject_t so = mem(p, pos)->sobject;
    return (so == LS_NONE || so == LS_OPENDOOR || so == LS_CLOSEDDOOR) ? 10 : 11;
}

/* shortest-path search (Dijkstra) over the known grid from the player;
   returns the first step towards the nearest target, or pos_invalid.
   Closed doors are walkable: stepping into one opens it. */
static position first_step(player *p, int m)
{
    static int dist[MAP_MAX_Y][MAP_MAX_X];
    static gint16 parent[MAP_MAX_Y][MAP_MAX_X];
    static bool done[MAP_MAX_Y][MAP_MAX_X];

    for (int y = 0; y < MAP_MAX_Y; y++)
        for (int x = 0; x < MAP_MAX_X; x++)
        {
            dist[y][x] = G_MAXINT;
            parent[y][x] = -1;
            done[y][x] = false;
        }

    position start = p->pos;
    dist[Y(start)][X(start)] = 0;

    while (true)
    {
        /* the open cell closest to the player (the map is small) */
        int best = G_MAXINT;
        position cur = start;
        for (int y = 0; y < MAP_MAX_Y; y++)
            for (int x = 0; x < MAP_MAX_X; x++)
                if (!done[y][x] && dist[y][x] < best)
                {
                    best = dist[y][x];
                    X(cur) = x;
                    Y(cur) = y;
                }

        if (best == G_MAXINT)
            return pos_invalid;

        done[Y(cur)][X(cur)] = true;

        if (!pos_identical(cur, start) && is_target(p, cur, m))
        {
            /* walk back to the cell next to the start */
            while (true)
            {
                int pi = parent[Y(cur)][X(cur)];
                position prev = cur;
                X(prev) = pi % MAP_MAX_X;
                Y(prev) = pi / MAP_MAX_X;
                if (pos_identical(prev, start))
                    return cur;
                cur = prev;
            }
        }

        for (direction dir = GD_NONE + 1; dir < GD_MAX; dir++)
        {
            if (dir == GD_CURR)
                continue;
            position n = pos_move(cur, dir);
            if (!pos_valid(n) || done[Y(n)][X(n)] || !walkable(p, n))
                continue;
            int d = best + step_cost(p, n);
            if (d < dist[Y(n)][X(n)])
            {
                dist[Y(n)][X(n)] = d;
                parent[Y(n)][X(n)] = (gint16)(Y(cur) * MAP_MAX_X + X(cur));
            }
        }
    }
}

/* nothing reachable: is a monster in the way? */
static bool blocked_by_monster(player *p, int m)
{
    through_monsters = true;
    bool found = pos_valid(first_step(p, m));
    through_monsters = false;
    return found;
}

static void stop(const char *msg)
{
    mode = 0;
    if (msg != NULL)
        log_add_entry(nlarn->log, "%s", msg);
}

/* stop on a visible threat and name it; true if stopped */
static bool stop_on_monster(player *p)
{
    GList *threats = player_visible_threats(p, false);
    if (threats == NULL)
        return false;

    monster *m = threats->data;
    log_add_entry(nlarn->log, _("You see %s."),
                  monster_get_name_art(m, ART_INDEF, GC_ACC, false));
    display_paint_screen(p);
    display_flash_monsters(p, threats);
    g_list_free(threats);
    mode = 0;
    return true;
}

void explore_start(player *p, int m)
{
    mode = 0;
    stepped = false;

    if (player_effect(p, ET_BLINDNESS) || player_effect(p, ET_CONFUSION))
    {
        log_add_entry(nlarn->log, _("You are in no state to walk on your own."));
        return;
    }

    if (stop_on_monster(p))
        return;

    if (!pos_valid(first_step(p, m)))
    {
        if (blocked_by_monster(p, m))
            log_add_entry(nlarn->log, _("Something is in the way."));
        else if (m == 'X')
            log_add_entry(nlarn->log, _("There is nothing left to explore here."));
        else if (m == '>')
            log_add_entry(nlarn->log, _("You know of no way down on this level."));
        else
            log_add_entry(nlarn->log, _("You know of no way up on this level."));
        return;
    }

    mode = m;
}

bool explore_stairs_here(player *p, bool down)
{
    map *pmap = game_map(nlarn, Z(p->pos));
    sobject_t so = map_sobject_at(pmap, p->pos);

    switch (so)
    {
    case LS_STAIRSDOWN: case LS_ELEVATORDOWN: case LS_CAVERNS_ENTRY:
    case LS_STAIRSUP:   case LS_ELEVATORUP:   case LS_CAVERNS_EXIT:
        return true;    /* the game answers, also for the wrong direction */
    case LS_HOME:   case LS_DNDSTORE: case LS_TRADEPOST: case LS_LRS:
    case LS_SCHOOL: case LS_BANK:     case LS_BANK2:     case LS_MONASTERY:
        return down;
    default:
        break;
    }

    /* standing on a known trap door / teleport trap: '>' uses it */
    if (down && player_memory_of(p, p->pos).trap != TT_NONE)
    {
        trap_t t = map_trap_at(pmap, p->pos);
        if (t == TT_TRAPDOOR || t == TT_TELEPORT)
            return true;
    }

    return false;
}

int explore_step(player *p)
{
    int ch;

    /* any key stops (it is used up); the wait also paints the step */
    wtimeout(stdscr, 40);
    ch = wgetch(stdscr);
    wtimeout(stdscr, -1);
    if (ch != ERR)
    {
        if (ch == KEY_MOUSE)
        {
            MEVENT ev;
            getmouse(&ev);
        }
        stop(NULL);
        return 0;
    }

    position next = first_step(p, mode);
    if (!pos_valid(next))
    {
        if (blocked_by_monster(p, mode))
            stop(_("Something is in the way."));
        else
            stop(mode == 'X' ? _("There is nothing left to explore here.") : NULL);
        return 0;
    }

    stepped = true;
    step_z = Z(p->pos);
    step_from = p->pos;
    step_door = (map_sobject_at(game_map(nlarn, Z(next)), next) == LS_CLOSEDDOOR);
    g_free(log_pending);
    log_pending = g_strdup(nlarn->log->buffer->str);
    if (quiet != NULL)
    {
        g_ptr_array_free(quiet, true);
        quiet = NULL;
    }
    log_last = log_length(nlarn->log)
        ? log_get_entry(nlarn->log, log_length(nlarn->log) - 1) : NULL;

    return player_move(p, pos_dir(p->pos, next), true);
}

void explore_monster_logged(monster *m, gsize from)
{
    if (!mode || !stepped || from >= nlarn->log->buffer->len)
        return;

    /* only peaceful monsters' doings are harmless: townspeople and
       servants (MA_CIVILIAN, MA_SERVE) */
    if (!monster_is_friendly(m))
        return;

    if (quiet == NULL)
        quiet = g_ptr_array_new_with_free_func(g_free);

    const char *said = nlarn->log->buffer->str + from;
    while (*said == ' ')
        said++;
    if (*said)
        g_ptr_array_add(quiet, g_strdup(said));
}

/* strip every occurrence of cut from s */
static void strip_all(gchar *s, const char *cut)
{
    gchar *hit;
    size_t len = strlen(cut);
    if (len == 0)
        return;
    while ((hit = strstr(s, cut)) != NULL)
        memmove(hit, hit + len, strlen(hit + len) + 1);
}

/* messages the walker causes itself, or peaceful monsters' talk: they
   need no attention */
static bool benign(const char *msg)
{
    gchar *s = g_strdup(msg);
    for (guint i = 0; quiet != NULL && i < quiet->len; i++)
        strip_all(s, g_ptr_array_index(quiet, i));
    gchar *see_door = g_strdup_printf(_("You see %s here."),
            noun_phrase(so_get_desc_raw(LS_OPENDOOR), ART_NONE, GC_ACC, false, false));
    const char *drop[] = { _("You open the door."), see_door };

    for (size_t i = 0; i < G_N_ELEMENTS(drop); i++)
        strip_all(s, drop[i]);

    bool empty = (*g_strstrip(s) == '\0');
    g_free(see_door);
    g_free(s);
    return empty;
}

static bool new_message(void)
{
    message_log *log = nlarn->log;
    guint len = log_length(log);

    /* find the entries added since the step (the log drops old ones) */
    guint first = 0;
    if (log_last != NULL)
    {
        first = len;
        for (guint i = len; i > 0; i--)
        {
            if (log_get_entry(log, i - 1) == log_last)
            {
                first = i;
                break;
            }
        }
    }

    /* text logged before the step but flushed during it is not new */
    size_t skip = log_pending ? strlen(log_pending) : 0;

    for (guint i = first; i < len; i++)
    {
        const char *msg = log_get_entry(log, i)->message;
        if (skip && g_str_has_prefix(msg, log_pending))
            msg += skip;
        skip = 0;
        if (!benign(msg))
            return true;
    }

    const char *buf = log->buffer->str;
    if (skip && g_str_has_prefix(buf, log_pending))
        buf += skip;
    if (*buf && !benign(buf))
        return true;

    return false;
}

void explore_after_turn(player *p, bool no_move, bool was_attacked)
{
    if (!mode || !stepped)
        return;
    stepped = false;

    if (Z(p->pos) != step_z || was_attacked || no_move
            || (pos_identical(p->pos, step_from) && !step_door))
    {
        mode = 0;
        return;
    }

    explore_visit(p);

    if (stop_on_monster(p))
        return;

    if (new_message())
    {
        mode = 0;
        return;
    }

    /* the stair walk ends on the stairs: press the key again to take them */
    if (mode != 'X' && stair_target(map_sobject_at(game_map(nlarn, Z(p->pos)), p->pos), mode))
        mode = 0;
}
