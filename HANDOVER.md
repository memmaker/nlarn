# NLarn RVIP handover

## Next steps (orchestrator handover, 2026-09-28)

Where we stand: stages 1-6 of `RVIP.md` are done, committed and pushed on
`memmaker/nlarn` `master` (upstream base nlarn/nlarn `master` @ 8851b1f; our
commits 696e1c5..74dca32). The finetuning items (now in the `RVIP.md` stage checklists) are done in stage 5
(DawnLike items do not apply: Amiga set only, like larn/ularn). Lessons are
in the "R-NLarn" section of `RVIP.md` in memmaker/rvip, branch
`claude/beautiful-heisenberg-g88r1w` (not merged into rvip `main` yet).
**Nothing is deployed:** the cloud container has no ssh and cannot reach
ruzzoli.de.

Stages 7 (publish) and 8 (shrine) are done too (see "RVIP progress"). Stage 9 is done too; nothing remains of the RVIP stages. Old plan kept for reference:

1. **Stage 9 - Graveyard + leaderboard** (step 12): `js_beacon` from NLarn's
   own run-end code (death/win/quit; `player_die()` / the scoreboard entry in
   `src/player.c` / `src/scoreboard.c`), via `RvipWM.report`; `g=nlarn`;
   score = NLarn's own score; killer art: add nlarn to
   `roguelikes-index/killers/make.py` (Amiga tiles via `port/tilemap.h`).
   Test that the win path is reached (golden rule). Wizard mode (Enter menu ->
   "activate wizard mode", `-` = level down) helps; runs in wizard mode are not
   scored by NLarn, so they must not be reported either.

**On the Mac, after the stages** (cloud cannot do these):
```
cd ~/Games/rvip-tools && git fetch && git merge origin/claude/beautiful-heisenberg-g88r1w   # RVIP.md R-NLarn, rvip-sound.js .mp3 fix
cd ~/Games/roguelikes-index && git checkout main && git pull && git fetch origin \
  && git merge origin/claude/beautiful-heisenberg-g88r1w && ./order.py && git push origin main   # NLarn card, tree, years.json, img/nlarn.png
git clone https://github.com/memmaker/nlarn ~/Games/nlarn   # or git pull
cd ~/Games/nlarn && sh web/build.sh && sh web/deploy.sh      # build on the Mac ships the town music
cd ~/Games/roguelikes-index && ./deploy.sh                   # card, tree, shrine (Info, ✦), shared rvip-*.js
git -C ~/Games/nlarn push origin --delete claude/beautiful-heisenberg-g88r1w   # stale WIP branch (proxy refused the delete)
```
Then the checks from the "Deploy" block below, `curl -s
https://ruzzoli.de/roguelikes/nlarn/ | grep og:image`, and the W10 layout
check in a real browser (headless runs miss layout problems).

Open problems carried forward:
- Translations (de/es/fr/pt) are not shipped (English only).
- Remembered item piles show the item type's generic tile after a reload.
- In item prompts there is no inventory/equipment/floor switch (NLarn
  prompts show one list); Shift+letter on an undroppable item is silent.
- Music ships only from a Mac build (`new_town.ogg` from ~/Projects).
- Upstream bug: a SIGHUP before the name is entered saves a nameless game
  that crashes `player_deserialize` on load (native only).

## RVIP progress

**Stage 1 (get + build): done.** **Stage 2 (explore + stairs): done.**
**Stage 3 (Enter menu + inventory): done.** **Stage 4 (tiles): done.**
**Stage 5 (web page, windows, finetuning): done.** **Stage 6 (docs + sound): done.**
**Stage 7 (publish): done.** **Stage 8 (shrine): done.** **Stage 9 (graveyard + leaderboard): done.** All RVIP stages done (2026-09-29; card, tree, shrine and game deployed).

