"""KairoST's catalog settings: Atari ST floppy disk media, artwork references and the platform's
slugs."""
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

# The floppy images Hatari reads (MainActivity's DiskImageLibrary formats).
DISK = {".st", ".msa", ".stx", ".dim"}
LIMITS = {"fd": 16 * 1024**2}

KINDS = set(LIMITS)


def kind(name):
    return "fd" if Path(name).suffix.lower() in DISK else None


MEDIA = content_ids.Media(kind, content_ids.same_file, LIMITS.__getitem__, 1)

MOBYGAMES = "atarist"
SCANS = {}

# The settings a game can ask for, as StCatalog's options name them.
MACHINE_OPTIONS = {
    "machine": {"st", "megast", "ste", "megaste", "tt", "falcon"},
    "memory": {"512", "1024", "2048", "4096"},
    "monitor": {"rgb", "mono"},
    "tos": {"emutos", "tos"},
}

# Disk sets (shared/tools/disk_sets.py): a game starts from its first disk; other disks are swapped in the game.
BOOT_KINDS = ["fd"]
COMPANION = {}
