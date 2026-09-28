#!/usr/bin/env python3
"""Pick the Dubtrain Angband Sound Pack samples (CC BY 4.0, Dubtrain
<angband@dubtrain.com>) for the sound events NLarn raises (SOUND("...") in
src/*.c, inc/display.h), copy them into web/sound and write
web/sound/sounds.json {event: [files]}.  web/build.sh copies web/sound to
dist/sound; web/nlarn.js plays a random file per event via rvip-sound.js.

The pack comes from upstream Angband (lib/sounds/*.mp3 + lib/customize/sound.prf):
  git clone --filter=blob:none --no-checkout https://github.com/angband/angband
  (sounds.py checks out only the files it uses)
Usage (repo root): python3 web/sounds.py [angband-checkout]   (default ../ref/angband)
Same event set as memmaker/larn and ularn web/sounds.py, plus NLarn's own."""
import glob, json, os, re, shutil, subprocess, sys

ANG = sys.argv[1] if len(sys.argv) > 1 else os.path.join('..', 'ref', 'angband')
OUT = os.path.join('web', 'sound')
# event -> Angband sound.prf message (upper case) or a list of sample names
MAP = {
    'hit': 'HIT', 'kill': 'KILL', 'mon_hit': 'MON_HIT', 'breathe': 'BR_FIRE',
    'death': 'DEATH', 'level': 'LEVEL', 'spell': 'SPELL', 'quaff': 'QUAFF',
    'study': 'STUDY', 'drop': 'DROP', 'money1': 'MONEY1', 'wield': 'WIELD',
    'wear': 'WIELD', 'opendoor': 'OPENDOOR', 'shutdoor': 'SHUTDOOR',
    'stairs_up': 'STAIRS_UP', 'stairs_down': 'STAIRS_DOWN',
    'store_enter': 'STORE_ENTER', 'store_home': 'STORE_HOME', 'store5': 'STORE5',
    'shoot': 'SHOOT', 'shoot_hit': 'SHOOT_HIT',
    # DASP's MISS is a bow sample; a melee miss is a swing (RVIP-Finetuning, Sound)
    'miss': ['plc_miss_swish'],
    # no Dubtrain event for picking up an item: a close sample (as Larn/Ularn)
    'pickup': ['plm_chest_latch'],
}

events = set()
for f in glob.glob('src/*.c'):
    events |= set(re.findall(r'SOUND\("(\w+)"', open(f, encoding='utf-8').read()))
    for a, b in re.findall(r'SOUND\([^;]*\?\s*"(\w+)"\s*:\s*"(\w+)"\)', open(f, encoding='utf-8').read()):
        events |= {a, b}
missing = events - set(MAP)
assert not missing, 'events without a sample: %s' % sorted(missing)

prf = {}
for line in open(os.path.join(ANG, 'lib/customize/sound.prf'), encoding='utf-8'):
    if line.startswith('sound:'):
        _, msg, names = line.strip().split(':', 2)
        prf[msg] = names.split()

used = {}
for e in sorted(events):
    names = MAP[e] if isinstance(MAP[e], list) else prf[MAP[e]]
    used[e] = [n + '.mp3' for n in names]
files = sorted({f for v in used.values() for f in v})
paths = ['lib/sounds/' + f for f in files]
subprocess.check_call(['git', '-C', ANG, 'sparse-checkout', 'add'] + ['/' + p for p in paths])
if os.path.isdir(OUT):
    shutil.rmtree(OUT)
os.makedirs(OUT)
for p in paths:
    shutil.copy(os.path.join(ANG, p), OUT)
json.dump(used, open(os.path.join(OUT, 'sounds.json'), 'w'), indent=0, sort_keys=True)
print('%d events, %d samples, %d KB' % (len(used), len(files),
      sum(os.path.getsize(os.path.join(OUT, f)) for f in files) // 1024))