Stage 9 facts: `be_run_end(score)` (`port/be_web.c`, declared in `inc/display.h`) is called in
`player_die()` (`src/player.c`) right after `score_new()`, i.e. only in the non-wizard branch, so
wizard runs are never reported (NLarn does not score them either). One place covers death, win
(`PD_WON`, `buildings.c`), quit (`^Q`) and the time-limit losses (`PD_TOO_LATE`/`PD_LOST` -> `ev=death`
without killer). Save-and-exit (`^S`) does not pass `player_die()`: nothing sent. Fields: g=nlarn, ev,
name (`score->player_name`), killer (`monster_type_name(cause)`, only for `PD_MONSTER`; traps, map,
spells, effects send no killer), depth (`Z(pos)`: 0 town, 1-10 caverns, 11-13 volcano), score
(NLarn's own `player_calc_score`), turns (`game_turn`), lvl. Killer art: `nlarn()` in
`roguelikes-index/killers/make.py` (65 PNGs from `web/tiles.png` via `port/tilemap.h` `mon_tile`).
Tested live: quit through the Enter menu (`^Q` entry) -> `g=nlarn&ev=quit&name=Beacontest&depth=0&score=58&turns=1&lvl=1`.
Death and win not played live; both run the same line as quit (no early return for them).

Stage 8 facts: `roguelikes-index/shrine/nlarn.html` (11 sections, hand-written `<!--og-->`
block, card image, card `<p>` as description) + `shrine/nlarn/` (`manual.html` = upstream
`lib/nlarn.hlp` at 8851b1f converted (markers -> `<b>`/`<em>`), `changelog.txt`, `license.txt`
GPL-3). Linked from the card (Info), the tree (✦) and `web/index.html` `#bar h1` (the
`#bar h1 a` rule was already there). No walkthrough exists; the project site has a
spoilers guide (linked). Trivia sources: nlarn.github.io pages (about, news 0.3, 0.6 ARRP,
moved-to-github, 256 colours, 0.8), verified from the site's source repo
`github.com/nlarn/nlarn.github.io` (git clone works; the site, RogueBasin, web.archive.org
and blog.roguetemple.com are blocked by the cloud proxy), plus the change log and code at
8851b1f. Wizard mode works in the web build via the Enter menu (tested headlessly:
confirm, `-` went Town -> D1); Ctrl+W itself closes the tab in browsers. 375 px: no overflow.

Stage 7 facts: README top = upstream (nlarn/nlarn @ 8851b1f6420c17afc122a47e0b1b9e2b7b251878,
NLarn 0.8.1) + compare view + "Web port" section. `<!--og-->` block in `web/index.html`
by hand (og.py's second loop; the old description meta is dropped, the og block has one).
Selection page (memmaker/roguelikes, branch `claude/beautiful-heisenberg-g88r1w`, NOT on
`main` and NOT deployed): card `img/nlarn.png` (65 NLarn monster tiles, shuffled, 24x5 at 2x
= 384x160), tag "Larn variant · 2009", `Based on NLarn 0.8.1 · nlarn/nlarn @ 8851b1f`,
no Info button yet (stage 8 adds it with the shrine); `years.json` `nlarn` = 2009; count
40 -> 41. `order.py` sorts cards by year, so the card sits between ToME 2 (2008) and
TinyAngband (2009), not next to Larn/uLarn. Tree: `<li class="insp">` under Larn (sibling of
RL_M and uLarn), "2009 · Joachim de Groot; with Johanna Ploog", why: new GPL C code
modelled on Larn 12.3.
Sources: year = `Changelog.md` "Release 0.3.0 (2009-07-13) First publicly available
version of nlarn" (linked at the base commit); SourceForge project registered 2009-03-09
(search snippet); git history starts 2009-06-19. Parent: README "Heritage" (modelled after
Larn 12.3, a few Ularn additions), GitHub description "A rewrite of Noah Morgan's
classic roguelike game Larn (1986)", every source file (C) 2009- Joachim de Groot, GPL-3
(no Larn code) -> inspired, not derived. Co-maintainer: Johanna Ploog (235 commits,
`git shortlog -sn`). RogueBasin, Wikipedia, nlarn.github.io and larn.org are blocked
by the cloud proxy: not checked directly (web search snippets agree: 2009, de Groot,
Larn 12.3). No disagreements found.
Open: the index page is 376 px wide at 375 px (1 px horizontal overflow, also without
the NLarn card: pre-existing).

**Deploy (on the Mac, nothing was deployed from the cloud):**
```
cd ~/Games/nlarn && git pull && sh web/build.sh && sh web/deploy.sh
curl -sI https://ruzzoli.de/roguelikes/nlarn/ | head -1          # 200
curl -s https://ruzzoli.de/roguelikes/nlarn/ | diff - web/dist/index.html && echo same
curl -sI https://ruzzoli.de/roguelikes/nlarn/nlarn-core.wasm | grep -i content-type   # application/wasm
```
(`deploy.sh` refuses to run from an unclean or unpushed tree. The page loads
`../rvip-wm.js`, `../rvip-app.js` and `../fonts/`: run `roguelikes-index/deploy.sh`
too if rvip-tools changed since its last deploy.)

Stage 6 facts:
- **Sound events:** `SOUND("event")` (`inc/display.h`, no-op without `__EMSCRIPTEN__`,
  web: `be_sound()` in `port/be_web.c` -> `Module.nl.sound`). Hooked at the game action:
  hit / miss (`player_attack`), kill (`monster_die` with a player), mon_hit / breathe
  (`monster_player_attack`), death (`player_die`, real deaths only), level
  (`player_level_gain`), spell (`spell_cast`), quaff, study (scroll read), pickup / money1,
  drop, wield / wear, opendoor / shutdoor, stairs_up / stairs_down, store_enter / store_home
  (`player_building_enter`), store5 (`building_player_charge`), shoot / shoot_hit (bows,
  thrown weapons). 24 events.
- **Samples:** Dubtrain Angband Sound Pack (CC BY 4.0) mp3s from upstream Angband,
  vendored in `web/sound/` (52 files, 645 KB, `sounds.json`, `README` credit), made by
  `python3 web/sounds.py [angband-checkout]` (reads `lib/customize/sound.prf`, asserts a
  sample for every `SOUND()` event in `src/*.c`; melee miss = `plc_miss_swish`, pickup =
  `plm_chest_latch`). `build.sh` copies `web/sound` -> `dist/sound`. JS picks a random file
  per event and plays it with `../rvip-sound.js` (rvip-tools: names with an extension are
  now used as is, so `.mp3` works). Nothing is fetched while Sound effects is off.
- **Music:** as larn/ularn, `new_town.ogg` looped in town (level 0). `build.sh` copies it
  only if `~/Projects/heavenAndHell/files/mods/heavenandhell/music/new_town.ogg` exists (the
  Mac); the cloud build has none, so switching Music on greys the checkbox out ("No music
  in this build"). **Rebuild on the Mac to ship the music.**
- **Deploy:** `rvip-sound.js` changed (rvip-tools 0cf7db7): run `roguelikes-index/deploy.sh` too.
- Audio off by default, stored in `web-layout.json` (`audio.sound/music`). Favicon `data:`.
- **Help** (`web/make-help.py`): essentials, full key list from `lib/nlarn.hlp`, keys to
  remember (`?`, `X`, Enter, `i`, `<`/`>`, `^S`), saving, tips, new-player guide, In the
  browser (incl. audio), Credits (Joachim de Groot per source headers/debian/copyright,
  Johanna Ploog from git history, GPL-3+, Larn/ULarn heritage, Amiga tiles MIT, Dubtrain
  CC BY 4.0, int10h fonts), About this version.
- Tested headless: sound off -> 0 requests to `sound/`; real clicks Audio ▾ -> Sound effects
  -> `sound/sounds.json`, `plm_floor_creak.mp3`, `mco_hit_whip.mp3`, `plm_floor_creak2.mp3`,
  `plc_die_laugh.mp3`, `plm_chest_latch.mp3` requested; events stairs_down/up, mon_hit,
  pickup, death seen; layout stores `{"sound":true,"music":false}`. Music click in the cloud
  build -> checkbox unchecked + disabled. No console errors.

Stage 5 facts:
- **Web page files:** `web/index.html` (top bar `Help · File ▾ | Windows ▾ · Tiles ·
  Font · Audio ▾` + key hints `X` explore · `i` inventory · `Enter` command menu ·
  `?` help; windows `#t-map`, `#t-msg`, `#t-stat`, `#t-inv`, `#t-vis`, pop-up `#pop`,
  help panel), `web/nlarn.js` (draws the panes, input, WM,
  RvipApp), `web/make-help.py` (cloud variant, self-contained: key list parsed from
  `lib/nlarn.hlp`, version from `inc/nlarn.h`; uses the Mac Docs entry `nlarn.html`
  if one exists), `web/deploy.sh` (guard line from roguelikes-index), `web/build.sh`
  (also writes `fonts.json` from `~/Games/roguelikes-index/fonts` and `help.html`).
  Loads `../rvip-wm.js`, `../rvip-app.js` (no copies in the repo).
- **Pane ids** (`port/be.h`): `P_MAP` 0 (67x17), `P_STATUS` 1 (46x23, trimmed),
  `P_MSG` 2 (90x200 history), `P_INV` 3 (64x120), `P_POP` 4 (bounding box of the
  shown panels). Visible is a string (`be_vis`), not a grid.
- **Routing** (`port/wcurses.c` `doupdate()`): stdscr cells go through `route()` by
  region: rows 0-16 x cols 0-66 -> `P_MAP` (with the tile ids from `tiles_paint()`);
  the right column (cols 68..) -> `P_STATUS` rows 6..; the two lines under the map
  are cut in their three segments (name/level at col 0, HP MP/XP at col 46, T/Lvl at
  col 68) -> `P_STATUS` rows 0-5; the border and stdscr's message rows (20..) are not
  sent. Windows registered with `wc_pane(win, pane)` (not panels) are panes of their
  own: `display_web_panes()` in `src/display.c` (`#ifdef __EMSCRIPTEN__`, end of
  `display_paint_screen()`) draws the **Messages** history (log entries oldest first,
  newest at the bottom, same colours/tags as the screen, 200 rows) and the
  **Inventory** (sorted and grouped like the `i` list, letters a.., colour =
  `item_colour()`, equipped bold with ` *`, icon per row with `wc_rowtile()` =
  `tiles_item()`; text mode `a) ! name`). Every visible **panel** is composed into
  `P_POP` (its screen origin goes to the page for mouse clicks). All text panes are
  trimmed by the shim (`be_extent`, used cols/rows). The cursor goes to `P_POP` (text
  entry) or `P_MAP` (targeting), never drawn on the hero. **Visible**: `tiles_visible()`
  (`port/tiles.c`): monsters the map shows (nearest first) + every item in view, with
  glyph, name, game colour (`wc_rgb()`), tile. `be_hero(y, x, level)` from
  `tiles_paint()` -> `RvipWM.center`. Prompt line: `be_prompt()` = this turn's log
  text (tags stripped) -> `RvipWM.prompt.text`; `web_at_cmd` (set in `mainloop()`
  around the command read) -> `RvipWM.prompt.wait`.
- **Saves:** IDBFS at `RvipApp.dir` (`/nlarn`), `-D /nlarn`; `nlarn.sav`, `nlarn.ini`,
  scores, `web-layout.json` (layout, WM sizes/titles, fonts, audio switches).
  **Autosave**: JS sets a flag (2 min, tab hidden, Export); `be_getkey()` runs
  `game_save()` only while `web_at_cmd`, with `web_autosaving` set so `game_save()`
  skips its "Saving...." pop-up (screen untouched: tested by counting lit map pixels
  and the save's mtime). Export/Import/New game via RvipApp.
- **Game end:** death and `^Q` -> NLarn's own flow (The End screen, scores, memorial
  question, back to its main menu = new game). `^S` (save and exit) and `q`/Esc in the
  main menu return from `main()` -> `be_end()` -> the page syncs IndexedDB and reloads
  by itself (no dead page, no overlay); with a save the main menu offers "Continue
  saved Game". A death also syncs IDBFS right after the `setjmp` return. Player
  name: NLarn asks itself (kept).
- **Layout:** multi = map | Status / Inventory / Visible, Messages under the map; one
  window = Messages on top, map with the Status column on its right. The map's cell
  height fits the whole map into the Map window unless A−/A+ on its title bar chose a
  zoom (kept per window mode, `L.mt`); `web-layout.json` holds splits, WM state, zoom,
  fonts, audio.
- **Tested headless** (Chromium, 1440x900, persistent profile): new game, all
  windows filled (status, messages, inventory with icons, visible with icons),
  inventory/menus/questions in the pop-up, Enter menu, `X` explore in town and D1,
  `>` stairs, map zoom (A+ on the Map title bar; zoomed map keeps the hero centred /
  clamped), text A+, autosave, `^S` -> page reloads -> Continue -> same turn,
  `^Q` -> The End -> main menu -> `q` -> page reloads into a new game, layout (split
  drag, A+, map zoom) survives reload, shop (DND store via map click travel), Help, Tiles None, Windows menu
  (one window / multi), font chooser, resize 1000x650 -> 1440x900 -> 1200x750 (with a
  question open) -> 760x500, `^P` `^A` `^R` `?` windows. No console errors.
- **Finetuning done:** inventory icons (cols 2-4, 1:2 aspect, clipped) and
  `a) ! name` in text mode; visible icons; tile switch redraws both (^L at the
  prompt); messages fill from the top and follow the end; Tiles Amiga -> None
  (late onload guarded); autosave doesn't touch the screen; pop-ups via
  `RvipWM.popup`; no cursor on the hero; zoom on the Map title bar; A−/A+ owned by
  the WM (`zoom:` only for map and the canvas text panes; pop-up follows Messages);
  `../rvip-app.js` (`RvipApp.dir`/`mount`); both font choosers (`L.face`,
  `L.mapFace` on the Map title bar, text mode only); crash check (no signature
  mismatch warnings in the build); movement (explore paints each step, `<`/`>` only
  walk, Enter menu has no moves: stages 2/3); top bar order + key hints; Audio ▾
  checkboxes stored, off by default (sounds: stage 6, use the sound table then).
  **Not applicable:** DawnLike floors/animation (Amiga set only, like larn/ularn),
  the game's own `i` pop-up keeps NLarn's UI colours (NLarn has its own themed
  pop-up windows; the pane carries the item colours).

Stage 4 facts:
- **Tile set: larn.org's Amiga Larn tiles** (github.com/primeau/Larn `src/img`, MIT,
  Jason Primeau; licence in `port/amiga/LICENSE`), 8x16, incl. primeau's ULarn-mode
  art (`m39v`, `m57v`-`m64v` visible stalker/demons). Same arrangement as memmaker/larn
  and memmaker/ularn: Tiles button cycles **Amiga -> None** (text), choice in
  `localStorage['nlarn-tileset']`. No DawnLike (siblings ship none; one set).
- **Generator:** `python3 port/mktiles.py` (from the repo root, needs Pillow) reads
  the ids from the game's X-macro enums (`MONSTER_TYPE_ENUM`, `MAP_TILE_TYPE_ENUM`,
  `SOBJECT_TYPE_ENUM`, `TRAP_TYPE_ENUM`, `ITEM_TYPE_ENUM`, amulet/ammo/armour/
  container/gem/potion/ring/scroll/spell/weapon enums in `inc/*.h`) and asserts
  a tile for every id: 285/285 ids (100%), 204 sheet slots. Writes `web/tiles.png`
  (32 per row) and `port/tilemap.h` (arrays indexed by the enums). Stand-ins from
  the same set, some recoloured (`TINTS`): grass/dirt (floor dot green/brown),
  tree (o97 plant), water/deep water/lava (wall texture blue/orange), fire, gas
  cloud, mountains (wall autotile in grey), spiked pit, sleep gas / mana drain
  traps, town person (elf), cloak/gloves/boots/helmets/shields, bag/crate, short
  sword, sling/bow/crossbow, club, stones/bullets/arrows/bolts. One picture for all
  potions / scrolls / books (Larn has one). **Rings and amulets are flavoured:**
  tile by the game's random material (`ring_material_mapping`,
  `amulet_material_mapping`), so tiles never identify them. Walls and mountains
  autotile by orthogonal neighbours (real map; doors count as wall).
