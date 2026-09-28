#!/usr/bin/env python3
"""Packs the Amiga Larn tiles (port/amiga/*.png, 8x16, from larn.org /
github.com/primeau/Larn src/img, MIT, primeau's ULarn mode included) into
web/tiles.png and writes port/tilemap.h for port/tiles.c.
Copied from ~/Games/larn and ~/Games/ularn port/mktiles.py.

Every NLarn id gets a tile from this one set: monsters (MT_*), map tile
types (LT_*), stationary objects (LS_*), traps (TT_*), item types (IT_*)
and every kind of amulet, ammo, armour, container, gem, potion, ring,
scroll, book (SP_*) and weapon. The ids are read from the game's enum
macros in inc/*.h, and the script fails if one has no tile (100%, no
ASCII fallback). Where the Amiga set has no picture, a stand-in from the
same set is used, sometimes recoloured (`tint`): grass, dirt, water, lava,
fire, gas, mountains, some traps, armour pieces Larn lacks.

Primeau names: mN = monster N, mNu / mNv = ULarn art / visible stalker and
demons, oN = object N (Larn 12 ids; 80 = LRS sign, 82 = sphere of
annihilation), wN = wall by neighbour bits (2 up, 4 right, 8 down,
16 left). Run from the repo root."""
import re
from pathlib import Path
from PIL import Image

SRC = Path("port/amiga")
PER_ROW = 32


def enum(header, macro):
    """Names of an X-macro enum (`#define <macro>(X) \\ X(A,) ...`), without *_MAX."""
    text = Path(header).read_text()
    body = text[text.index(f"#define {macro}("):]
    lines = []
    for line in body.splitlines():
        lines.append(line)
        if not line.rstrip().endswith("\\"):
            break
    names = re.findall(r"\w+\((\w+),", "\n".join(lines[1:]))
    return [n for n in names if not n.endswith("_MAX") and n not in ("IT_ALL",)]


ENUMS = {
    "mon": enum("inc/monsters.h", "MONSTER_TYPE_ENUM"),
    "lt": enum("inc/map.h", "MAP_TILE_TYPE_ENUM"),
    "so": enum("inc/sobjects.h", "SOBJECT_TYPE_ENUM"),
    "trap": enum("inc/traps.h", "TRAP_TYPE_ENUM"),
    "it": enum("inc/items.h", "ITEM_TYPE_ENUM"),
    "amulet": enum("inc/amulets.h", "AMULET_TYPE_ENUM"),
    "ammo": enum("inc/weapons.h", "AMMO_TYPE_ENUM"),
    "armour": enum("inc/armour.h", "ARMOUR_TYPE_ENUM"),
    "container": enum("inc/container.h", "CONTAINER_TYPE_ENUM"),
    "gem": enum("inc/gems.h", "GEM_TYPE_ENUM"),
    "potion": enum("inc/potions.h", "POTION_TYPE_ENUM"),
    "ring": enum("inc/rings.h", "RING_TYPE_ENUM"),
    "scroll": enum("inc/scrolls.h", "SCROLL_TYPE_ENUM"),
    "book": enum("inc/spells.h", "SPELL_TYPE_ENUM"),
    "weapon": enum("inc/weapons.h", "WEAPON_TYPE_ENUM"),
}

# Amiga palette classes (primeau's art uses these few colours)
RED, DRED, GREY, WHITE, YELLOW, BLUE, GREEN, LGREY = (
    (255, 0, 0), (156, 0, 0), (99, 85, 99), (255, 239, 255), (255, 223, 0),
    (0, 0, 255), (0, 154, 0), (170, 170, 170))
