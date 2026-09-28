/* NLarn web page glue (RVIP stage 1: one canvas terminal).
 * The game (port/be_web.c) hands finished cells (code point, RGB colours,
 * attributes) per pane; this file only draws them and forwards input. */
(function () {
	'use strict';
	var FONT = 16, FAMILY = '"DejaVu Sans Mono", Menlo, Consolas, monospace';
	var canvas = document.getElementById('screen'), ctx = canvas.getContext('2d');
	var status = document.getElementById('status');
	var cols = 90, rows = 25, cw = 9, ch = 18, dpr = window.devicePixelRatio || 1;
	var text = [];                 /* text shadow of the screen (tests) */
	var keys = [];
	var syncing = false, syncAgain = false, ready = false;
	var DIR = '/nlarn';            /* IndexedDB folder (RvipApp.dir for /roguelikes/nlarn/) */

	function hex(v) { return '#' + ('00000' + (v & 0xffffff).toString(16)).slice(-6); }

	/* map tiles: the game sends a tile id per map cell (port/tiles.c, W0);
	 * this page only blits them from the sheet, nearest-neighbour */
	var TW = 8, TH = 16, PER_ROW = 32, TILE_DIM = 0x8000;
	var TILESETS = [['tiles.png', 'Amiga'], [null, 'None']], tileset = 0;
	var tiles = new Image(), tilesReady = false;
	var cells = new Uint32Array(cols * rows * 4);   /* last cells from the game */
	var STEPS = [16, 20, 24, 28, 32, 36, 40, 48];    /* cell height; width = height / 2 (tile 8x16) */

	function layout() {
		var avail = window.innerHeight - 28, step = STEPS[0];
		STEPS.forEach(function (h) { if (cols * h / 2 <= window.innerWidth && rows * h <= avail) step = h; });
		ch = step; cw = step / 2; FONT = Math.round(step * 0.78);
		canvas.width = Math.round(cols * cw * dpr);
		canvas.height = Math.round(rows * ch * dpr);
		canvas.style.width = cols * cw + 'px';
		canvas.style.height = rows * ch + 'px';
		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.imageSmoothingEnabled = false;
		ctx.textBaseline = 'middle';
		ctx.textAlign = 'center';
		redraw();
	}

	function drawCell(y, x, cy, cx) {
		var i = (y * cols + x) * 4;
		var code = cells[i], fg = cells[i + 1], bga = cells[i + 2], t = cells[i + 3] | 0;
		ctx.fillStyle = hex(bga);
		ctx.fillRect(x * cw, y * ch, cw, ch);
		if (t >= 0 && tilesReady) {
			var id = t & 0x7fff;
			ctx.fillStyle = '#000';
			ctx.fillRect(x * cw, y * ch, cw, ch);
			if (t & TILE_DIM) ctx.globalAlpha = 0.45;
			ctx.drawImage(tiles, (id % PER_ROW) * TW, ((id / PER_ROW) | 0) * TH, TW, TH, x * cw, y * ch, cw, ch);
			ctx.globalAlpha = 1;
			return;
		}
		if (y === cy && x === cx) {
			ctx.fillStyle = hex(fg);
			ctx.fillRect(x * cw, y * ch + ch - 3, cw, 2);
		}
		if (code > 32) {
			ctx.font = ((bga >>> 24) & 1 ? 'bold ' : '') + FONT + 'px ' + FAMILY;
			ctx.fillStyle = hex(fg);
			ctx.fillText(String.fromCodePoint(code), x * cw + cw / 2, y * ch + ch / 2 + 1);
		}
	}

	var curY = -1, curX = -1;
	function redraw() {
		for (var y = 0; y < rows; y++) for (var x = 0; x < cols; x++) drawCell(y, x, curY, curX);
	}

	function setTileset(n) {
		tileset = n;
		try { localStorage.setItem('nlarn-tileset', String(n)); } catch (err) { /* no storage */ }
		var b = document.getElementById('btn-tiles');
		if (b) b.textContent = 'Tiles: ' + TILESETS[n][1];
		if (!TILESETS[n][0]) { tilesReady = false; redraw(); return; }
		tiles.onload = function () { if (TILESETS[tileset][0]) { tilesReady = true; redraw(); } };
		tiles.src = TILESETS[n][0];
	}
	try { tileset = (+localStorage.getItem('nlarn-tileset') || 0) % TILESETS.length; } catch (err) { /* no storage */ }
	document.getElementById('btn-tiles').onclick = function () {
		setTileset((tileset + 1) % TILESETS.length);
		canvas.focus();
	};
	window.addEventListener('resize', function () { layout(); });

	function sync() {
		if (!ready) return;
		if (syncing) { syncAgain = true; return; }
		syncing = true;
		FS.syncfs(false, function (err) {
			syncing = false;
			if (err) console.warn('syncfs', err);
			if (syncAgain) { syncAgain = false; sync(); }
		});
	}

	var nl = {
		init: function (p, c, r) {
			if (p !== 0) return;
			cols = c; rows = r;
			cells = new Uint32Array(cols * rows * 4);
			text = [];
			for (var y = 0; y < rows; y++) text.push(new Array(cols + 1).join(' '));
			layout();
			status.textContent = '';
		},
		draw: function (p, idx, c, y0, y1, cy, cx) {
			if (p !== 0) return;
			var H = HEAPU32;
			if (cy !== curY || cx !== curX) { curY = cy; curX = cx; }
			for (var y = y0; y < y1; y++) {
				var line = '';
				for (var x = 0; x < c; x++) {
					var i = idx + (y * c + x) * 4, j = (y * cols + x) * 4;
					cells[j] = H[i]; cells[j + 1] = H[i + 1]; cells[j + 2] = H[i + 2]; cells[j + 3] = H[i + 3];
					line += String.fromCodePoint(H[i] || 32);
					drawCell(y, x, cy, cx);
				}
				text[y] = line;
			}
		},
		key: function () { return keys.length ? keys.shift() : -1; },
		bell: function () { },
		sync: sync
	};

	/* keys: characters as code points, special keys as curses KEY_* codes */
	var SPECIAL = {
		Enter: 13, Escape: 27, Tab: 9, Backspace: 263,
		ArrowDown: 258, ArrowUp: 259, ArrowLeft: 260, ArrowRight: 261,
		Home: 262, End: 360, PageUp: 339, PageDown: 338, Insert: 331, Delete: 330
	};
	document.addEventListener('keydown', function (e) {
		var k = -1;
		if (SPECIAL[e.key] !== undefined) k = SPECIAL[e.key];
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
	});

	/* mouse: BE_MOUSE | button << 16 | y << 8 | x (port/be.h) */
	var down = false;
	function mouse(e, b) {
		var r = canvas.getBoundingClientRect();
		var x = Math.floor((e.clientX - r.left) / r.width * cols);
		var y = Math.floor((e.clientY - r.top) / r.height * rows);
		if (x < 0 || y < 0 || x >= cols || y >= rows) return;
		keys.push(0x40000000 | b << 16 | y << 8 | x);
	}
	canvas.addEventListener('mousedown', function (e) {
		e.preventDefault(); canvas.focus();
		if (e.button === 0) { down = true; mouse(e, 1); } else if (e.button === 2) mouse(e, 3);
	});
	window.addEventListener('mouseup', function (e) { if (e.button === 0 && down) { down = false; mouse(e, 2); } });
	canvas.addEventListener('mousemove', function (e) { if (down) mouse(e, 6); });
	canvas.addEventListener('wheel', function (e) { e.preventDefault(); mouse(e, e.deltaY < 0 ? 4 : 5); }, { passive: false });
	canvas.addEventListener('contextmenu', function (e) { e.preventDefault(); });

	setInterval(sync, 15000);
	document.addEventListener('visibilitychange', function () { if (document.hidden) sync(); });
	window.addEventListener('pagehide', sync);

	window.Module = {
		nl: nl,
		arguments: ['-D', DIR],
		preRun: [function () {
			FS.mkdirTree(DIR);
			FS.mount(IDBFS, {}, DIR);
			FS.mkdirTree('/nlarn-data');
			FS.chdir('/nlarn-data');
			addRunDependency('idbfs');
			FS.syncfs(true, function (err) {
				if (err) console.warn('syncfs', err);
				ready = true;
				removeRunDependency('idbfs');
			});
		}],
		print: function (s) { console.log(s); },
		printErr: function (s) { console.warn(s); },
		onAbort: function (w) { status.textContent = 'The game crashed (' + w + '). Reload the page.'; }
	};
	window.addEventListener('unhandledrejection', function (e) {
		status.textContent = 'The game crashed. Reload the page.';
		console.error(e.reason);
	});
	window.nlText = function () { return text.join('\n'); };
	/* tests: the tile ids of the map rows (-1 = text) */
	window.nlTiles = function (y0, y1) {
		var out = [];
		for (var y = y0; y < y1; y++) { var r = []; for (var x = 0; x < cols; x++) r.push(cells[(y * cols + x) * 4 + 3] | 0); out.push(r); }
		return out;
	};
	setTileset(tileset);
})();
