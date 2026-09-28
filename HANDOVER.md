# NLarn RVIP handover

## RVIP progress

**Stage 1 (get + build): done.** **Stage 2 (explore + stairs): done.**
**Stage 3 (Enter menu + inventory): done.** Next: stage 4 (tiles).

Stage 3 facts:
- **Menu widget:** `display_key_menu(title, display_menu_item[], n, initial)` in
  `src/display.c` (decl. `inc/display.h`): content-sized floating list (key column +
  text, rows with key 0 = group headings, scrolls when taller than the screen),
  arrows / 8 / 2 / wheel / PgUp / PgDn move, Enter / Space / 5 / 6 / click choose,
  a row's own key chooses it, Esc / 4 / 0 cancel. Returns the row index or -1.
- **Enter menu (3b):** `command_menu()` in `src/nlarn.c`, parsed at run time from
  `lib/nlarn.hlp`: every line starting with `` `KEY`k`end` `` is a command, grouped
  under the help's `` `TITLE` `` headings (Auto-travel, Other actions, Control Keys,
  Wizard Mode Actions). Movement / running are pictures in the help, so they are not
  in the menu (Finetuning "Movement"). Skipped: duplicate keys (first wins: `<`/`>`
  from Auto-travel, `^D` = landmarks), `^U` (list paging), wizard keys except `^W`
  outside wizard mode. `mainloop()`: right after `ch = display_getch(NULL)`, Enter
  (LF/CR/KEY_ENTER) becomes `ch = command_menu()`, so the chosen key runs through
  the normal `switch` with its own prompts (`g` then asks for a direction).
  Help lines were shortened so the menu fits 90 columns (`<`/`>`), `e` added.
- **Inventory (3c):** extended `display_inventory()` (`src/display.c`), not replaced.
  Every list shows page-relative letters `a)`... (a = first item row shown). Lists
  with callbacks: letter = main action, Shift+letter = drop (player lists only),
  Ctrl+letter = examine (not ^I/^J/^M: Tab/Enter), Enter/Space/5/click on a row =
  item menu (`inv_item_menu()`: every fitting callback with its key + `x` examine,
  cursor on the main action), `+` `-` `*` = main/drop/examine on the cursor row,
  `0`/`.` close, `4`/`6`/left/right switch inventory <-> equipment, any other key
  closes the list and is `ungetch()`ed to run as a command. Caption: "(Enter)
  actions (?) help" (clickable). Width sized to content (`inv_content_width()`).
- **How item actions run: direct calls** of the callback functions
  (`cb->function(p, cb->inv, item)`), no key queue. Main action =
  first fitting callback with the new `display_inv_callback.primary` flag (player
  inventory: equip, open, unequip, use); in other lists (shops, floor, containers,
  home) the first fitting callback; none = examine (`inv_examine()`: item details
  in a message window). After an action the list reopens unless
  `player_visible_threats(p, false)` is non-empty.
- `i` / `e`: `player_inv_display_list(p, equipment)` in `src/player.c` (filter
  `player_inv_filter_equipped`); `player_inv_display(p)` kept as a wrapper. It sets
  the globals `display_inv_main` (switching, drop, pass-through; `display_inventory`
  clears it for nested lists) and reads `display_inv_switch`.
