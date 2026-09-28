# NLarn RVIP handover

## RVIP progress

**Stage 1 (get + build): done.** Next: stage 2 (explore + stairs).

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

Open problems: game end (main returns) leaves a dead page; no pane routing,
tiles, help button or rvip-wm layout yet (stage 4/5); translations not shipped
(English only, `g_get_language_names()` = "C").