# derived stand-ins: name -> (base tile, {palette colour: new colour})
TINTS = {
    "dirt": ("o0", {LGREY: (150, 100, 40)}),
    "grass": ("o0", {LGREY: (60, 200, 60)}),
    "water": ("w30", {RED: (40, 110, 255), DRED: (0, 40, 170)}),
    "deepwater": ("w30", {RED: (0, 50, 200), DRED: (0, 10, 110)}),
    "lava": ("w30", {RED: (255, 170, 0), DRED: (220, 50, 0)}),
    "fire": ("o97", {GREEN: (255, 120, 0)}),
    "cloud": ("m29", {GREEN: (200, 200, 200), RED: (130, 130, 130), YELLOW: (255, 255, 255)}),
    "sleepgas": ("o75", {RED: (0, 154, 0), GREY: (170, 170, 170)}),
    "manadrain": ("o75", {RED: (0, 0, 255), GREY: (170, 170, 170)}),
    "spikedpit": ("o4", {GREY: (156, 0, 0)}),
    "townperson": ("m26", {GREEN: (156, 110, 60)}),
    "cloak": ("o25", {GREY: (60, 60, 140)}),
    "lgloves": ("o86", {RED: (170, 120, 60), DRED: (110, 70, 30)}),
    "leatheritem": ("o25", {GREY: (150, 100, 40)}),
    "chainhood": ("o24", {BLUE: (99, 85, 99)}),
    "plateitem": ("o63", {YELLOW: (170, 170, 170)}),
    "woodshield": ("o68", {BLUE: (150, 100, 40)}),
    "steelshield": ("o68", {BLUE: (170, 170, 170)}),
    "bag": ("o44", {YELLOW: (150, 100, 40)}),
    "crate": ("o44", {YELLOW: (120, 80, 30), WHITE: (170, 120, 60)}),
    "shortsword": ("o58", {YELLOW: (170, 170, 170)}),
    "stone": ("o3", {}),
    "sbullet": ("o3", {GREY: (220, 220, 230)}),
    "arrow": ("o99", {}),
    "bolt": ("o99", {WHITE: (170, 170, 170)}),
    "bow": ("o89", {RED: (170, 120, 60), DRED: (110, 70, 30)}),
    "sling": ("o89", {RED: (150, 100, 40), DRED: (90, 60, 20)}),
    "club": ("o27", {RED: (150, 100, 40), YELLOW: (120, 80, 30)}),
}
for m in range(0, 32, 2):   # mountains: the wall autotile in grey stone
    TINTS[f"mount{m}"] = (f"w{m}", {RED: (150, 140, 130), DRED: (90, 85, 80)})

MON = {
    "MT_GIANT_BAT": "m1", "MT_GNOME": "m2", "MT_HOBGOBLIN": "m3", "MT_JACKAL": "m4",
    "MT_KOBOLD": "m5", "MT_ORC": "m6", "MT_SNAKE": "m7", "MT_CENTIPEDE": "m8",
    "MT_JACULUS": "m9", "MT_TROGLODYTE": "m10", "MT_GIANT_ANT": "m11",
    "MT_FLOATING_EYE": "m12", "MT_LEPRECHAUN": "m13", "MT_NYMPH": "m14",
    "MT_QUASIT": "m15", "MT_RUST_MONSTER": "m16", "MT_ZOMBIE": "m17",
    "MT_ASSASSIN_BUG": "m18", "MT_BUGBEAR": "m19", "MT_HELLHOUND": "m20",
    "MT_ICE_LIZARD": "m21", "MT_CENTAUR": "m22", "MT_TROLL": "m23", "MT_YETI": "m24",
    "MT_WHITE_DRAGON": "m25", "MT_ELF": "m26", "MT_GELATINOUSCUBE": "m27",
    "MT_METAMORPH": "m28", "MT_VORTEX": "m29", "MT_ZILLER": "m30",
    "MT_VIOLET_FUNGUS": "m31", "MT_WRAITH": "m32", "MT_FORVALAKA": "m33",
    "MT_LAMA_NOBE": "m34", "MT_OSQUIP": "m35", "MT_ROTHE": "m36", "MT_XORN": "m37",
    "MT_VAMPIRE": "m38", "MT_STALKER": "m39v", "MT_POLTERGEIST": "m40",
    "MT_DISENCHANTRESS": "m41", "MT_SHAMBLINGMOUND": "m42", "MT_YELLOW_MOLD": "m43",
    "MT_UMBER_HULK": "m44", "MT_GNOME_KING": "m45", "MT_MIMIC": "m46",
    "MT_WATER_LORD": "m47", "MT_BRONZE_DRAGON": "m48", "MT_GREEN_DRAGON": "m49",
    "MT_PURPLE_WORM": "m50", "MT_XVART": "m51", "MT_SPIRIT_NAGA": "m52",
    "MT_SILVER_DRAGON": "m53", "MT_PLATINUM_DRAGON": "m54", "MT_GREEN_URCHIN": "m55",
    "MT_RED_DRAGON": "m56",
    # demon lords and prince as seen (NLarn draws them when they are visible)
    "MT_DEMONLORD_I": "m57v", "MT_DEMONLORD_II": "m58v", "MT_DEMONLORD_III": "m59v",
    "MT_DEMONLORD_IV": "m60v", "MT_DEMONLORD_V": "m61v", "MT_DEMONLORD_VI": "m62v",
    "MT_DEMONLORD_VII": "m63v", "MT_DEMON_PRINCE": "m64v",
    "MT_TOWN_PERSON": "townperson",     # stand-in: the elf in townsfolk brown
}
# terrain; "wall"/"mountain" are autotiled by neighbours in port/tiles.c
LT = {
    "LT_NONE": None, "LT_MOUNTAIN": "mountain", "LT_GRASS": "grass", "LT_DIRT": "dirt",
    "LT_TREE": "o97", "LT_FLOOR": "o0", "LT_WATER": "water", "LT_DEEPWATER": "deepwater",
    "LT_LAVA": "lava", "LT_FIRE": "fire", "LT_CLOUD": "cloud", "LT_WALL": "wall",
}
SO = {
    "LS_NONE": None, "LS_ALTAR": "o1", "LS_THRONE": "o2", "LS_THRONE2": "o81",
    "LS_DEADTHRONE": "o79", "LS_STAIRSDOWN": "o13", "LS_STAIRSUP": "o5",
    "LS_ELEVATORDOWN": "o55", "LS_ELEVATORUP": "o56", "LS_FOUNTAIN": "o7",
    "LS_DEADFOUNTAIN": "o17", "LS_STATUE": "o8", "LS_URN": "o85", "LS_MIRROR": "o11",
    "LS_OPENDOOR": "o19", "LS_CLOSEDDOOR": "o20", "LS_CAVERNS_ENTRY": "o54",
    "LS_CAVERNS_EXIT": "o54", "LS_HOME": "o69", "LS_DNDSTORE": "o12",
    "LS_TRADEPOST": "o77", "LS_LRS": "o80", "LS_SCHOOL": "o10", "LS_BANK": "o16",
    "LS_BANK2": "o15", "LS_MONASTERY": "o6",   # stand-in: the Amiga sign box
}
TRAP = {
    "TT_NONE": None, "TT_ARROW": "o66", "TT_DART": "o74", "TT_TELEPORT": "o9",
    "TT_PIT": "o4", "TT_SPIKEDPIT": "spikedpit", "TT_SLEEPGAS": "sleepgas",
    "TT_MANADRAIN": "manadrain", "TT_TRAPDOOR": "o75",
}
# generic tile per item type: remembered items (the player's memory keeps the type only)
IT = {
    "IT_NONE": None, "IT_AMULET": "o45", "IT_AMMO": "arrow", "IT_ARMOUR": "o25",
    "IT_BOOK": "o43", "IT_CONTAINER": "o44", "IT_GEM": "o50", "IT_GOLD": "o18",
    "IT_POTION": "o42", "IT_RING": "o33", "IT_SCROLL": "o41", "IT_WEAPON": "o58",
}
# amulets and rings are flavoured: indexed by the game's random material
# (nlarn->amulet_material_mapping / ring_material_mapping), not by kind
AMULET_FLAVOUR = ["o45", "o46", "o47", "o48", "o49", "o86", "o87", "o101", "o22"]
RING_FLAVOUR = ["o32", "o33", "o34", "o35", "o36", "o37", "o38", "o39"]
AMMO = {"AMT_STONE": "stone", "AMT_SBULLET": "sbullet", "AMT_OARROW": "arrow",
        "AMT_ARROW": "arrow", "AMT_BOLT": "bolt"}