- **Loader / how C hands tile ids to JS:** `port/tiles.c` `tiles_paint(p)`, called
  at the end of `display_paint_screen()` (`src/display.c`, `#ifdef __EMSCRIPTEN__`),
  decides every map cell from game data with the text map's own rules (visible:
  sobject > shown item (gem > gold > top) > known trap > terrain; else player memory;
  monsters in sight / detected, mimics show their disguise's item tile; spheres;
  player = Amiga `player` block) and calls the shim's `wc_settile(y, x, tile)`.
  The shim stores the tile with the stdscr cell it was set for; `doupdate()` sends
  it only if the composed cell still comes from stdscr and has that char+attr
  (so pop-ups, animations, targeting stay text). `be_put(..., tile)` → 4th uint32
  per cell (`-1` = text; bit `0x8000` = remembered, drawn at 45 % alpha).
  Remembered item piles keep the last seen item's tile (runtime cache in tiles.c,
  not saved; after a reload they show the item type's generic tile).
- **Scale:** single canvas, cell = tile aspect (w = h/2); height the largest of
  16..48 px that fits the window (1440x900: 16x32 = 2x). Nearest-neighbour
  (`imageSmoothingEnabled = false`). Text cells use a font of 0.78 x cell height.
  Test hook: `window.nlTiles(y0, y1)` returns the tile ids per map row.
