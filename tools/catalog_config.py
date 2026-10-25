"""KairoST's catalog settings: Atari ST floppy disk media, artwork references, the platform's
slugs and the machine a disk's name asks for."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "shared/tools"))
import content_ids
from launchbox_artwork import References

APP = "kairost"
PLATFORMS = ("atarist",)
ARTWORK_CODE = 83
ARTWORK = References("KairoST", (ARTWORK_CODE,))

# The floppy images Hatari reads and the programs it starts from their folder (MainActivity's
# DiskImageLibrary formats).
DISK = {".st", ".msa", ".stx", ".dim"}
PROGRAM = {".prg", ".tos", ".ttp", ".app", ".gtp"}
LIMITS = {"fd": 16 * 1024**2, "prg": 16 * 1024**2}

KINDS = set(LIMITS)


def kind(name):
    suffix = Path(name).suffix.lower()
    return "fd" if suffix in DISK else "prg" if suffix in PROGRAM else None


MEDIA = content_ids.Media(kind, content_ids.same_file, LIMITS.__getitem__, 1)

MOBYGAMES = "atarist"
# The standard set: TOSEC 2012-04-23's Games folder (its [ST], [STX] and Public Domain sets).
# Its other folders (applications, demos, compilations, coverdisks and so on) are not catalogued.
# "extra" is the top level of roms/ (scan_media.py --top-level): common dumps outside TOSEC that
# the device and UI tests boot, GOAL.MSA among them.
SCANS = {"atarist": (".downloads/scan/atarist.json", ("atarist",)),
         "extra": (".downloads/scan/extra.json", ("atarist",))}

# The settings a game can ask for, as StCatalog's options name them.
MACHINE_OPTIONS = {
    "machine": {"st", "megast", "ste", "megaste", "tt", "falcon"},
    "memory": {512, 1024, 2048, 4096},
    "monitor": {"rgb", "mono"},
    "region": {"uk", "us"},
    "tos": {"emutos", "tos"},
}

# Disks that fail on EmuTOS and run on the player's TOS: {content ID: what goes wrong on EmuTOS}.
_TOS = json.loads((ROOT / "catalog/research/tos-disks-v1.json").read_text(encoding="utf-8"))["disks"]

# How each floppy starts, read from the disk itself (tools/boot_disks.py): boot "sector", "auto",
# "desktop" (programs in its top folder only) or "data", with its top folder's programs.
_BOOT = json.loads((ROOT / "catalog/research/boot-disks-v1.json").read_text(encoding="utf-8"))["disks"]

# Disks that end on the GEM desktop on EmuTOS and on the player's TOS (tools/tos_pass.py), even
# where their boot sector is executable: the game is a program a player would open there.
_DESKTOP = json.loads((ROOT / "catalog/research/desktop-disks-v1.json").read_text(encoding="utf-8"))["disks"]

# Programs that are not the game: installers, setup, music and document viewers, editors, cheats.
_NOT_GAME_PROGRAM = re.compile(r"INSTAL|SETUP|CONFIG|MUSIC|DOC|DOX|README|LISEZ|EDIT|CHT|CHEAT|DEMO|PRINT", re.I)


def DATA_DISK(content_id):
    """A floppy that never starts a game: no executable boot sector, no AUTO programs and no
    programs at all in its top folder (boot-disks-v1.json "data"): game data read sector by sector,
    or an empty disk. shared disk_sets never starts a set from one. An STX image is an original that
    may start from a loader its file system does not show, so it is never taken for a data disk."""
    record = _BOOT.get(content_id, {})
    return record.get("boot") == "data" and record.get("image") != "stx"


def _program(content_id, name):
    """The program on a floppy that TOS must start, or None: for a disk that boots to the desktop
    (its boot sector starts nothing), the one program in its top folder that is the game, or in its
    other folders when the top folder has none. The core
    starts a lone program itself on plain (.st, .msa) images that do not boot; an STX image, a disk
    with several programs, or one whose executable boot sector leads to the desktop needs it named."""
    record = _BOOT.get(content_id)
    if not record or not (record["boot"] == "desktop" or content_id in _DESKTOP):
        return None
    # A name TOS could not be asked to start (a character a path cannot hold) is left to the player.
    def game_programs(programs):
        return [program for program in programs
                if not _NOT_GAME_PROGRAM.search(program.rsplit("\\", 1)[-1].rsplit(".", 1)[0])
                and re.fullmatch(LAUNCH_FIELDS["program"], program)]
    # The top folder's programs first; a disk with none there starts the one in its other folders.
    games = game_programs(record.get("programs", [])) or game_programs(record.get("folders", []))
    if len(games) != 1:
        return None
    automatic = (record["boot"] == "desktop" and len(record.get("programs", [])) == 1
                 and Path(name).suffix.lower() in (".st", ".msa", ".dim"))
    return None if automatic else games[0]


# The launch fields a disk's record may carry (shared app_catalog): the program TOS starts.
LAUNCH_FIELDS = {"program": r"(?:[A-Za-z0-9_\-!~#$%&@^'(){}]{1,8}(?:\.[A-Za-z0-9_\-]{1,3})?\\)*"
                            r"[A-Za-z0-9_\-!~#$%&@^'(){}]{1,8}\.[A-Za-z]{1,3}"}

_STE = re.compile(r"\(STE\)|\[STE\]")
_FALCON = re.compile(r"\[Falcon only\]|\(Falcon\)", re.I)
_MONO = re.compile(r"\[monochrome\]|\[mono\]", re.I)
_MEMORY = re.compile(r"\[([124])\s*MB\]", re.I)


def MACHINE(name, set_name, content_id):
    """The machine a disk's TOSEC name asks for: "(STE)" an STE, "[Falcon only]" a Falcon (with
    4 MB, its usual memory), "[monochrome]" the mono monitor, "[2Mb]" or "[4MB]" that much memory.
    A disk tos-disks-v1.json lists starts on the player's TOS; a disk that boots to the desktop
    starts its game's program (launch.program, _program)."""
    settings = {"tos": "tos"} if content_id in _TOS else {}
    if _FALCON.search(name):
        settings.update(machine="falcon", memory=4096)
    elif _STE.search(name):
        settings["machine"] = "ste"
    if _MONO.search(name):
        settings["monitor"] = "mono"
    memory = _MEMORY.search(name)
    if memory and memory.group(1) != "1":
        settings["memory"] = int(memory.group(1)) * 1024
    program = _program(content_id, name)
    if program:
        settings["launch"] = {"program": program}
    return settings


# Disk sets (shared/tools/disk_sets.py): a game boots from its first floppy, with its second in drive B.
BOOT_KINDS = ["fd"]
COMPANION = {"fd": ("fd", "floppyB", False)}
