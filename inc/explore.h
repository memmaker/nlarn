/*
 * explore.h
 * Auto-explore and walking to known stairs (RVIP port addition).
 *
 * NLarn is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef EXPLORE_H
#define EXPLORE_H

#include <stdbool.h>
#include "player.h"

/* forget every "stood here" mark (new game / restored game) */
void explore_reset(void);

/* note the cell the player stands on; call once per main loop pass */
void explore_visit(player *p);

/* start auto-explore ('X') or a walk to the nearest known stairs ('<', '>') */
void explore_start(player *p, int mode);

/* true while explore or a stair walk is running */
bool explore_active(void);

/* stop without a message */
void explore_cancel(void);

/* take one step; returns the moves used (0 = stopped) */
int explore_step(player *p);

/* after the turn: stop on a visible monster, a new message, a failed move,
   a level change or arrival */
void explore_after_turn(player *p, bool no_move, bool was_attacked);

/* the '<' / '>' command: take the stairs when on them, else walk to the
   nearest known staircase of that kind (never to a level-skipping shaft).
   Returns true when the original command should run. */
bool explore_stairs_here(player *p, bool down);

#endif
