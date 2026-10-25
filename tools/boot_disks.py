#!/usr/bin/env python3
"""Tell from each catalog floppy itself how it starts (KairoST#37).

Usage: boot_disks.py [--workers N]

Reads every floppy the scans list (tools/catalog_config.SCANS: .st and .msa images directly, .stx
images once their sectors are decoded) and records how an ST starts it in
catalog/research/boot-disks-v1.json, by content ID:

  {"boot": "sector"}    its boot sector is executable (its big-endian words add up to 0x1234): the
                        ST runs it when it boots from the disk
  {"boot": "auto"}      its AUTO folder holds programs, which TOS runs as it boots
  {"boot": "desktop"}   neither, but it holds programs: TOS boots to the desktop, where a player opens
                        one; "programs" lists those in its top folder, "folders" those in others
  {"boot": "data"}      none of these, including a disk whose file system is empty or not FAT (game
                        data a game reads sector by sector): never the disk a game starts from
  {"boot": "unreadable", "why": ...}   the image could not be decoded

Every record names its image format ("image": st, msa, stx or dim). STX images are originals,
often copy protected: one can start from a loader no file system shows (Black Tiger's Disk 1 reads
as an empty disk with a plain boot sector), so their "data" is a reading, not a certainty.

Every record but "unreadable" also lists the top folder's programs ("programs") and the AUTO
folder's ("auto"). Programs are .PRG, .TOS, .TTP, .APP and .GTP files, as the core names them
(core/kairo/main.c autostart_floppy_program).
"""
import argparse
import json
import struct
import sys
import zipfile
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import catalog_config

OUT = ROOT / "catalog/research/boot-disks-v1.json"
PROGRAMS = {"PRG", "TOS", "TTP", "APP", "GTP"}
SECTOR = 512


class Unreadable(Exception):
    pass


def msa_sectors(data):
    """The sectors of an MSA image, in logical order."""
    magic, per_track, sides, first, last = struct.unpack(">5H", data[:10])
    if magic != 0x0E0F:
        raise Unreadable("not an MSA image")
    out, at = bytearray(), 10
    size = per_track * SECTOR
    for _ in range(first, last + 1):
        for _ in range(sides + 1):
            length = struct.unpack(">H", data[at:at + 2])[0]
            at += 2
            block = data[at:at + length]
            at += length
            if length == size:
                track = bytearray(block)
            else:
                track, i = bytearray(), 0
                while i < len(block):
                    if block[i] == 0xE5:
                        value, count = block[i + 1], struct.unpack(">H", block[i + 2:i + 4])[0]
                        track += bytes([value]) * count
                        i += 4
                    else:
                        track.append(block[i])
                        i += 1
            if len(track) != size:
                raise Unreadable(f"an MSA track unpacks to {len(track)} bytes, not {size}")
            out += track
    # Tracks before the first are missing from the image: the boot sector is on track 0.
    if first != 0:
        raise Unreadable(f"the MSA image starts at track {first}")
    return bytes(out)


def stx_sectors(data):
    """{(track, side, sector number): data} of an STX (Pasti) image's 512-byte sectors."""
    if data[:4] != b"RSY\0":
        raise Unreadable("not an STX image")
    tracks = data[10]
    at = 16
    sectors = {}
    for _ in range(tracks):
        block, fuzzy, count, flags, _mfm, number, _type = struct.unpack("<IIHHHBB", data[at:at + 16])
        if block < 16:
            raise Unreadable("an STX track block is shorter than its header")
        track, side = number & 0x7F, number >> 7
        p = at + 16
        if count and not flags & 0x01:
            # Only sector data: count standard sectors numbered from 1.
            for n in range(count):
                sectors[(track, side, n + 1)] = data[p + n * SECTOR:p + (n + 1) * SECTOR]
        elif count:
            track_data = p + count * 16 + fuzzy
            for n in range(count):
                offset, _bit, _time, _id_track, _id_side, id_sector, id_size, _crc, status, _r = struct.unpack(
                    "<IHHBBBBHBB", data[p + n * 16:p + n * 16 + 16])
                if status & 0x10 or (128 << (id_size & 3)) != SECTOR:
                    continue  # no data, or not a 512-byte sector
                sectors.setdefault((track, side, id_sector), data[track_data + offset:track_data + offset + SECTOR])
        at += block
    return sectors