- Tested headless (Chromium 1440x900): town, D2/D6/V1 with wizard full view,
  inventory pop-up over the map (text), Tiles -> None -> reload keeps None. No errors.

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
  in the menu (`RVIP.md` stage 3). Skipped: duplicate keys (first wins: `<`/`>`
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

Open problems: the parking branch `claude/beautiful-heisenberg-g88r1w` (stage-5 WIP,
now merged into master as a single commit) is still on GitHub: the cloud git proxy
refused the delete (403); remove it on the Mac with `git push origin --delete
claude/beautiful-heisenberg-g88r1w`. Translations not shipped (English only, `g_get_language_names()` =
"C"). NLarn's own pop-up windows are sized for the 90x25 screen (the command menu
scrolls); the layout check on the Mac in a real browser is still to do (W10).

### W0 rule 6 (text windows as HTML) — 2026-09-28

- Status, Messages, Inventory and the pop-up are HTML lines from the shim (`port/wcurses.c`
  `send_text`: `be_line`, `be_rows`, `wc_rowattr`; `be_extent`/`be_rowtile`/`wc_rowtile` gone),
  as ularn 6229406. The map is the only canvas (`be_put` is map only); one `cursor()` helper.
- Runs: `"\x05#rrggbb"` (bold `"\x05*#rrggbb"`) … `"\x06"`, reverse video between `\x01`/`\x02`.
  NLarn extension: a background other than the pane's (menu/inventory highlight bars) is
  appended as `"/#rrggbb"`. The pop-up's own box colour (dark blue) is not sent: pop-ups
  show on black.
- Pop-up clicks: row = line of the `<pre>`, column = caret offset under the pointer.
- Inventory/Visible icons: CSS sprites sized in em (1.2em high), grow with A+.
- Native build unchanged (real ncurses; the web code is `#ifdef __EMSCRIPTEN__`).