ARMOUR = {
    "AT_CLOAK": "cloak", "AT_LGLOVES": "lgloves", "AT_LBOOTS": "leatheritem",
    "AT_LHELMET": "leatheritem", "AT_LEATHER": "o25", "AT_WSHIELD": "woodshield",
    "AT_SLEATHER": "o61", "AT_RINGMAIL": "o60", "AT_LSHIELD": "o68",
    "AT_CHAINHOOD": "chainhood", "AT_CHAINMAIL": "o24", "AT_SPLINTMAIL": "o62",
    "AT_PHELMET": "plateitem", "AT_PBOOTS": "plateitem", "AT_PLATEMAIL": "o23",
    "AT_SSHIELD": "steelshield", "AT_SPLATEMAIL": "o64", "AT_ELVENCHAIN": "o92",
}
CONTAINER = {"CT_BAG": "bag", "CT_CASKET": "o44", "CT_CHEST": "o44", "CT_CRATE": "crate"}
GEM = {"GT_DIAMOND": "o50", "GT_RUBY": "o51", "GT_EMERALD": "o52", "GT_SAPPHIRE": "o53"}
WEAPON = {
    "WT_ODAGGER": "o31", "WT_DAGGER": "o31", "WT_SLING": "sling",
    "WT_OSHORTSWORD": "shortsword", "WT_SHORTSWORD": "shortsword",
    "WT_ESHORTSWORD": "shortsword", "WT_OSPEAR": "o30", "WT_SPEAR": "o30",
    "WT_ESPEAR": "o30", "WT_OBOW": "bow", "WT_BOW": "bow", "WT_CLUB": "club",
    "WT_MACE": "o27", "WT_FLAIL": "o59", "WT_BATTLEAXE": "o57", "WT_CROSSBOW": "bow",
    "WT_LONGSWORD": "o58", "WT_ELONGSWORD": "o58", "WT_2SWORD": "o29",
    "WT_SWORDSLASHING": "o26", "WT_LANCEOFDEATH": "o65", "WT_VORPALBLADE": "o90",
    "WT_SLAYER": "o91", "WT_SUNSWORD": "o28", "WT_BESSMAN": "o27",
}
TABLES = [  # (C array, enum, mapping)
    ("mon_tile", "mon", MON), ("lt_tile", "lt", LT), ("so_tile", "so", SO),
    ("trap_tile", "trap", TRAP), ("it_tile", "it", IT), ("ammo_tile", "ammo", AMMO),
    ("armour_tile", "armour", ARMOUR), ("container_tile", "container", CONTAINER),
    ("gem_tile", "gem", GEM), ("weapon_tile", "weapon", WEAPON),
]