- Item prompts (`display_inventory` without callbacks, e.g. "Choose an item to
  drop"): letter / Enter / 5 / click choose, Ctrl+letter examines, `0`/`.` cancel.
  NLarn prompts show one list, so there is no inventory/equipment/floor switch there.
- **Town chatter fix:** `explore_monster_logged(m, from)` (`src/explore.c`) is called
  from `monsters.c` right after a townsperson's talk and "bumps into you"; when
  `monster_is_friendly(m)` (MA_CIVILIAN / MA_SERVE) the text the monster added to the
  log buffer is recorded and ignored by `new_message()` for that step. Tested with a
  scratch build that makes townsfolk talk every turn within 6 cells: `>` walks reach
  the caverns entrance through the chatter.
- Tested: native pty build (`TERM=screen-256color`: pyte ignores the REP sequence of
  xterm-256color, which left stale cells in scrolled lists) incl. ASan+UBSan, and
  headless Chromium on `web/dist` (Enter menu, click on a menu row, click on an item
  row -> item menu, click examine, menu `X`). No errors.

Stage 2 facts:
- Explore key **`X`** (`x` is NLarn's weapon swap; `e` kept free for the 3c
  equipment list). Code: `src/explore.c` + `inc/explore.h` (new), hooks in
  `src/nlarn.c` `mainloop()`: `explore_reset()` before the loop, `explore_visit()`
  after the repaint at the loop top, `else if (explore_active()) moves_count =
  explore_step()` between the travel branch and the run branch,
  `explore_after_turn(p, no_move, was_attacked)` after `player_update_fov()`;
  `case 'X'`, and `<`/`>` call `explore_stairs_here()` first.
- "Known grid" test: `player_memory_of(p,pos).type != LT_NONE` (the player's
  tile memory). Walkable = known, `mt_is_passable`, not fire/gas cloud, no
  remembered trap (`memory.trap`), sobject passable or a closed door (stepping
  in opens it via `player_move(..., true)`; NLarn has no locks), no visible
  monster. Targets: frontier cells (next to an unknown cell) and remembered
  items, until the player stood on them (`visited[z][y][x]`, not saved).
  Search = small Dijkstra, features (stairs, fountains...) cost 11 vs 10 so
  ties don't cross them (standing on one logs "You see ... here" = stop).
  Own search instead of `path_find()`: that one only penalises known traps
  and needs one goal; the travel loop stays unchanged.
- Stops: any key (40 ms `wtimeout` poll = the step's paint pause), visible
  threat (`player_visible_threats`, logged "You see a giant bat." + flash),
  any new log text except "You open the door." / "You see an open door
  here." (text pending before the step is ignored), failed move, level change,
  damage. Blocked only by a monster: "Something is in the way."
- `<`/`>`: on stairs/shaft/entrance/building (and `>` on a known trapdoor or
  teleport trap) the original command runs; else walk to the nearest
  remembered `LS_STAIRSDOWN`/`LS_CAVERNS_ENTRY` (`>`) or
  `LS_STAIRSUP`/`LS_CAVERNS_EXIT` (`<`) and stop on it; press again to take.
  **Volcanic shafts (`I`, `LS_ELEVATORDOWN/UP`) are never walked to** (they
  skip levels); used only when stood on. Town `>` walks to the caverns entrance `O`.
- 3d: NLarn has no `--more--` prompts (messages scroll in the message area),
  nothing to change.
- Help: `lib/nlarn.hlp` (Auto-travel section + `<`/`>` lines).
- Tested: native pty build (gcc + glib shim + ncurses, pyte) and headless
  Chromium on `web/dist`: town `>` walks to `O` (not the shaft), stops,
  `>` descends; `X` explores a whole maze level (D3), stops on items,
  monsters, traps, keys; `<` walks back to the exit and takes it on the
  second press; town `<` says "You know of no way up on this level.".

- Folder: `/home/user/nlarn` (Mac: `~/Games/nlarn`), fork of upstream nlarn/nlarn
  master @ 8851b1f (pristine base commit), our commits on top, branch `master` (the fork has no `main`).
- Case: **R/O** — C99 on GLib + ncursesw/panelw, all drawing in `src/display.c`
  (stdscr: map 67x17 at 0,0 with border, status column from x=69, 2 status rows +
  messages below row 18; pop-ups are curses panels, `display_window_new()`).
  Sibling reference: `memmaker/larn` `port/` + `web/`.
- Web frontend: `port/be_web.c` (EM_JS, Asyncify) + `web/index.html` + `web/nlarn.js`
  (stage-1 single canvas terminal 90x25, text shadow `window.nlText()` for tests).
- Curses: `port/curses.h`, `port/panel.h`, `port/wcurses.c` (in-memory, wide chars,
  panels, mouse). `doupdate()` composes stdscr + visible panels and sends changed
  cells through `route()` to `be_put(pane, …)` with RGB colours resolved in C
  (256-colour palette + NLarn's `init_color`). Stage 1 routes everything to
  `P_SCREEN`; for stage 5 split in `route()` (stdscr regions → map/status/messages
  panes, panel windows → `P_POP`), API in `port/be.h`.
- GLib: **own subset shim** `port/glib/` (glib.h/glib.c, glib/gi18n.h, glib/gstdio.h,
  ~1500 lines, 146 APIs incl. GKeyFile, GOptionContext, GHashTable, stable sorts).
  NLarn uses no GObject/GIO/main loop, so building real GLib was not needed.
  Verified natively too (gcc + real ncurses + shim, ASan).
- Build: `sh web/build.sh` → `web/dist` (`nlarn-core.{js,wasm,data}`, index.html,
  nlarn.js). `ASAN=1 sh web/build.sh` = ASan variant (asserts on). Flags:
  `-O2 -std=gnu99 -DG_DISABLE_ASSERT -Iport -Iport/glib -Iinc -Iinc/external
  -sUSE_ZLIB=1 -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=4MB
  -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sFORCE_FILESYSTEM -lidbfs.js
  -sENVIRONMENT=web`, data `lib/{fortune,maze,nlarn.hlp,nlarn.msg}` →
  `/nlarn-data/lib` (cwd; argv[0]'s dir + `/lib`). No linker warnings.
- Saves: IDBFS mounted at `/nlarn` (= RvipApp.dir for /roguelikes/nlarn/), passed
  as `-D /nlarn` (NLarn's own `--userdir`): `nlarn.sav`, `nlarn.ini`, highscores.
  `web_sync_files()` at the end of `game_save()` and after `main()`'s ini write
  (`#ifdef __EMSCRIPTEN__`), plus JS every 15 s / hidden / pagehide. Tested: save
  (Ctrl+S) → reload → "Continue saved Game" → same turn.
- Upstream edits (all small): `src/game.c`, `src/nlarn.c` (sync hooks, guarded),
  `src/config.c` (leak fix: `colours` string from the ini was never freed; found
  by LeakSanitizer).
- ASan: native (gcc ASan+UBSan, glib shim, real ncurses, driven through a pty) and
  wasm (`emcc -fsanitize=address` in headless Chromium: start, name, build, moves,
  inventory, help, mouse travel, save, reload, restore). Only finding: the leak
  above. ASan build and objects removed.
- Quirks: setjmp/longjmp (`nlarn_death_jump`) works with Asyncify (Ctrl+S path
  tested). Esc in the main menu quits = `main()` returns and the page stays on the
  last screen; Ctrl+S also ends `main()` — game end / reload handling is stage 5
  (W5). Keys: characters as code points, ctrl+letter 1-26, specials as curses
  KEY_* codes (web/nlarn.js `SPECIAL`); mouse = `BE_MOUSE|b<<16|y<<8|x`.
- Cloud: the emsdk zlib port download is blocked (github archive 403): seed
  `emsdk/upstream/emscripten/cache/ports/zlib/zlib-1.3.2` from a `git clone -b
  v1.3.2 https://github.com/madler/zlib` plus a `.emscripten_url` file holding the
  archive URL; on the Mac `-sUSE_ZLIB=1` just downloads.

Open problems: Game end (main returns) leaves a dead page; no pane routing,
tiles, help button or rvip-wm layout yet (stage 4/5); translations not shipped
(English only, `g_get_language_names()` = "C").
