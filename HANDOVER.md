# NLarn: handover

RVIP import, all stages done and deployed (2026-09-29): https://ruzzoli.de/roguelikes/nlarn/,
shrine `roguelikes-index/shrine/nlarn.html`. Stages 1–6 ran in a cloud session.

## Source and build

- Fork of nlarn/nlarn `master` @ 8851b1f (NLarn 0.8.1, GPL-3), our commits on top; remote
  `origin` = github.com/memmaker/nlarn, branch `master` (no `main`).
- Case **R/O**: C99 on GLib + ncursesw/panelw, all drawing in `src/display.c`.
- `sh web/build.sh` → `web/dist` (`nlarn-core.{js,wasm,data}`, index.html, nlarn.js, help,
  fonts.json, sounds). `ASAN=1 sh web/build.sh` = ASan variant. Data `lib/{fortune,maze,
  nlarn.hlp,nlarn.msg}` → `/nlarn-data/lib`.
- `sh web/deploy.sh` (refuses an unclean/unpushed tree). The page loads `../rvip-wm.js`,
  `../rvip-app.js`, `../rvip-sound.js`, `../fonts/`: run `roguelikes-index/deploy.sh` too if
  rvip-tools changed. Check: `curl -sI https://ruzzoli.de/roguelikes/nlarn/nlarn-core.wasm`
  → `application/wasm`.
- **Music ships only from a Mac build**: `build.sh` copies `new_town.ogg` from
  `~/Projects/heavenAndHell/...` if present; without it the Music checkbox greys out.
- Native build (upstream `Makefile`, real GLib/ncurses) is unchanged; all web code is
  `#ifdef __EMSCRIPTEN__`. Test it through a pty with `TERM=screen-256color` (pyte ignores
  xterm-256color's REP sequence).

## Port

- GLib: own subset shim `port/glib/` (~1500 lines, 146 APIs; NLarn uses no GObject/GIO).
- Curses: `port/curses.h`, `port/panel.h`, `port/wcurses.c` (in-memory, wide chars, panels,
  mouse). `doupdate()` routes stdscr regions by `route()`: map rows 0-16 × cols 0-66 → `P_MAP`
  (canvas, tile ids from `tiles_paint()`); status column + the two lines under the map →
  `P_STATUS`; windows registered with `wc_pane()` and visible panels → `P_POP`. Pane ids in
  `port/be.h`. Messages and Inventory panes are drawn by `display_web_panes()`
  (`src/display.c`); Visible by `tiles_visible()` (`port/tiles.c`).
- Text windows are HTML lines (`send_text`: `be_line`, `be_rows`, `wc_rowattr`). Runs:
  `"\x05#rrggbb"` (bold `"\x05*#rrggbb"`) … `"\x06"`, reverse between `\x01`/`\x02`, a
  non-pane background appended as `"/#rrggbb"`; pop-up background via `be_popbg`. Only the
  map is a canvas (`be_put`).
- Prompt line: `be_prompt()` = this turn's log text; `web_at_cmd` (set in `mainloop()`
  around the command read) = prompt wait flag.
- Explore `X` (`x` is weapon swap), `<`/`>` walk to known stairs (never to volcanic shafts):
  `src/explore.c`, hooks in `mainloop()`. Town chatter is ignored by explore
  (`explore_monster_logged`). NLarn has no `--more--`.
- Enter menu: `command_menu()` (`src/nlarn.c`), parsed at run time from `lib/nlarn.hlp`.
  Menu widget `display_key_menu()` (`src/display.c`).
- Inventory: extended `display_inventory()`; item actions call the callbacks directly
  (`display_inv_callback.primary` = main action); `i`/`e` via `player_inv_display_list()`.
- Tiles: larn.org Amiga tiles (MIT, `port/amiga/LICENSE`), `python3 port/mktiles.py` →
  `web/tiles.png` + `port/tilemap.h` (285/285 ids, asserted; stand-ins recoloured via
  `TINTS`; rings/amulets by random material). Tiles button: Amiga → None, stored in the
  layout file. Test hook `window.nlTiles(y0, y1)`.
- Saves: IDBFS at `/nlarn` (`-D /nlarn`): `nlarn.sav`, `nlarn.ini`, scores,
  `web-layout.json`. Autosave: JS flag (2 min, hidden, Export) → `be_getkey()` runs
  `game_save()` at the command prompt with `web_autosaving` (no "Saving...." pop-up).
  `^S` / `q` in the main menu → `be_end()` → page syncs and reloads.
- Sound: `SOUND("event")` (24 events, `inc/display.h`) → `be_sound()`; Dubtrain mp3s vendored
  in `web/sound/` by `python3 web/sounds.py [angband-checkout]`.
- Help: `web/make-help.py` (key list from `lib/nlarn.hlp`; uses the Docs entry `nlarn.html`
  if present).
- Beacon: `be_run_end(score)` in `player_die()` right after `score_new()` (non-wizard only):
  death, win, quit, time-limit losses. `^S` sends nothing. Killer only for `PD_MONSTER`.
  Killer art: `nlarn()` in `roguelikes-index/killers/make.py`. Death and win not played live.
- Upstream fix: `src/config.c` leaked the `colours` ini string.

## Open

- Translations (de/es/fr/pt) not shipped (English only).
- Remembered item piles show the item type's generic tile after a reload (cache not saved).
- Item prompts show one list: no inventory/equipment/floor switch; Shift+letter on an
  undroppable item is silent.
- Upstream bug: SIGHUP before the name is entered saves a nameless game that crashes
  `player_deserialize` on load (native only).