def image_geometry(boot, size):
    """(sectors per track, sides) of a plain image's layout, as Hatari guesses it
    (floppy.c Floppy_FindDiskDetails): the boot sector's, unless they disagree with the size."""
    per_track = boot[24] | boot[25] << 8
    sides = boot[26] | boot[27] << 8
    total = boot[19] | boot[20] << 8
    if total == size // SECTOR and 1 <= sides <= 2 and 1 <= per_track <= 48:
        return per_track, sides
    sides = 1 if size < 500 * 1024 else 2
    sectors = size // SECTOR
    for spt in (9, 10, 11, 12):
        if any(sectors == tracks * spt * sides for tracks in range(80, 85)):
            return spt, sides
    if 5 <= per_track <= 48:
        return per_track, sides
    raise Unreadable(f"no floppy geometry fits {size} bytes")


def plain_reader(data):
    """read(track, side, sector number) of a plain (track by track, side by side) image."""
    per_track, sides = image_geometry(data[:SECTOR], len(data))
    def read(track, side, sector):
        at = ((track * sides + side) * per_track + sector - 1) * SECTOR
        return data[at:at + SECTOR] if sector <= per_track and at + SECTOR <= len(data) else bytes(SECTOR)
    return read


def stx_reader(sectors):
    """read(track, side, sector number) of an STX image's sector map. A sector the image does not
    hold reads as zeros: an image without a boot sector (only track images, as some copy-protected
    data disks are) cannot boot, and is a data disk."""
    return lambda track, side, sector: sectors.get((track, side, sector), bytes(SECTOR))


def directory(logical, first_sector, count, cluster_sectors, fat, cluster=None):
    """[(name, extension, attributes, first cluster)] of a FAT12 folder: the root's sectors, or a
    folder's cluster chain."""
    entries = []
    def read(sector_list):
        for sector in sector_list:
            block = logical(sector)
            for i in range(0, len(block) - 31, 32):
                entry = block[i:i + 32]
                if entry[0] == 0:
                    return
                if entry[0] == 0xE5 or entry[11] & 0x08:
                    continue
                name = entry[:8].decode("latin-1").rstrip()
                ext = entry[8:11].decode("latin-1").rstrip()
                entries.append((name, ext, entry[11], entry[26] | entry[27] << 8))
    if cluster is None:
        read(range(first_sector, first_sector + count))
    else:
        seen = set()
        sector_list = []
        while 2 <= cluster < 0xFF0 and cluster not in seen:
            seen.add(cluster)
            start = first_sector + (cluster - 2) * cluster_sectors
            sector_list.extend(range(start, start + cluster_sectors))
            cluster = fat(cluster)
        read(sector_list)
    return entries


