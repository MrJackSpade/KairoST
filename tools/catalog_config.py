"""KairoST's catalog settings: Atari ST floppy disk media, artwork references, the platform's
slugs and the machine a disk's name asks for."""
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
SCANS = {"atarist": (".downloads/scan/atarist.json", ("atarist",))}

# The settings a game can ask for, as StCatalog's options name them.
MACHINE_OPTIONS = {
    "machine": {"st", "megast", "ste", "megaste", "tt", "falcon"},
    "memory": {"512", "1024", "2048", "4096"},
    "monitor": {"rgb", "mono"},
    "region": {"uk", "us"},
    "tos": {"emutos", "tos"},
}

_STE = re.compile(r"\(STE\)|\[STE\]")
_FALCON = re.compile(r"\[Falcon only\]|\(Falcon\)", re.I)
_MONO = re.compile(r"\[monochrome\]|\[mono\]", re.I)
_MEMORY = re.compile(r"\[([124])\s*MB\]", re.I)


def MACHINE(name, set_name, content_id):
    """The machine a disk's TOSEC name asks for: "(STE)" an STE, "[Falcon only]" a Falcon (with
    4 MB, its usual memory), "[monochrome]" the mono monitor, "[2Mb]" or "[4MB]" that much memory."""
    settings = {}
    if _FALCON.search(name):
        settings.update(machine="falcon", memory="4096")
    elif _STE.search(name):
        settings["machine"] = "ste"
    if _MONO.search(name):
        settings["monitor"] = "mono"
    memory = _MEMORY.search(name)
    if memory and memory.group(1) != "1":
        settings["memory"] = str(int(memory.group(1)) * 1024)
    return settings


# Disk sets (shared/tools/disk_sets.py): a game boots from its first floppy, with its second in drive B.
BOOT_KINDS = ["fd"]
COMPANION = {"fd": ("fd", "floppyB", False)}
