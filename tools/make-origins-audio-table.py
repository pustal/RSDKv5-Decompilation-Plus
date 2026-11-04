#!/usr/bin/env python3
"""
Writes RSDKv5/RSDK/Plus/PlusOriginsTable.hpp: which Sonic Origins sound
(a CRIWARE cue name) each retro game plays for each file it asks for
("Global/Jump" -> "SFX_A0_Jump"). The Plus module's converter
(Plus/PlusOrigins.cpp) uses it to turn the player's Sonic Origins audio into
data packs the engine plays.

Sonic Origins keeps that table in its reflection parameters,
retro_rfl_param.rfl inside raw/rfl/rfl_resident.pac. This tool reads the
player's own copy of that file and writes only the names: no game data goes
into the header.

    pip install lz4
    tools/make-origins-audio-table.py <Sonic Origins>/image/x64/raw/rfl/rfl_resident.pac

The .pac is a "PACx403" archive: its data is cut in LZ4 blocks, which joined
make a "PACx402" archive holding the .rfl files, each a "BINA" file. In
retro_rfl_param the table is two runs of fixed-size records, each string a
64-bit offset into the file's string table:
  - sound effects, 48 bytes: RSDK name (no extension), cue, and the name of a
    music record when the effect is a jingle ("Global/1Up" -> "1up");
  - music, 40 bytes: RSDK file name, cue, playback speed (1.2 for the
    speed-shoes versions, "_F").
Each run lists the games in turn (Sonic 1, Sonic 2, Sonic 3 & Knuckles, Sonic
CD), each game's names in order; a game's list starts again at "Global/1Up"
(sound effects) or "ActComplete.ogg" (music), Sonic 3 & Knuckles' at its first
"3K" name and Sonic CD's music at "R1B.ogg".
"""
import struct
import sys
from pathlib import Path

try:
    import lz4.block
except ImportError:
    sys.exit("make-origins-audio-table.py needs the lz4 module: pip install lz4")

OUTPUT = Path(__file__).resolve().parent.parent / "RSDKv5" / "RSDK" / "Plus" / "PlusOriginsTable.hpp"
GAMES = ["sonic1", "sonic2", "sonic3k", "soniccd"]
# The engine plays Sonic 1, Sonic 2 and Sonic CD.
WANTED = {"sonic1": "kOriginsSonic1", "sonic2": "kOriginsSonic2", "soniccd": "kOriginsSonicCd"}
SFX_RECORD = 48
MUSIC_RECORD = 40


def unpack_pac(data: bytes) -> bytes:
    """The PACx402 archive inside a PACx403 one."""
    if data[:8] != b"PACx403L":
        sys.exit("not a Sonic Origins .pac (PACx403L)")
    root_offset, = struct.unpack_from("<I", data, 0x10)
    chunk_count, = struct.unpack_from("<I", data, 0x30)
    out, pos = bytearray(), root_offset
    for c in range(chunk_count):
        compressed, uncompressed = struct.unpack_from("<II", data, 0x34 + c * 8)
        out += lz4.block.decompress(data[pos:pos + compressed], uncompressed_size=uncompressed)
        pos += compressed
    return bytes(out)


def read_records(archive: bytes):
    """The sound and music records of retro_rfl_param, in file order."""
    marker = archive.find(b"Global/HammerJump\0")
    bina = archive.rfind(b"BINA210L", 0, marker)
    if marker < 0 or bina < 0:
        sys.exit("retro_rfl_param not found in the archive")
    node = bina + 16
    if archive[node:node + 4] != b"DATA":
        sys.exit("unexpected BINA layout")
    _, strings, strings_size, _, extra = struct.unpack_from("<IIIIH", archive, node + 4)
    base = node + 24 + extra

    def text(offset):
        if not strings <= offset < strings + strings_size:
            return None
        end = archive.index(b"\0", base + offset)
        return archive[base + offset:end].decode("utf-8")

    def u64(at):
        return struct.unpack_from("<Q", archive, base + at)[0]

    sounds, music, pos = [], [], 0
    while pos + SFX_RECORD <= strings:
        name = text(u64(pos))
        if name and name.endswith(".ogg"):
            break
        sounds.append((name, text(u64(pos + 16)), text(u64(pos + 32))))
        pos += SFX_RECORD
    while pos + MUSIC_RECORD <= strings:
        name = text(u64(pos))
        if name is None:
            break
        music.append((name, text(u64(pos + 16)), struct.unpack_from("<f", archive, base + pos + 32)[0]))
        pos += MUSIC_RECORD
    return sounds, music


