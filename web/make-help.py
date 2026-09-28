#!/usr/bin/env python3
"""Writes the in-page game guide (dist/help.html) for the NLarn web build.

Cloud variant (RVIP Part 2): self-contained. The complete key list is parsed
from the game's own help (lib/nlarn.hlp), the version from inc/nlarn.h; the
rest (about, tips, new player's guide, saving, the browser) is written here.
Shaped like a Docs GAMES/GUIDES entry (tagline, info, essentials, all, guide),
so the Mac's Docs folder can take it over; if that folder has an 'nlarn.html'
entry, its texts are used instead."""
import html, importlib.util, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DOCS = os.path.expanduser('~/Desktop/Games/Roguelikes/Docs')
PAGE = 'nlarn.html'
BASE = '8851b1f6420c17afc122a47e0b1b9e2b7b251878'
esc = html.escape


def kbd(k):
    return '<kbd>' + esc(k) + '</kbd>'


def version():
    h = open(os.path.join(ROOT, 'inc/nlarn.h')).read()
    return '.'.join(re.search(r'define VERSION_%s (\d+)' % n, h).group(1) for n in ('MAJOR', 'MINOR', 'PATCH'))


def help_keys():
    """[(section, [(key, text)])] from lib/nlarn.hlp: lines `KEY`k`end` text"""
    out, cur = [], None
    for line in open(os.path.join(ROOT, 'lib/nlarn.hlp'), encoding='utf-8'):
        t = re.match(r'`TITLE`(.*?)`end`', line)
        if t:
            cur = (t.group(1).strip(), [])
            out.append(cur)
            continue
        m = re.match(r'`KEY`(.+?)`end`\s+(.*)', line.rstrip('\n'))
        if m and cur:
            text = re.sub(r'`(KEY|EMPH)`(.*?)`end`', r'\2', m.group(2)).strip()
            key = m.group(1).replace('CTRL+', 'Ctrl+')
            cur[1].append((key, text))
    return [(s, k) for s, k in out if k]


VERSION = version()
SECTIONS = help_keys()
ALL = [(k, d) for s, keys in SECTIONS if not s.startswith('Wizard') for k, d in keys]

GAME = {
    'file': PAGE,
    'tagline': 'NLarn is a modern Larn: a small town above a cavern of ten levels and a volcano below it, '
               'and a daughter who dies if you are too slow.',
    'info': {
        'About the game': '<p>Your daughter has caught <em>dianthroritis</em>. The only cure is a potion hidden deep '
                          'in the volcano under the caverns of Larn, and the doctor gives her a limited time. You '
                          'start in town with a dagger and leather armour; the caverns entrance is the '
                          '<strong>O</strong> on the town map. Fight, collect gold and gems, buy better equipment '
                          'in town, and learn spells at the college, until you are strong enough to go down the '
                          'volcanic shaft <strong>I</strong> and find the potion.</p>',
        'Tips': '<ul>'
                '<li><strong>Time is the real enemy.</strong> Every turn counts against your daughter (the status '
                'window shows the turn as <strong>T</strong>; 100 turns are one <em>mobul</em>). Explore with '
                '<kbd>X</kbd> and use <kbd>&lt;</kbd> / <kbd>&gt;</kbd> to walk straight to the stairs.</li>'
                '<li>Sell gems to the bank in town; money buys armour and weapons in the DND store and '
                'education at the college, which raises your stats.</li>'
                '<li>Unknown potions and scrolls are risky but often useful: read scrolls when no monster is '
                'near, and keep a healing potion for emergencies once you know one.</li>'
                '<li>Rest with <kbd>w</kbd> to heal, but not for too long: time passes.</li>'
                '<li>Spells come from books (<kbd>r</kbd>); cast them with <kbd>c</kbd> and repeat the last one '
                'with <kbd>a</kbd>. Magic missile is a good first attack spell.</li>'
                '<li>Fountains, altars and thrones can bless or curse you: save first if you like to gamble.</li>'
                '<li>Do not dive too fast: monsters on deeper levels outpace a weak character quickly.</li>'
                '</ul>',
    },
    'essentials': [
        ('Moving', [('arrows / hjklyubn', 'Walk (the number pad works too)'),
                    ('HJKLYUBN', 'Run'), ('X', 'Auto-explore the level'),
                    ('< / >', 'Walk to the nearest known stairs; press again to take them'),
                    ('v', 'Travel to a place you choose')]),
        ('Items', [('i', 'Inventory: letter = main action, Enter = all actions'),
                   ('e', 'Equipment list'), (',', 'Pick up'), ('d', 'Drop'),
                   ('W / T', 'Wear or wield / take off'), ('q / r', 'Drink / read')]),
        ('Fighting', [('walk into it', 'Melee attack'), ('f', 'Fire a ranged weapon'),
                      ('c / a', 'Cast a spell / cast the last one again'), ('t', 'Throw an item')]),
        ('Game', [('Enter', 'Command menu (every command)'), ('?', 'In-game help'),
                  ('Ctrl+R', 'Message history'), ('Ctrl+S', 'Save and end the session'),
                  ('Ctrl+Q', 'Quit (the character is lost)')]),
    ],
    'all': ALL,
}

