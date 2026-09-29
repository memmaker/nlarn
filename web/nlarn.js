/*
 * NLarn in the browser: draws the panes the curses shim routes (port/be_web.c
 * calls Module.nl), keyboard and mouse input, tiling windows (../rvip-wm.js),
 * saves in IndexedDB (../rvip-app.js). Loaded before nlarn-core.js.
 * The game decides every map cell (character, RGB colours, tile id, W0) and
 * sends the text windows as trimmed, coloured lines (W0 rule 6: HTML text,
 * the map is the only canvas); this file only shows them.
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

	var panes = [];            /* the map: {cv, ctx, cols, rows, cw, ch, cells, text, cy, cx} */
	var keys = [];
	var tiles = new Image(), tilesReady = false;
	var saveReq = false, app;
	var hero = { y: -1, x: -1, z: -1 };
	var pop = { y0: 0, x0: 0 };
	var dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
	var L = null, rects = {}, wm = null;

	/* Messages follows the newest line unless the player scrolled up to read
	 * back (msgMark before a change); a key follows again */
	var keyFollow = false, msgFollow = null, msgTimer = false;
	document.addEventListener('keydown', function () { keyFollow = true; }, true);

	function $(id) { return document.getElementById(id); }
	function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }
	function hex(v) { return '#' + ('00000' + (v & 0xffffff).toString(16)).slice(-6); }
	function status(msg, isError) { app.status(msg, isError); }

	/* ---------- panes ---------- */

	/* top-bar font: the text windows; the map (text mode) has its own */
	function face(p) { var n = L && (p === P_MAP ? L.mapFace : L.face); return n ? '"' + n + '", ' + FONT : FONT; }

	/* the map's cell size from its zoom; rebuilds the canvas and redraws */
	function shape(p) {
		var T = panes[p];
		if (!T) return;
		T.cw = L.tile / 2; T.ch = L.tile;
		T.font = (L.mapFace ? '' : 'bold ') + Math.round(T.ch * 0.78) + 'px ' + face(p);
		var w = T.cols * T.cw, h = T.rows * T.ch;
		T.cv.width = Math.round(w * dpr); T.cv.height = Math.round(h * dpr);
		T.w = w; T.h = h;
		T.ctx = T.cv.getContext('2d');
		T.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		T.ctx.imageSmoothingEnabled = false;                 /* nearest-neighbour tiles */
		T.ctx.textAlign = 'center'; T.ctx.textBaseline = 'middle';
		T.ctx.fillStyle = '#000'; T.ctx.fillRect(0, 0, w, h);
		for (var y = 0; y < T.rows; y++) for (var x = 0; x < T.cols; x++) cell(p, y, x);
		scrollMap();
	}

	function makePane(p, cols, rows) {
		panes[p] = { cv: document.querySelector('#t-map canvas'), cols: cols, rows: rows, cells: new Uint32Array(cols * rows * 4),
			text: [], cy: -1, cx: -1 };
		for (var i = 0; i < cols * rows; i++) { panes[p].cells[i * 4] = 32; panes[p].cells[i * 4 + 3] = -1 >>> 0; }
		shape(p);
	}

	/* ---------- text windows (RVIP W0 rule 6): HTML lines from the game ----------
	 * The game (port/wcurses.c send_text) sends each changed row trimmed:
	 * highlighted cells between \x01 and \x02, colour runs "\x05#rrggbb" or bold
	 * "\x05*#rrggbb" (a highlight: "/#rrggbb" background appended) up to \x06, with the row colour and icon tile, and the
	 * rows in use; the WM sets the text size (A−/A+ on each window). */
	var txt = [];              /* pane -> {el, lines, css, tile, n} */
	var cur = { p: -1, y: 0, x: 0 };
	function textPane(p) {
		var el = p === P_POP ? $('pop').firstElementChild : document.querySelector('#t-' + WIN[p] + ' .body' + (p === P_INV ? '' : ' pre'));
		el.textContent = '';
		txt[p] = { el: el, lines: [], css: [], tile: [], n: 0 };
	}
	var RUN = /\x05\*?#[0-9a-f]{6}(?:\/#[0-9a-f]{6})?|[\x01\x02\x06]/g;
	function esc(s) { return s.replace(/&/g, '&amp;').replace(/</g, '&lt;'); }
	function rowHtml(p, y) {
		var T = txt[p], s = T.lines[y] || '', cx = cur.p === p && cur.y === y ? cur.x : -1;
		if (cx >= 0) {                              /* the cursor: one cell, past the end if need be */
			var vis = s.replace(RUN, '');
			while (vis.length <= cx) { s += ' '; vis += ' '; }
			for (var i = 0, k = 0; i < s.length; i++) {
				if (s[i] === '\x05') { i += /^\x05\*?#[0-9a-f]{6}(?:\/#[0-9a-f]{6})?/.exec(s.slice(i))[0].length - 1; continue; }
				if (s[i] > '\x06' && k++ === cx) break;
			}
			s = s.slice(0, i) + '\x03' + s[i] + '\x04' + s.slice(i + 1);
		}
		return esc(s).replace(/\x01/g, '<span class="so">').replace(/[\x02\x04\x06]/g, '</span>')
			.replace(/\x03/g, '<span class="cur">')
			.replace(/\x05(\*?)(#[0-9a-f]{6})(?:\/(#[0-9a-f]{6}))?/g, function (m, b, c, g) { return '<span style="color:' + c + (b ? ';font-weight:bold' : '') + (g ? ';background:' + g : '') + '">'; });
	}
	function drawRow(p, y) {
		var T = txt[p], d = T && T.el.children[y];
		if (!d) return;
		d.innerHTML = rowHtml(p, y);
		d.style.color = T.css[y] || '';
		if (p === P_INV) { var ic = visIcon(T.tile[y]); if (ic) d.insertBefore(ic, d.firstChild); }
	}
	function setRows(p, n) {
		var T = txt[p];
		while (T.el.children.length < n) { T.el.appendChild(document.createElement('div')); drawRow(p, T.el.children.length - 1); }
		while (T.el.children.length > n) T.el.removeChild(T.el.lastChild);
		T.n = n;
	}
	/* before a Messages change: was it scrolled to the end? After the batch
	 * (the game yields to the page), follow the newest line if so */
	function msgMark() {
		var b = txt[P_MSG] && txt[P_MSG].el.parentNode;
		if (!b) return;
		if (msgFollow == null) msgFollow = b.scrollTop + b.clientHeight >= b.scrollHeight - 4;
		if (!msgTimer) {
			msgTimer = true;
			Promise.resolve().then(function () {
				if (msgFollow || keyFollow) b.scrollTop = b.scrollHeight;
				msgFollow = null; keyFollow = false; msgTimer = false;
			});
		}
	}
	function popFont() { $('pop').style.fontSize = RvipWM.fontSize('msg') + 'px'; placePop(); }
	function placePop() { if (!$('pop').hidden && rects.map && L) RvipWM.popup($('pop'), { x: L.tile }); }

	function cell(p, y, x) {
		var T = panes[p], c = T.ctx, i = (y * T.cols + x) * 4;
		var code = T.cells[i], fg = hex(T.cells[i + 1]), bga = T.cells[i + 2], t = T.cells[i + 3] | 0;
		var px = x * T.cw, py = y * T.ch;
		c.fillStyle = hex(bga);
		c.fillRect(px, py, T.cw, T.ch);
		if (t >= 0 && tilesReady) {
			var id = t & 0x7fff;
			c.fillStyle = '#000'; c.fillRect(px, py, T.cw, T.ch);
			if (t & TILE_DIM) c.globalAlpha = 0.45;
			c.drawImage(tiles, (id % PER_ROW) * TW, ((id / PER_ROW) | 0) * TH, TW, TH, px, py, T.cw, T.ch);
			c.globalAlpha = 1;
			drawCursor(p, y, x);
			return;
		}
		var at = bga >>> 24;
		if (at & 2) { c.fillStyle = fg; c.fillRect(px, py + T.ch - 1, T.cw, 1); }
		if (code > 32) {
			c.font = T.font;
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
		var sideW = Math.ceil(SIDE_COLS * font * 0.6) + BORDER + 12;   /* ~0.6em per monospace cell */
		TILE_STEPS.forEach(function (t) { if (MAP_COLS * t / 2 + BORDER <= W - sideW - GUT && MAP_ROWS * t + BORDER + TITLE_H <= H * 0.74) tile = t; });
		var mapH = MAP_ROWS * tile + BORDER + TITLE_H;
		return { v: 1, tile: tile, auto: true, mt: {},
			split: { bottom: (mapH + GUT / 2) / H, side: (W - sideW - GUT / 2) / W,
				stat: (24 * Math.round(font * 1.3) + TITLE_H + BORDER + GUT / 2) / H },
			audio: { sound: false } };
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
				if (s.audio) d.audio = { sound: s.audio.sound === true };
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
		var line = Math.round(RvipWM.fontSize('msg') * 1.3) * 2 + 6, stat = Math.ceil(22 * RvipWM.fontSize('stat') * 0.6) + BORDER + 12;
		wm = RvipWM({
			area: $('game'), menu: $('btn-layout'),
			wins: [{ id: 'map', title: 'Map' }, { id: 'msg', title: 'Messages' }, { id: 'stat', title: 'Status' }, { id: 'inv', title: 'Inventory' }, { id: 'vis', title: 'Visible' }],
			multi: { d: 'h', r: s.side, a: { d: 'v', r: s.bottom, a: 'map', b: 'msg' }, b: { d: 'v', r: s.stat, a: 'stat', b: { d: 'v', r: 0.6, a: 'inv', b: 'vis' } } },
			single: { d: 'v', r: line / A.h, a: 'msg', b: { d: 'h', r: 1 - stat / A.w, a: 'map', b: 'stat' } },
			state: L.wm,
			save: function (st) { L.wm = st; saveLayout(); },
			layout: function (r) { rects = r; L.tile = mapTile(); shape(P_MAP); placePop(); },
			/* A− / A+: the map steps its tiles; the text windows are the WM's; the pop-up follows Messages */
			zoom: { map: function (size, d) { zoomMap(d); }, msg: popFont },
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

	function resetLayout() {
		var a = L.audio, fc = L.face, mf = L.mapFace, ts = L.tiles;
		L = defaultLayout(); L.audio = a; L.face = fc; L.mapFace = mf; L.tiles = ts; L.wm = wm.state ? wm.state() : L.wm;
		L.tile = mapTile();
		shape(P_MAP); popFont();
		applyDom(); saveLayout();
	}

	/* ---------- audio ---------- */
	/* sound events come from the game (SOUND() -> port/be_web.c); web/mksounds.py
	 * synthesizes one wav per event at build time; rvip-sound.js plays them.
	 * Nothing is fetched until Sound effects is on. NLarn has no music. */
	var audio = { cfg: null, loading: false, played: 0 };
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
	function toggleAudio(k) {
		L.audio[k] = !L.audio[k];
		if (k === 'sound' && L.audio.sound && !audio.cfg) play('');   /* load the event list */
		renderAudio(); saveLayout();
	}
	function renderAudio() {
		$('chk-sound').checked = !!(L && L.audio.sound);
	}

	/* ---------- called by the game (port/be_web.c) ---------- */
	var nl = {
		init: function (p, cols, rows) {
			if (!L) loadLayout();
			if (p === P_MAP) makePane(p, cols, rows); else textPane(p);
			if (!wm && panes[P_MAP] && txt[P_STATUS]) {
				$('game').hidden = false; status('');
				makeWM(); applyFace(); popFont();
			}
		},
		draw: function (p, idx, c, y0, y1, cy, cx) {   /* the map's changed rows */
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
		},
		line: function (p, y, s, c, t) {
			var T = txt[p];
			if (!T) return;
			if (p === P_MSG) msgMark();
			T.lines[y] = s; T.css[y] = c; T.tile[y] = t;
			if (y < T.n) drawRow(p, y);
		},
		rows: function (p, n) { if (!txt[p]) return; if (p === P_MSG) msgMark(); setRows(p, n); if (p === P_POP) placePop(); },
		cursor: function (p, y, x) {   /* a text pane's cursor (text entry in a pop-up); y < 0: none */
			var o = cur.p, oy = cur.y;
			if (y < 0) { if (o === p) cur.p = -1; } else { cur.p = p; cur.y = y; cur.x = x; }
			if (txt[o] && (o !== cur.p || oy !== cur.y)) drawRow(o, oy);
			if (y >= 0 && txt[p]) drawRow(p, y);
		},
		popup: function (rows, cols, y0, x0) {
			if (!rows) { $('pop').hidden = true; txt[P_POP] = null; if (cur.p === P_POP) cur.p = -1; return; }
			pop.y0 = y0; pop.x0 = x0;
			if (cur.p === P_POP) cur.p = -1;
			textPane(P_POP);
			$('pop').hidden = false;
			placePop();
		},
		popbg: function (rgb) { $('pop').style.background = '#' + ('00000' + rgb.toString(16)).slice(-6); },
		icons: function () { return tilesReady ? 1 : 0; },
		hero: function (y, x, z) {
			if (y === hero.y && x === hero.x && z === hero.z) return;
			var oy = hero.y, ox = hero.x;
			hero.y = y; hero.x = x; hero.z = z;
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
			app.running = false;
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
	 * the map is the screen's top-left, the pop-up sits at its own origin: its
	 * cell is the row (a line of the <pre>) and the character under the pointer */
	var down = null;
	function popCell(e) {
		var T = txt[P_POP];
		if (!T) return null;
		var d = e.target.closest && e.target.closest('#pop .txt > div');
		if (!d) return null;
		var y = Array.prototype.indexOf.call(T.el.children, d), x = 0, node = null, off = 0;
		if (document.caretPositionFromPoint) { var cp = document.caretPositionFromPoint(e.clientX, e.clientY); if (cp) { node = cp.offsetNode; off = cp.offset; } }
		else if (document.caretRangeFromPoint) { var cr = document.caretRangeFromPoint(e.clientX, e.clientY); if (cr) { node = cr.startContainer; off = cr.startOffset; } }
		if (node && d.contains(node)) {
			var w = document.createTreeWalker(d, NodeFilter.SHOW_TEXT), n;
			while ((n = w.nextNode()) && n !== node) x += n.length;
			x += off;
		}
		return { y: y, x: x };
	}
	function mouse(e, p, b) {
		if (!app.running) return;
		var x, y;
		if (p === P_POP) {
			var c = popCell(e);
			if (!c) return;
			y = c.y + pop.y0; x = c.x + pop.x0;
		} else {
			var T = panes[p];
			if (!T) return;
			var r = T.cv.getBoundingClientRect();
			x = Math.floor((e.clientX - r.left) / r.width * T.cols); y = Math.floor((e.clientY - r.top) / r.height * T.rows);
			if (x < 0 || y < 0 || x >= T.cols || y >= T.rows) return;
		}
		keys.push(BE_MOUSE | b << 16 | y << 8 | x);
	}
	function wireMouse(sel, p) {
		var cv = document.querySelector(sel);
		cv.addEventListener('mousedown', function (e) {
			e.preventDefault();
			if (e.button === 0) { down = p; mouse(e, p, 1); } else if (e.button === 2) mouse(e, p, 3);
		});
		cv.addEventListener('mousemove', function (e) { if (down === p) mouse(e, p, 6); });
		if (p === P_MAP) cv.addEventListener('wheel', function (e) { e.preventDefault(); mouse(e, p, e.deltaY < 0 ? 4 : 5); }, { passive: false });
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
			if (panes[P_MAP]) shape(P_MAP);
			if (txt[P_INV]) for (var y = 0; y < txt[P_INV].n; y++) drawRow(P_INV, y);
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
		var redraw = function () { if (panes[P_MAP]) shape(P_MAP); applyFace(); applyDom(); };
		if (!n) { if (now) redraw(); return; }
		var ff = new FontFace(n, 'url(../fonts/' + n + '.woff)');
		ff.load().then(function () { document.fonts.add(ff); redraw(); }).catch(function () { status('Could not load the font ' + n + '.', true); });
	}
	/* the top-bar font on every text window and the pop-up */
	function applyFace() {
		['#t-stat .body', '#t-msg .body', '#t-inv .body', '#t-vis .body', '#pop'].forEach(function (q) { var e = document.querySelector(q); if (e) e.style.fontFamily = face(P_STATUS); });
	}
	/* Visible and Inventory icon: the tile (8x16) as a CSS sprite sized in em,
	 * so it grows with the window's A+ */
	function visIcon(t) {
		if (!tilesReady || !(t >= 0)) return null;
		var s = document.createElement('i'), h = 1.2, w = h * TW / TH, rows = Math.max(1, Math.round(tiles.naturalHeight / TH));
		s.className = 'wm-ic';
		s.style.cssText = 'display:inline-block;vertical-align:middle;width:' + w + 'em;height:' + h + 'em;margin:0 0.3em;image-rendering:pixelated;' +
			'background:url(' + tiles.src + ') ' + -(t % PER_ROW) * w + 'em ' + -((t / PER_ROW) | 0) * h + 'em / ' + PER_ROW * w + 'em ' + rows * h + 'em no-repeat';
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
		wireMouse('#t-map canvas', P_MAP);
		wireMouse('#pop', P_POP);
		/* a click on the Inventory window opens the game's inventory (at the command prompt) */
		document.querySelector('#t-inv .body').addEventListener('mousedown', function (e) {
			e.preventDefault();
			if (app.running && nl.atCmd === 1) keys.push(105);
		});
		$('btn-tiles').onclick = toggleTileset;
		renderTileset();
		$('chk-sound').onchange = function () { toggleAudio('sound'); };
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
	window.nlText = function (p) {
		if (p !== undefined && p !== P_MAP) {
			var X = txt[p];
			return X ? X.lines.slice(0, X.n).map(function (l) { return (l || '').replace(RUN, ''); }).join('\n') : '';
		}
		var T = panes[P_MAP]; return T ? T.text.slice(0, T.rows).join('\n') : '';
	};
	window.nlTiles = function (y0, y1) {
		var T = panes[P_MAP], out = [];
		for (var y = y0; y < y1; y++) { var r = []; for (var x = 0; x < T.cols; x++) r.push(T.cells[(y * T.cols + x) * 4 + 3] | 0); out.push(r); }
		return out;
	};
	window.nlPanes = function () { return panes.map(function (T) { return T && { cols: T.cols, rows: T.rows, w: T.w, h: T.h, cw: T.cw, ch: T.ch }; }); };
	window.nlLines = function (p) { var X = txt[p]; return X ? X.lines.slice(0, X.n) : []; };   /* tests: raw lines with runs */
	window.nlHero = function () { return hero; };
})();
