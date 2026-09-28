/*
 * NLarn in the browser: draws the panes the curses shim routes (port/be_web.c
 * calls Module.nl), keyboard and mouse input, tiling windows (../rvip-wm.js),
 * saves in IndexedDB (../rvip-app.js). Loaded before nlarn-core.js.
 * The game decides every cell (character, RGB colours, tile id, W0); this
 * file only blits them.
 */
(function () {
	'use strict';

	var P_MAP = 0, P_STATUS = 1, P_MSG = 2, P_INV = 3, P_POP = 4;
	var WIN = ['map', 'stat', 'msg', 'inv'];          /* pane -> window id */
	var DIR = RvipApp.dir;                            /* IDBFS: save, ini, scores, layout (NLarn's -D userdir) */
	var SAVE = DIR + '/nlarn.sav', LAYOUT_FILE = DIR + '/web-layout.json';
	var TW = 8, TH = 16, PER_ROW = 32, TILE_DIM = 0x8000;   /* Amiga Larn tiles in tiles.png */
	var MAP_COLS = 67, MAP_ROWS = 17, SIDE_COLS = 44;
	var FONT = '"DejaVu Sans Mono", Menlo, Consolas, "Liberation Mono", monospace';
	var GUT = 4, TITLE_H = 20, BORDER = 2;
	/* map cell height in px (cells are half as wide, the tiles' 8x16) */
	var TILE_STEPS = [16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96];
	var BE_MOUSE = 0x40000000;

	var panes = [];            /* {cv, ctx, cols, rows, vc, vr, cw, ch, cells, text, cy, cx} */
	var keys = [];
	var tiles = new Image(), tilesReady = false;
	var saveReq = false, app;
	var hero = { y: -1, x: -1, z: -1 };
	var pop = { y0: 0, x0: 0 };
	var dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
	var L = null, rects = {}, wm = null;

	/* Messages follows the newest line unless the player scrolled up to read back */
	var msgStick = true, byUser = false;
	document.addEventListener('keydown', function () { msgStick = true; byUser = false; }, true);

	function $(id) { return document.getElementById(id); }
	function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }
	function hex(v) { return '#' + ('00000' + (v & 0xffffff).toString(16)).slice(-6); }
	function status(msg, isError) { app.status(msg, isError); }

	/* ---------- panes ---------- */

	/* top-bar font: the text windows; the map (text mode) has its own */
	function face(p) { var n = L && (p === P_MAP ? L.mapFace : L.face); return n ? '"' + n + '", ' + FONT : FONT; }
	function measure(px, p) {
		var c = document.createElement('canvas').getContext('2d');
		c.font = px + 'px ' + face(p);
		return Math.ceil(c.measureText('M').width);
	}
	function textWin(p) { return p === P_POP ? 'msg' : WIN[p]; }   /* pop-up text follows Messages */

	/* cell size from the zoom settings; rebuilds the canvas and redraws */
	function shape(p) {
		var T = panes[p];
		if (!T) return;
		if (p === P_MAP) {
			T.cw = L.tile / 2; T.ch = L.tile;
			T.font = (L.mapFace ? '' : 'bold ') + Math.round(T.ch * 0.78) + 'px ' + face(p);
		} else {
			var f = RvipWM.fontSize(textWin(p));
			T.fs = f; T.cw = measure(f, p); T.ch = Math.round(f * 1.3);
			T.font = f + 'px ' + face(p);
		}
		/* text panes come trimmed from the game (be_extent) */
		if (p === P_MAP || p === P_POP) { T.vc = T.cols; T.vr = T.rows; }
		var w = T.vc * T.cw, h = T.vr * T.ch;
		T.cv.width = Math.round(w * dpr); T.cv.height = Math.round(h * dpr);
		T.w = w; T.h = h;
		T.ctx = T.cv.getContext('2d');
		T.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		T.ctx.imageSmoothingEnabled = false;                 /* nearest-neighbour tiles */
		T.ctx.textAlign = 'center'; T.ctx.textBaseline = 'middle';
		T.ctx.fillStyle = '#000'; T.ctx.fillRect(0, 0, w, h);
		for (var y = 0; y < T.vr; y++) for (var x = 0; x < T.vc; x++) cell(p, y, x);
		fit(p);
	}

	function makePane(p, cols, rows) {
		var cv = p === P_POP ? document.querySelector('#pop canvas') : document.querySelector('#t-' + WIN[p] + ' canvas');
		var old = panes[p];
		panes[p] = { cv: cv, cols: cols, rows: rows, vc: cols, vr: rows, cells: new Uint32Array(cols * rows * 4),
			text: [], cy: -1, cx: -1, icon: old && old.cols === cols ? old.icon : [] };
		for (var i = 0; i < cols * rows; i++) { panes[p].cells[i * 4] = 32; panes[p].cells[i * 4 + 3] = -1 >>> 0; }
		if (p !== P_MAP && p !== P_POP) { panes[p].vc = 1; panes[p].vr = 1; }
		shape(p);
	}

	function cell(p, y, x) {
		var T = panes[p], c = T.ctx, i = (y * T.cols + x) * 4;
		if (x >= T.vc || y >= T.vr) return;                 /* outside the trimmed canvas */
		var code = T.cells[i], fg = hex(T.cells[i + 1]), bga = T.cells[i + 2], t = T.cells[i + 3] | 0;
		var px = x * T.cw, py = y * T.ch;
		c.fillStyle = hex(bga);
		c.fillRect(px, py, T.cw, T.ch);
		if (p === P_MAP && t >= 0 && tilesReady) {
			var id = t & 0x7fff;
			c.fillStyle = '#000'; c.fillRect(px, py, T.cw, T.ch);
			if (t & TILE_DIM) c.globalAlpha = 0.45;
			c.drawImage(tiles, (id % PER_ROW) * TW, ((id / PER_ROW) | 0) * TH, TW, TH, px, py, T.cw, T.ch);
			c.globalAlpha = 1;
			drawCursor(p, y, x);
			return;
		}
		var ic = p === P_INV && T.icon ? T.icon[y] : -1;
		if (ic >= 0 && tilesReady && x >= 2 && x <= 4) {   /* the tile's 1:2 shape, centred on cols 2-4, clipped per cell */
			var ih = Math.min(T.ch, 6 * T.cw), iw = ih / 2, ix = 2 * T.cw + (3 * T.cw - iw) / 2;
			c.save(); c.beginPath(); c.rect(px, py, T.cw, T.ch); c.clip();
			c.drawImage(tiles, (ic % PER_ROW) * TW, ((ic / PER_ROW) | 0) * TH, TW, TH, ix, py + (T.ch - ih) / 2, iw, ih);
			c.restore();
			return;
		}
		var at = bga >>> 24;
		if (at & 2) { c.fillStyle = fg; c.fillRect(px, py + T.ch - 1, T.cw, 1); }
		if (code > 32) {
			c.font = (at & 1) && p !== P_MAP && !(L && L.face) ? 'bold ' + T.font : T.font;
			c.fillStyle = fg;
			c.fillText(String.fromCodePoint(code), px + T.cw / 2, py + T.ch / 2 + 1);
		}
		drawCursor(p, y, x);
	}

	/* the game's cursor (targeting on the map, text entry in a pop-up); none on the hero */
	function drawCursor(p, y, x) {
		var T = panes[p];
		if (y !== T.cy || x !== T.cx) return;
		if (p === P_MAP && y === hero.y && x === hero.x) return;
		var c = T.ctx, px = x * T.cw, py = y * T.ch;
		c.strokeStyle = c.fillStyle = '#e8e8e8';
		if (p === P_MAP && tilesReady && (T.cells[(y * T.cols + x) * 4 + 3] | 0) >= 0) { c.lineWidth = 1; c.strokeRect(px + 0.5, py + 0.5, T.cw - 1, T.ch - 1); }
		else c.fillRect(px, py + T.ch - 2, T.cw, 2);
	}

	/* ---------- tiling layout ---------- */
	/*
	 *   +-----------------------+-----------+
	 *   |          map          |  status   |
	 *   |                       +-----------+
	 *   +-----------------------+ inventory |
	 *   |       messages        +-----------+
	 *   |                       |  visible  |
	 *   +-----------------------+-----------+
	 */
	function areaSize() { var g = $('game'); return { w: g.clientWidth, h: g.clientHeight }; }

	function defaultLayout() {
		var A = areaSize(), W = A.w, H = A.h;
		if (W < 400 || H < 300) { W = 1280; H = 720; }
		var font = RvipWM.fontSize('stat'), tile = TILE_STEPS[0];
		var sideW = SIDE_COLS * measure(font) + BORDER + 4;
		TILE_STEPS.forEach(function (t) { if (MAP_COLS * t / 2 + BORDER <= W - sideW - GUT && MAP_ROWS * t + BORDER + TITLE_H <= H * 0.74) tile = t; });
		var mapH = MAP_ROWS * tile + BORDER + TITLE_H;
		return { v: 1, tile: tile, auto: true, mt: {},
			split: { bottom: (mapH + GUT / 2) / H, side: (W - sideW - GUT / 2) / W,
				stat: (24 * Math.round(font * 1.3) + TITLE_H + BORDER + GUT / 2) / H },
			audio: { sound: false, music: false } };
	}

	function loadLayout() {
		var d = defaultLayout();
		try {
			var s = JSON.parse(Module.FS.readFile(LAYOUT_FILE, { encoding: 'utf8' }));
			if (s && s.v === 1) {
				if (!s.auto) {
					d.auto = false;
					['bottom', 'stat', 'side'].forEach(function (k) { if (s.split && s.split[k] > 0 && s.split[k] < 1) d.split[k] = s.split[k]; });
				}
				/* map zoom per window mode (A-/A+ on the Map title bar); none = fit the window */
				if (s.mt) ['multi', 'single'].forEach(function (m) { if (TILE_STEPS.indexOf(s.mt[m]) >= 0) d.mt[m] = s.mt[m]; });
				if (s.wm) d.wm = s.wm;
				if (typeof s.face === 'string') d.face = s.face;
				if (typeof s.mapFace === 'string') d.mapFace = s.mapFace;
				if (typeof s.tiles === 'string') d.tiles = s.tiles;  /* the tile set, by name */
				if (s.audio) d.audio = { sound: s.audio.sound === true, music: s.audio.music === true };
			}
		} catch (err) { /* nothing saved yet */ }
		L = d;
		renderAudio();
		$('sel-font').value = L.face || '';
		loadFace(L.face); loadFace(L.mapFace);
	}

	var saveTimer = 0;
	function saveLayout() {
		clearTimeout(saveTimer);
		saveTimer = setTimeout(function () {
			try { Module.FS.writeFile(LAYOUT_FILE, JSON.stringify(L)); app.sync(); }
			catch (err) { console.warn('layout not saved', err); }
		}, 400);
	}

	/* text windows: shown 1:1 at the user's size, the window scrolls; the map
	 * never shrinks and keeps the hero centred; the pop-up scales down only
	 * when it is bigger than the room over the map */
	function fit(p) {
		var T = panes[p];
		if (!T || !T.w) return;
		if (p === P_MAP) { scrollMap(); return; }
		var sc = 1;
		if (p === P_POP) {
			var A = RvipWM.popupBox();
			if (A.w && A.h) sc = Math.min(1, (A.w - 2) / T.w, (A.h - 2) / T.h);
		}
		T.cv.style.width = T.w * sc + 'px';
		T.cv.style.height = T.h * sc + 'px';
		if (p === P_POP && !$('pop').hidden) RvipWM.popup($('pop'), { x: L.tile });
	}

	function scrollMap() {
		var T = panes[P_MAP];
		if (!T || hero.y < 0) return;
		T.cv.style.width = T.w + 'px'; T.cv.style.height = T.h + 'px';
		RvipWM.center(T.cv, (hero.x + 0.5) * T.cw, (hero.y + 0.5) * T.ch, T.w, T.h);
	}

	function applyDom() { if (wm) wm.apply(); }
	function makeWM() {
		var s = defaultLayout().split, A = areaSize();
		if (A.h < 300) A.h = 720;
		if (A.w < 400) A.w = 1280;
		var line = Math.round(RvipWM.fontSize('msg') * 1.3) * 2 + 6, stat = 22 * measure(RvipWM.fontSize('stat')) + BORDER + 4;
		wm = RvipWM({
			area: $('game'), menu: $('btn-layout'),
			wins: [{ id: 'map', title: 'Map' }, { id: 'msg', title: 'Messages' }, { id: 'stat', title: 'Status' }, { id: 'inv', title: 'Inventory' }, { id: 'vis', title: 'Visible' }],
			multi: { d: 'h', r: s.side, a: { d: 'v', r: s.bottom, a: 'map', b: 'msg' }, b: { d: 'v', r: s.stat, a: 'stat', b: { d: 'v', r: 0.6, a: 'inv', b: 'vis' } } },
			single: { d: 'v', r: line / A.h, a: 'msg', b: { d: 'h', r: 1 - stat / A.w, a: 'map', b: 'stat' } },
			state: L.wm,
			save: function (st) { L.wm = st; saveLayout(); },
			layout: function (r) { rects = r; L.tile = mapTile(); for (var p = 0; p < panes.length; p++) if (panes[p]) shape(p); },
			zoom: { map: function (size, d) { zoomMap(d); }, msg: zoomText, stat: zoomText, inv: zoomText },
			onReset: resetLayout
		});
		wm.apply();
		renderMapSel();
	}

	/* the map's cell height: the zoom chosen in this window mode, else the
	 * largest step that shows the whole map in the Map window */
	function mapTile() {
		var z = L.mt[wm ? wm.mode() : 'multi'];
		if (z) return z;
		var b = document.querySelector('#t-map .body'), W = b.clientWidth, H = b.clientHeight, t = TILE_STEPS[0];
		if (W < 50 || H < 50) return L.tile;
		TILE_STEPS.forEach(function (s) { if (MAP_COLS * s / 2 <= W && MAP_ROWS * s <= H) t = s; });
		return t;
	}
	function zoomMap(d) {
		var i = clamp(TILE_STEPS.indexOf(L.tile) + d, 0, TILE_STEPS.length - 1);
		L.tile = L.mt[wm.mode()] = TILE_STEPS[i];
		shape(P_MAP); saveLayout();
	}

	/* a text window's A− / A+: the WM keeps the size; redraw the panes at it */
	function zoomText() {
		for (var p = 1; p < panes.length; p++) if (panes[p] && RvipWM.fontSize(textWin(p)) !== panes[p].fs) shape(p);
	}

	function resetLayout() {
		var a = L.audio, fc = L.face, mf = L.mapFace, ts = L.tiles;
		L = defaultLayout(); L.audio = a; L.face = fc; L.mapFace = mf; L.tiles = ts; L.wm = wm.state ? wm.state() : L.wm;
		L.tile = mapTile();
		for (var p = 0; p < panes.length; p++) if (panes[p]) shape(p);
		applyDom(); saveLayout();
	}

	/* ---------- audio ---------- */
	/* sound events come from the game (SOUND() -> port/be_web.c), named like the Dubtrain
	 * Angband Sound Pack's (web/sounds.py picks the samples); rvip-sound.js plays them.
	 * Nothing is fetched until Sound effects is on. Music: the Larn siblings' town loop,
	 * only in builds that have music/new_town.ogg (web/build.sh). */
	var audio = { cfg: null, loading: false, town: false, el: null, played: 0, noMusic: false };
	function play(name) {
		if (!L || !L.audio.sound) return;
		if (!audio.cfg) {
			if (!audio.loading) {
				audio.loading = true;
				fetch('sound/sounds.json').then(function (r) { return r.json(); })
					.then(function (c) { audio.cfg = c; }).catch(function () { audio.loading = false; });
			}
			return;
		}
		var files = audio.cfg[name];
		if (!files || !files.length) return;
		audio.played++;                          /* testing */
		RVIPSound.play([files[Math.floor(Math.random() * files.length)]], 0.6);
	}
	function updateMusic() {
		var on = L && L.audio.music && audio.town && app.running && !audio.noMusic;
		if (on && !audio.el) {
			audio.el = new Audio('music/new_town.ogg');
			audio.el.loop = true; audio.el.volume = 0.4;
			audio.el.onerror = function () {        /* this build has no music */
				audio.noMusic = true; audio.el = null;
				L.audio.music = false; renderAudio(); saveLayout();
			};
		}
		if (!audio.el) return;
		if (on) audio.el.play().catch(function () { }); else audio.el.pause();
	}
	function toggleAudio(k) {
		L.audio[k] = !L.audio[k];
		if (k === 'sound' && L.audio.sound && !audio.cfg) play('');   /* load the event list */
		renderAudio(); updateMusic(); saveLayout();
	}
	function renderAudio() {
		var a = L ? L.audio : { sound: false, music: false };
		$('chk-sound').checked = a.sound;
		$('chk-music').checked = a.music && !audio.noMusic;
		$('chk-music').disabled = audio.noMusic;
		$('chk-music').parentNode.title = audio.noMusic ? 'No music in this build' : 'Music on or off (town)';
	}

	/* ---------- called by the game (port/be_web.c) ---------- */
	var nl = {
		init: function (p, cols, rows) {
			if (!L) loadLayout();
			makePane(p, cols, rows);
			if (!wm && panes[P_MAP] && panes[P_STATUS]) {
				$('game').hidden = false; status('');
				makeWM();
			}
		},
		draw: function (p, idx, c, y0, y1, cy, cx) {
			var T = panes[p];
			if (!T || c !== T.cols) return;
			var H = Module.HEAPU32, n = (y1 - y0) * c * 4, o = y0 * c * 4;
			var ocy = T.cy, ocx = T.cx;
			T.cells.set(H.subarray(idx + o, idx + o + n), o);
			T.cy = cy; T.cx = cx;
			for (var y = y0; y < y1; y++) {
				var line = '';
				for (var x = 0; x < c; x++) { line += String.fromCodePoint(T.cells[(y * c + x) * 4] || 32); cell(p, y, x); }
				T.text[y] = line.replace(/\s+$/, '');
			}
			if (ocy >= 0 && (ocy < y0 || ocy >= y1)) cell(p, ocy, ocx);
			if (p === P_MSG) {
				var mb = document.querySelector('#t-msg .body');
				if (mb && msgStick) mb.scrollTop = mb.scrollHeight;   /* the newest message stays in view */
			}
		},
		extent: function (p, c, r) {   /* the game's trimmed size of a text pane */
			var T = panes[p];
			if (!T || (T.vc === c && T.vr === r)) return;
			T.vc = c; T.vr = r; shape(p);
			if (p === P_MSG) { var mb = document.querySelector('#t-msg .body'); if (mb && msgStick) mb.scrollTop = mb.scrollHeight; }
		},
		popup: function (rows, cols, y0, x0) {
			if (!rows) { $('pop').hidden = true; panes[P_POP] = null; return; }
			pop.y0 = y0; pop.x0 = x0;
			makePane(P_POP, cols, rows);
			$('pop').hidden = false;
			fit(P_POP);
		},
		rowtile: function (p, y, t) {   /* the game's icon for an inventory row */
			var T = panes[p];
			if (!T) return;
			T.icon[y] = t;
			for (var x = 2; x <= 4 && x < T.cols; x++) cell(p, y, x);
		},
		icons: function () { return tilesReady ? 1 : 0; },
		hero: function (y, x, z) {
			if (y === hero.y && x === hero.x && z === hero.z) return;
			var oy = hero.y, ox = hero.x;
			hero.y = y; hero.x = x; hero.z = z;
			if ((z === 0) !== audio.town) { audio.town = z === 0; updateMusic(); }
			if (panes[P_MAP] && oy >= 0) cell(P_MAP, oy, ox);
			scrollMap();
		},
		prompt: function (s) { RvipWM.prompt.text(s); },
		vis: function (s) { RvipWM.visible(document.querySelector('#t-vis .body'), s, visIcon); },
		key: function (atCmd) {
			if (atCmd !== nl.atCmd) { RvipWM.prompt.wait(atCmd); nl.atCmd = atCmd; }
			return keys.length ? keys.shift() : -1;
		},
		requestSave: function () { saveReq = true; },   /* also for testing */
		wantSave: function () {
			if (!saveReq || !app.running) return 0;
			saveReq = false;
			return 1;                                    /* game_save() syncs when done */
		},
		sync: function () { app.sync(); },
		bell: function () { },
		sound: function (name) { play(name); },
		sounds: function () { return audio.played; },   /* testing */
		end: function () {                               /* main() returned: quit, or saved and quit */
			app.running = false; updateMusic();
			status(hasSave() ? 'Game saved. Restarting…' : 'Starting a new game…');
			app.sync(function () { setTimeout(function () { location.reload(); }, 800); });
		}
	};
	nl.atCmd = -1;

	/* ---------- input ---------- */
	/* characters as code points, ctrl+letter 1-26, special keys as curses KEY_* codes */
	var SPECIAL = {
		Enter: 13, Escape: 27, Tab: 9, Backspace: 263,
		ArrowDown: 258, ArrowUp: 259, ArrowLeft: 260, ArrowRight: 261,
		Home: 262, End: 360, PageUp: 339, PageDown: 338, Insert: 331, Delete: 330
	};
	function onKey(e) {
		if (!app.running || e.isComposing || e.metaKey) return;
		var tg = e.target && e.target.tagName;
		if (tg === 'INPUT' || tg === 'TEXTAREA' || tg === 'SELECT') return;
		var k = -1, m = /^Numpad(\d)$/.exec(e.code || '');
		if (m && !e.getModifierState('NumLock')) k = 48 + +m[1];
		else if (SPECIAL[e.key] !== undefined) k = SPECIAL[e.key];
		else if (/^F([1-9]|1[0-2])$/.test(e.key)) k = 264 + parseInt(e.key.slice(1), 10);
		else if (e.key.length === 1 || (e.key.length === 2 && e.key.codePointAt(0) > 0xffff)) {
			k = e.key.codePointAt(0);
			if (e.ctrlKey && !e.altKey) {
				if (/[a-z]/i.test(e.key)) k = e.key.toUpperCase().charCodeAt(0) & 0x1f;
				else return;
			}
		}
		if (k < 0) return;
		e.preventDefault();
		keys.push(k);
	}

	/* mouse: BE_MOUSE | button << 16 | y << 8 | x in screen cells (port/be.h);
	 * the map is the screen's top-left, the pop-up sits at its own origin */
	var down = null;
	function mouse(e, p, b) {
		var T = panes[p];
		if (!T || !app.running) return;
		var r = T.cv.getBoundingClientRect();
		var x = Math.floor((e.clientX - r.left) / r.width * T.cols), y = Math.floor((e.clientY - r.top) / r.height * T.rows);
		if (x < 0 || y < 0 || x >= T.cols || y >= T.rows) return;
		if (p === P_POP) { y += pop.y0; x += pop.x0; }
		keys.push(BE_MOUSE | b << 16 | y << 8 | x);
	}
	function wireMouse(sel, p) {
		var cv = document.querySelector(sel);
		cv.addEventListener('mousedown', function (e) {
			e.preventDefault();
			if (e.button === 0) { down = p; mouse(e, p, 1); } else if (e.button === 2) mouse(e, p, 3);
		});
		cv.addEventListener('mousemove', function (e) { if (down === p) mouse(e, p, 6); });
		cv.addEventListener('wheel', function (e) { e.preventDefault(); mouse(e, p, e.deltaY < 0 ? 4 : 5); }, { passive: false });
		cv.addEventListener('contextmenu', function (e) { e.preventDefault(); });
	}
	window.addEventListener('mouseup', function (e) {
		if (e.button === 0 && down !== null) { var p = down; down = null; mouse(e, p, 2); }
	});

	/* ---------- saves: IndexedDB (IDBFS), help, crashes: ../rvip-app.js ---------- */
	function hasSave() { try { Module.FS.stat(SAVE); return true; } catch (e) { return false; } }
	app = RvipApp({
		name: 'nlarn',
		save: function () { return hasSave() ? SAVE : null; },
		clear: function () { try { Module.FS.unlink(SAVE); } catch (e) { } },
		put: function (file, data) { Module.FS.writeFile(SAVE, data); },
		flush: function (done) { saveReq = true; setTimeout(done, 1500); },   /* the game saves at its next command prompt */
		noSave: 'There is no saved game yet (NLarn saves once a game has started).',
		helpText: 'Press ? in the game for its own help.'
	});

	/* ---------- startup ---------- */
	var tilesDone = false, tilesWait = false;
	window.Module = {
		nl: nl,
		arguments: ['-D', DIR],
		preRun: [function () {
			var FS = Module.FS;
			Module.addRunDependency('tiles'); tilesWait = true;   /* the sheet loads once the layout says which */
			FS.mkdirTree('/nlarn-data');
			FS.chdir('/nlarn-data');             /* the data (lib/) sits next to argv[0] */
			Module.addRunDependency('idbfs');
			RvipApp.mount(function (err) {
				if (err) status('Could not read saved games from IndexedDB (' + err + '). Saving may not work in this browser mode.', true);
				if (!L) loadLayout();            /* before the game: it holds the tile set */
				TILESETS.forEach(function (t, i) { if (t[1] === L.tiles) tileset = i; });
				startTiles();
				Module.removeRunDependency('idbfs');
			});
		}],
		onRuntimeInitialized: function () { app.running = true; },
		print: function (s) { console.log(s); },
		printErr: function (s) { console.warn(s); },
		setStatus: function (s) { if (s && !app.running) status(s.replace(/\(\d+\/\d+\)/, '').trim() || 'Loading…'); },
		onAbort: function (what) { app.crashed(what); }
	};

	/* tile sets: the Amiga tiles or none (text); the choice is kept by name in the
	   layout file (IndexedDB), never localStorage */
	var TILESETS = [['tiles.png', 'Amiga'], [null, 'None']], tileset = 0;
	function tilesFinished(ok) {
		tilesReady = ok && !!TILESETS[tileset][0]; tilesDone = true;   /* None picked meanwhile: stay text */
		if (!ok) status('Could not load the tile set; using text.', true);
		if (tilesWait) { tilesWait = false; Module.removeRunDependency('tiles'); }
	}
	tiles.onload = function () { tilesFinished(true); };
	tiles.onerror = function () { tilesFinished(false); };
	function renderTileset() { var b = $('btn-tiles'); if (b) b.textContent = 'Tiles: ' + TILESETS[tileset][1]; }
	function toggleTileset() {
		tileset = (tileset + 1) % TILESETS.length;
		L.tiles = TILESETS[tileset][1]; saveLayout();
		renderTileset();
		var redraw = function () {
			[P_MAP, P_INV].forEach(function (p) { if (panes[p]) shape(p); });
			var vb = document.querySelector('#t-vis .body');
			if (vb && vb._vis != null) { var s = vb._vis; vb._vis = null; nl.vis(s); }
			if (nl.atCmd === 1) keys.push(12);   /* ^L: the game repaints, the Inventory gets or drops its icons */
		};
		renderMapSel();
		if (!TILESETS[tileset][0]) { tilesReady = false; redraw(); return; }   /* text mode */
		tiles.onload = function () { if (TILESETS[tileset][0]) { tilesReady = true; redraw(); } };
		tiles.src = TILESETS[tileset][0];
	}
	/* map font chooser: on the Map title bar (shown on hover), text mode only */
	var mapSel = document.createElement('select');
	mapSel.title = 'Map font (text mode)';
	mapSel.innerHTML = '<option value="">Default font</option>';
	mapSel.addEventListener('pointerdown', function (e) { e.stopPropagation(); });   /* not a window drag */
	mapSel.addEventListener('mousedown', function (e) { e.stopPropagation(); });
	function renderMapSel() {
		var bs = document.querySelector('#t-map .wm-btns');
		if (bs && mapSel.parentNode !== bs) bs.insertBefore(mapSel, bs.firstChild);
		mapSel.hidden = !!TILESETS[tileset][0];
		mapSel.value = (L && L.mapFace) || '';
	}
	/* text font: a face from the index page's fonts/ (web/build.sh lists them) */
	function loadFace(n, now) {
		var redraw = function () {
			for (var p = 0; p < panes.length; p++) if (panes[p]) shape(p);
			document.querySelector('#t-vis .body').style.fontFamily = L.face ? '"' + L.face + '", monospace' : '';
		};
		if (!n) { if (now) redraw(); return; }
		var ff = new FontFace(n, 'url(../fonts/' + n + '.woff)');
		ff.load().then(function () { document.fonts.add(ff); redraw(); }).catch(function () { status('Could not load the font ' + n + '.', true); });
	}
	/* Visible window icon: the tile (8x16) as a CSS sprite */
	function visIcon(t) {
		if (!tilesReady || !(t >= 0)) return null;
		var s = document.createElement('i');
		s.className = 'wm-ic';
		s.style.cssText = 'width:8px;margin:0 4px;image-rendering:pixelated;background:url(' + tiles.src + ') -' + (t % PER_ROW) * TW + 'px -' + ((t / PER_ROW) | 0) * TH + 'px';
		return s;
	}
	function startTiles() {
		renderTileset();
		if (TILESETS[tileset][0]) tiles.src = TILESETS[tileset][0];
		else { tilesDone = true; if (tilesWait) { tilesWait = false; Module.removeRunDependency('tiles'); } }   /* None: text */
	}

	/* autosave: every 2 minutes and when the page is hidden (the game saves at its command prompt) */
	setInterval(function () { saveReq = true; }, 120000);
	setInterval(function () { if (app.running) app.sync(); }, 15000);
	document.addEventListener('visibilitychange', function () { if (document.hidden) { saveReq = true; app.sync(); } });
	window.addEventListener('pagehide', function () { app.sync(); });
	window.addEventListener('beforeunload', function (e) { if (app.running && nl.atCmd >= 0) { e.preventDefault(); e.returnValue = ''; } });

	document.addEventListener('keydown', onKey);
	document.addEventListener('DOMContentLoaded', function () {
		var mb = document.querySelector('#t-msg .body');
		['wheel', 'touchstart', 'pointerdown'].forEach(function (t) { mb.addEventListener(t, function () { byUser = true; }, { passive: true }); });
		mb.addEventListener('scroll', function () { if (byUser) msgStick = mb.scrollTop + mb.clientHeight >= mb.scrollHeight - 4; });
		wireMouse('#t-map canvas', P_MAP);
		wireMouse('#pop canvas', P_POP);
		/* a click on the Inventory window opens the game's inventory (at the command prompt) */
		document.querySelector('#t-inv canvas').addEventListener('mousedown', function (e) {
			e.preventDefault();
			if (app.running && nl.atCmd === 1) keys.push(105);
		});
		$('btn-tiles').onclick = toggleTileset;
		renderTileset();
		$('chk-sound').onchange = function () { toggleAudio('sound'); };
		$('chk-music').onchange = function () { toggleAudio('music'); };
		RvipWM.dropdown($('btn-audio'), $('menu-audio'));
		RvipWM.dropdown($('btn-file'), $('menu-file'));
		fetch('fonts.json').then(function (r) { return r.json(); }).then(function (list) {
			[[$('sel-font'), 'face'], [mapSel, 'mapFace']].forEach(function (a) {
				list.forEach(function (n) { var o = document.createElement('option'); o.value = n; o.textContent = n.replace(/^Web(Plus|437)_/, '').replace(/_/g, ' '); a[0].appendChild(o); });
				a[0].value = (L && L[a[1]]) || '';
			});
		}).catch(function () { });
		[[$('sel-font'), 'face'], [mapSel, 'mapFace']].forEach(function (a) {
			a[0].onchange = function () { if (!L) return; L[a[1]] = this.value; saveLayout(); loadFace(this.value, true); this.blur(); };
		});
		renderAudio();
		document.querySelectorAll('button').forEach(function (b) {
			b.addEventListener('mousedown', function (e) { e.preventDefault(); });
		});
	});
	var resizeTimer = 0;
	window.addEventListener('resize', function () {
		if (!L) return;
		clearTimeout(resizeTimer);
		resizeTimer = setTimeout(function () {
			applyDom();                          /* layout() fits the map again unless zoomed */
		}, 150);
	});

	/* tests: a pane's text, the map's tile ids */
	window.nlText = function (p) { var T = panes[p === undefined ? P_MAP : p]; return T ? T.text.slice(0, T.rows).join('\n') : ''; };
	window.nlTiles = function (y0, y1) {
		var T = panes[P_MAP], out = [];
		for (var y = y0; y < y1; y++) { var r = []; for (var x = 0; x < T.cols; x++) r.push(T.cells[(y * T.cols + x) * 4 + 3] | 0); out.push(r); }
		return out;
	};
	window.nlPanes = function () { return panes.map(function (T) { return T && { cols: T.cols, rows: T.rows, vc: T.vc, vr: T.vr, w: T.w, h: T.h, cw: T.cw, ch: T.ch }; }); };
	window.nlHero = function () { return hero; };
})();