GUIDE = {
    'Your first steps': '<p>After the welcome screen choose <em>New Game</em>, type a name, pick a gender and a '
                        'character build (strong, agile, tough or smart). You start at home in town. Walk out, '
                        'look around (the Visible window lists what you see), and press <kbd>&gt;</kbd>: you walk '
                        'to the caverns entrance <strong>O</strong>; press it again to go down.</p>',
    'In the caverns': '<p>Press <kbd>X</kbd> to explore. It stops when a monster appears or something happens; '
                      'fight by walking into the monster, then press <kbd>X</kbd> again. Pick up what you find '
                      'with <kbd>,</kbd>. When the level is done, <kbd>&gt;</kbd> walks you to the stairs down.</p>',
    'Back to town': '<p>Go up with <kbd>&lt;</kbd> when you are hurt or rich. In town: the bank buys gems, the '
                    'DND store sells equipment, the college teaches (raising your stats), the trading post buys '
                    'items, and the monastery cures diseases and sells a few provisions. Enter a building by standing on it and '
                    'pressing <kbd>&gt;</kbd>.</p>',
    'Growing stronger': '<p>Experience levels raise your hit points and mana. Wear the best armour you can '
                        'afford, learn spells from books, and identify your potions and scrolls by using or '
                        'buying them. The caverns have ten levels; the volcano three more, where the potion '
                        'lies. Bring it home before time runs out.</p>',
}


def docs_entry():
    """the Mac Docs folder's entry, when it has one for NLarn"""
    try:
        sys.path.insert(0, DOCS)
        spec = importlib.util.spec_from_file_location('build_docs', os.path.join(DOCS, 'build-docs.py'))
        docs = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(docs)
        from guides import GUIDES
        return next(g for g in docs.GAMES if g['file'] == PAGE), dict(GUIDES[PAGE])
    except Exception:
        return None


if os.environ.get('NLARN_HELP_DOCS', '1') == '1':
    d = docs_entry()
    if d:
        GAME, GUIDE = d

SAVING = '''<ul>
<li><strong>Saving is automatic.</strong> The game is stored in this browser (IndexedDB) when a new character starts, every two minutes while it waits for your next command, and whenever you switch to another tab or window. Reloading the page and choosing <em>Continue saved Game</em> goes on from there.</li>
<li><kbd>Ctrl+S</kbd> saves and ends the session, as in the original; the page restarts by itself and the main menu offers <em>Continue saved Game</em>.</li>
<li>When your character dies, or you quit with <kbd>Ctrl+Q</kbd> (the character is lost), NLarn shows its end screen and the high scores and then its main menu for a new game. <em>Quit Game</em> in that menu restarts the page.</li>
<li>Each browser keeps <strong>one game</strong>. <em>File ▾ → New game</em> deletes it and starts over.</li>
<li><em>File ▾ → Export save</em> downloads the save file (<code>nlarn.sav</code>); <em>Import save</em> loads one. Use them for a backup or to move a game to another browser or computer (the same NLarn version reads it).</li>
<li>Your settings (<code>nlarn.ini</code>), the high scores, the window layout and sizes and the font choices are stored in the same browser storage.</li>
<li>Private/incognito windows and "clear site data" delete the stored game. Export first if it matters.</li>
</ul>'''

WEB = '''<ul>
<li><strong>Windows:</strong> the tiled map with Messages (with history) below it; Status, Inventory and Visible (monsters and items in view) on the right. Menus, lists and questions pop up over the map; the newest message also shows in a line over the map until your next command.</li>
<li><strong>Windows ▾</strong> switches between one window and several, shows or hides each window and resets the layout. Drag the gaps between windows to resize them, drag a title bar to move a window. Hover over a title bar for <em>✎</em> (rename), <em>A−</em> / <em>A+</em> (text size of that window) and <em>×</em>. Everything is remembered.</li>
<li><strong>Zoom:</strong> <em>A−</em> / <em>A+</em> on the Map title bar change the size of the map. A map bigger than its window scrolls to keep you in the middle.</li>
<li><strong>Tiles</strong> switches between the Amiga Larn tiles and plain text. <strong>Font</strong> picks the font of the text windows; in text mode the map has its own font chooser on its title bar.</li>
<li><strong>Mouse:</strong> click a spot on the map to travel there, click a monster to attack it; menus and lists take clicks too. A click on the Inventory window opens the inventory.</li>
<li><strong>Keys:</strong> the arrow keys, the number pad or <kbd>hjklyubn</kbd> move you; capital letters run. Browsers keep a few shortcuts for themselves (<kbd>Ctrl+W</kbd>, <kbd>Ctrl+T</kbd>, <kbd>Ctrl+N</kbd>, <kbd>Cmd</kbd> shortcuts on a Mac), so those never reach the game.</li>
<li><strong>Audio ▾</strong> holds the switches for sound effects and music (off by default).</li>
<li>If the game ever crashes, a message appears at the top; reload the page to continue from the last autosave.</li>
</ul>'''