names = []


def image(name):
    if name in TINTS:
        base, cmap = TINTS[name]
        t = image(base)
        px = t.load()
        for y in range(t.size[1]):
            for x in range(t.size[0]):
                r, g, b, a = px[x, y]
                for src, dst in cmap.items():
                    if max(abs(r - src[0]), abs(g - src[1]), abs(b - src[2])) < 24:
                        px[x, y] = dst + (a,)
                        break
        return t
    t = Image.open(SRC / f"{name}.png").convert("RGBA")
    if t.size != (8, 16):  # o90 (Vorpal Blade) is 13x20 upstream
        t = t.resize((8, 16), Image.NEAREST)
    return t


def add(name):
    if name is None:
        return -1
    if name == "wall":
        return -2
    if name == "mountain":
        return -3
    assert name in TINTS or (SRC / f"{name}.png").exists(), f"no tile {name}"
    if name not in names:
        names.append(name)
    return names.index(name)


out = {}
for arr, key, mapping in TABLES:
    ids = ENUMS[key]
    missing = [n for n in ids if n not in mapping]
    extra = [n for n in mapping if n not in ids]
    assert not missing, f"{arr}: no tile for {missing}"
    assert not extra, f"{arr}: unknown ids {extra}"
    out[arr] = [add(mapping[n]) for n in ids]
assert len(AMULET_FLAVOUR) == len(ENUMS["amulet"]) and len(RING_FLAVOUR) == len(ENUMS["ring"])
out["amulet_flavour_tile"] = [add(n) for n in AMULET_FLAVOUR]
out["ring_flavour_tile"] = [add(n) for n in RING_FLAVOUR]
# one Amiga picture each for every potion, scroll and book (Larn has one)
out["potion_tile"] = [add("o42")] * len(ENUMS["potion"])
out["scroll_tile"] = [add("o41")] * len(ENUMS["scroll"])
out["book_tile"] = [add("o43")] * len(ENUMS["book"])
wall = [add(f"w{m}") for m in range(0, 32, 2)]
mountain = [add(f"mount{m}") for m in range(0, 32, 2)]
extra = {"PLAYER_TILE": add("player"), "SPHERE_TILE": add("o82")}

n = sum(len(v) for v in ENUMS.values())
print(f"coverage: {n}/{n} ids (100%): " + ", ".join(f"{k} {len(v)}" for k, v in ENUMS.items()))
print(f"{len(names)} tiles")

rows = (len(names) + PER_ROW - 1) // PER_ROW
sheet = Image.new("RGBA", (PER_ROW * 8, rows * 16))
for i, name in enumerate(names):
    sheet.paste(image(name), ((i % PER_ROW) * 8, (i // PER_ROW) * 16))
sheet.save("web/tiles.png")


def carr(name, a):
    return f"static const short {name}[{len(a)}] = {{{','.join(map(str, a))}}};\n"


h = ["/* Generated by port/mktiles.py from the Amiga Larn tiles (web/tiles.png,\n"
     f" * 8x16, {PER_ROW} per row). Do not edit. -1 = nothing, -2 = wall, -3 = mountain\n"
     " * (autotiled by neighbours: wall_tile / mountain_tile, bits 1 up 2 right\n"
     " * 4 down 8 left). Arrays are indexed by the game's enums. */\n"]
for k, v in out.items():
    h.append(carr(k, v))
h.append(carr("wall_tile", wall))
h.append(carr("mountain_tile", mountain))
for k, v in extra.items():
    h.append(f"#define {k} {v}\n")
h.append(f"#define TILES_PER_ROW {PER_ROW}\n#define TILE_COUNT {len(names)}\n")
Path("port/tilemap.h").write_text("".join(h))