def split_games(records, starts_game):
    """Cuts a run of records into the games' lists (see the file comment)."""
    games, previous = [[]], None
    for record in records:
        if games[-1] and starts_game(record[0], previous):
            games.append([])
        games[-1].append(record)
        previous = record[0]
    if len(games) != len(GAMES):
        sys.exit(f"expected {len(GAMES)} games' lists, found {len(games)}: the file's layout changed")
    return dict(zip(GAMES, games))


def sfx_start(name, previous):
    return name == "Global/1Up" or (name or "").startswith("3K") and not (previous or "").startswith("3K")


def music_start(name, previous):
    return name in ("ActComplete.ogg", "R1B.ogg") or name.startswith("3K/") and not (previous or "").startswith("3K/")


def c_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sounds, music = read_records(unpack_pac(Path(sys.argv[1]).read_bytes()))
    sound_games = split_games(sounds, sfx_start)
    music_games = split_games(music, music_start)

    lines = [
        "// Generated by tools/make-origins-audio-table.py from Sonic Origins' retro_rfl_param.rfl: do not edit.",
        "// Names only: which Sonic Origins cue each retro game plays for each file it asks for.",
        "",
        "#ifndef PLUS_ORIGINS_TABLE_H",
        "#define PLUS_ORIGINS_TABLE_H",
        "",
        "namespace RSDK",
        "{",
        "namespace Plus",
        "{",
        "",
        "/** A file a retro game asks for and the Sonic Origins cue that plays for it. */",
        "struct OriginsSound {",
        "    bool music;       ///< Data/Music/<path> (else Data/SoundFX/<path>.wav).",
        "    const char *path; ///< As the game asks for it.",
        "    const char *cue;  ///< The cue's name in Sonic Origins' .acb files.",
        "};",
        "",
    ]
    music_cues = {}
    for game in GAMES:
        for name, cue, _ in music_games[game]:
            music_cues.setdefault(game, {})[name] = cue

    for game, symbol in WANTED.items():
        entries, seen = [], set()
        for name, cue, jingle in sound_games[game]:
            if not name or not cue or name.startswith("_") or "xxxxx" in name:
                continue  # Placeholders and entries only Sonic Origins' own menus use.
            if jingle:  # An effect that is a music jingle ("Global/1Up" -> "1up" -> "88_1up").
                cue = music_cues[game].get(jingle, cue)
            key = (False, name.lower())
            if key not in seen:
                seen.add(key)
                entries.append((False, name, cue))
        for name, cue, _speed in music_games[game]:
            if not name.lower().endswith(".ogg") or not cue or cue.startswith("_BGM_"):
                continue  # Jingle records ("1up") and the 2013 menus' music, which Sonic Origins lacks.
            key = (True, name.lower())
            if key not in seen:
                seen.add(key)
                entries.append((True, name, cue))
        lines.append(f"/** {game}: {len(entries)} files. */")
        lines.append(f"const OriginsSound {symbol}[] = {{")
        for is_music, name, cue in entries:
            lines.append(f"    {{ {'true' if is_music else 'false'}, {c_string(name)}, {c_string(cue)} }},")
        lines.append("};")
        lines.append("")
    lines += ["} // namespace Plus", "} // namespace RSDK", "", "#endif // PLUS_ORIGINS_TABLE_H", ""]
    OUTPUT.write_text("\n".join(lines))
    print(f"wrote {OUTPUT}")


if __name__ == "__main__":
    main()