def classify(read):
    """The record of one disk, by read(track, side, sector number). TOS finds a logical sector by the
    geometry the boot sector names, whatever the image's own layout."""
    disk = read(0, 0, 1)
    checksum = sum(struct.unpack(">256H", disk)) & 0xFFFF
    per_track = disk[24] | disk[25] << 8
    sides = disk[26] | disk[27] << 8

    def logical(n):
        track, rest = divmod(n, per_track * sides)
        side, sector = divmod(rest, per_track)
        return read(track, side, sector + 1)

    record = {"programs": [], "auto": [], "folders": []}
    sector = disk[11] | disk[12] << 8
    cluster_sectors = disk[13]
    # TOS takes the FATs to start after the boot sector, whatever the reserved count says (many disks
    # say 0).
    reserved = max(disk[14] | disk[15] << 8, 1)
    fats = disk[16]
    entries = disk[17] | disk[18] << 8
    fat_sectors = disk[22] | disk[23] << 8
    if (sector == SECTOR and cluster_sectors and reserved and fats and entries and fat_sectors
            and 1 <= per_track <= 48 and sides in (1, 2)):
        fat_bytes = b"".join(logical(reserved + i) for i in range(fat_sectors))
        def fat(cluster):
            at = cluster * 3 // 2
            if at + 1 >= len(fat_bytes):
                return 0xFFF
            value = fat_bytes[at] | fat_bytes[at + 1] << 8
            return value >> 4 if cluster & 1 else value & 0xFFF
        root_start = reserved + fats * fat_sectors
        root_count = (entries * 32 + SECTOR - 1) // SECTOR
        data_start = root_start + root_count
        root = directory(logical, root_start, root_count, cluster_sectors, fat)
        record["programs"] = sorted(f"{name}.{ext}" for name, ext, attr, _ in root
                                    if ext.upper() in PROGRAMS and not attr & 0x10 and name)
        auto = next((first for name, ext, attr, first in root if attr & 0x10 and name.upper() == "AUTO" and not ext), None)
        if auto is not None:
            record["auto"] = sorted(f"{name}.{ext}" for name, ext, attr, _ in
                                    directory(logical, data_start, 0, cluster_sectors, fat, auto)
                                    if ext.upper() in PROGRAMS and not attr & 0x10 and name)
        # Programs in other folders, which a player opens on the desktop: "FOLDER\\GAME.PRG".
        seen = set()
        def walk(entries, path, depth):
            for name, ext, attr, first in entries:
                full = f"{path}{name}{'.' + ext if ext else ''}"
                if attr & 0x10 and name not in (".", "..") and name and first not in seen and depth < 4:
                    if not path and name.upper() == "AUTO" and not ext:
                        continue
                    seen.add(first)
                    walk(directory(logical, data_start, 0, cluster_sectors, fat, first), full + "\\", depth + 1)
                elif path and not attr & 0x10 and ext.upper() in PROGRAMS and name:
                    record["folders"].append(full)
        walk(root, "", 0)
        record["folders"].sort()
    record["boot"] = ("sector" if checksum == 0x1234 else "auto" if record["auto"] else
                      "desktop" if record["programs"] or record["folders"] else "data")
    return record


def read_disk(job):
    """(content ID, record) of one disk in an archive."""
    content_id, archive, member = job
    try:
        if archive.suffix.lower() == ".zip":
            with zipfile.ZipFile(archive) as z:
                data = z.read(member)
            name = member
        else:
            data = archive.read_bytes()
            name = archive.name
        suffix = Path(name).suffix.lower()
        if suffix == ".msa":
            read = plain_reader(msa_sectors(data))
        elif suffix == ".stx":
            read = stx_reader(stx_sectors(data))
        elif suffix in (".st", ".dim"):
            # A .dim image starts with a 32-byte header.
            read = plain_reader(data[32:] if suffix == ".dim" else data)
        else:
            raise Unreadable(f"not a floppy image: {name}")
        return content_id, {**classify(read), "image": suffix[1:]}
    except (Unreadable, struct.error, IndexError) as error:
        return content_id, {"boot": "unreadable", "why": str(error) or type(error).__name__}


def jobs():
    for _, (index_path, _) in catalog_config.SCANS.items():
        index = json.loads((ROOT / index_path).read_text(encoding="utf-8"))
        for archive in index["archives"]:
            if archive["status"] != "ready":
                continue
            for disk in archive["disks"]:
                if disk["contentId"].startswith("sha256-fd-"):
                    yield disk["contentId"], Path(index["root"]) / archive["path"], disk.get("entry")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--workers", type=int, default=8)
    args = parser.parse_args()
    unique = {}
    for content_id, archive, member in jobs():
        unique.setdefault(content_id, (content_id, archive, member))
    with ProcessPoolExecutor(args.workers) as pool:
        disks = dict(pool.map(read_disk, unique.values(), chunksize=32))
    OUT.write_text(json.dumps({"schemaVersion": 1, "disks": dict(sorted(disks.items()))}, indent=1, sort_keys=True)
                   + "\n", encoding="utf-8")
    counts = {}
    for record in disks.values():
        counts[record["boot"]] = counts.get(record["boot"], 0) + 1
    print(f"{len(disks)} floppies: {counts}")


if __name__ == "__main__":
    main()