KEY_HINTS = [
    ('?', 'In-game help: all commands'),
    ('X', 'Auto-explore: walk to the nearest unexplored spot or item'),
    ('Enter', 'Menu of all commands'),
    ('i', 'Inventory: letter = main action, Enter = all actions of an item'),
    ('<', 'Go up (walks to the nearest known stairs up first)'),
    ('>', 'Go down or enter a building (walks to the nearest known stairs down; in town: the caverns)'),
    ('Ctrl+S', 'Save and end the session'),
]


def dl(items):
    return '<dl>' + ''.join(f'<dt>{kbd(k)}</dt><dd>{esc(d)}</dd>' for k, d in items) + '</dl>'


def section(anchor, title, body):
    return f'<h2 id="h-{anchor}">{esc(title)}</h2>{body}'


info = dict(GAME['info'])
parts = []
toc = [('keys', 'Keyboard controls'), ('saving', 'Saving your game'), ('tips', 'Tips'),
       ('guide', "New player's guide"), ('web', 'In the browser'), ('version', 'About this version')]
parts.append('<p>' + esc(GAME['tagline']) + '</p>' + info['About the game'] + '<ul class="toc">' +
             ''.join(f'<li><a href="#h-{a}">{esc(t)}</a></li>' for a, t in toc) + '</ul>')

ess = ''.join(f'<div class="box"><h3>{esc(cat)}</h3>{dl(items)}</div>' for cat, items in GAME['essentials'])
all_keys = GAME['all']() if callable(GAME['all']) else GAME['all']
full = ''.join(f'<div>{kbd(k)}<span>{esc(d)}</span></div>' for k, d in all_keys)
parts.append(section('keys', 'Keyboard controls',
                     '<div class="box key"><h3>The keys to remember</h3>' + dl(KEY_HINTS) + '</div>'
                     '<h3>Essential keys</h3><div class="grid">' + ess + '</div>'
                     '<details><summary>Complete key list (' + str(len(all_keys)) + ' commands, from the game\'s help)</summary>'
                     '<div class="all">' + full + '</div></details>'))
parts.append(section('saving', 'Saving your game', SAVING))
parts.append(section('tips', 'Tips', info['Tips']))
parts.append(section('guide', "New player's guide", ''.join(f'<h3>{esc(t)}</h3>{b}' for t, b in GUIDE.items())))
parts.append(section('web', 'In the browser', WEB))

# RVIP W1: source and changes
parts.append(section('version', 'About this version', '<ul>'
             f'<li>Based on <strong>NLarn {VERSION}</strong>, upstream nlarn/nlarn @ {BASE[:7]}.</li>'
             f'<li>Original source: <a href="https://github.com/nlarn/nlarn/tree/{BASE}" target="_blank" rel="noopener">nlarn/nlarn, commit {BASE[:7]}</a> (GPL 3).</li>'
             '<li>Our changes (browser build, auto-explore, command menu, inventory, tiles, windows): '
             f'<a href="https://github.com/memmaker/nlarn" target="_blank" rel="noopener">memmaker/nlarn</a> '
             f'(<a href="https://github.com/memmaker/nlarn/compare/{BASE[:7]}...master" target="_blank" rel="noopener">all changes</a>).</li>'
             '<li>NLarn by Joachim de Groot, with Johanna Ploog and contributors; based on Larn by Noah Morgan.</li>'
             '<li>Tiles: the Amiga Larn set from <a href="https://larn.org/" target="_blank" rel="noopener">larn.org</a> '
             '(github.com/primeau/Larn, MIT, Jason Primeau), with a few recoloured for town and volcano terrain.</li>'
             '<li>Fonts: the text-window fonts come from the Ultimate Oldschool PC Font Pack by VileR '
             '(<a href="https://int10h.org/oldschool-pc-fonts/" target="_blank" rel="noopener">int10h.org</a>, CC BY-SA 4.0); '
             'the default is your system\'s monospace font (DejaVu Sans Mono, Menlo or Consolas).</li>'
             '</ul>'))
print('\n'.join(parts))
