#!/usr/bin/env python3
"""Writes core/test/st/timing.st, the shared PC boot test's clock program for KairoST (KairoST#84):
a 720 KB FAT12 floppy holding TIMING.TOS, 68000 code that counts 2,000,000 down, prints
KAIRO TIMING DONE through GEMDOS (Cconws) and then waits for good. The table starts it as the
game's Startup program ("program": "TIMING.TOS").

    make_timing_st.py
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COUNT = 2_000_000
MESSAGE = b"KAIRO TIMING DONE\r\n\0"


def code():
    text = bytearray()
    text += bytes([0x20, 0x3C]) + COUNT.to_bytes(4, "big")   # 0  move.l #COUNT, d0
    text += bytes([0x53, 0x80])                              # 6  subq.l #1, d0
    text += bytes([0x66, 0xFC])                              # 8  bne.s 6
    text += bytes([0x48, 0x7A, 0x00, 0x0C])                  # 10 pea message(pc): 24 - 12
    text += bytes([0x3F, 0x3C, 0x00, 0x09])                  # 14 move.w #9, -(sp) (Cconws)
    text += bytes([0x4E, 0x41])                              # 18 trap #1
    text += bytes([0x5C, 0x8F])                              # 20 addq.l #6, sp
    text += bytes([0x60, 0xFE])                              # 22 bra.s 22
    if len(text) != 24:
        raise SystemExit(f"the message is at {len(text)}, not 24")
    text += MESSAGE
    if len(text) % 2:
        text += b"\0"
    # GEMDOS program header: magic, text, data, bss, symbols, reserved, flags, absolute flag.
    header = (0x601A).to_bytes(2, "big") + len(text).to_bytes(4, "big") + bytes(4 * 4) + bytes(4) + bytes(2)
    # No relocations: a zero longword after the text.
    return header + bytes(text) + bytes(4)


def floppy(name, data):
    sector = 512
    image = bytearray(1440 * sector)
    # Boot sector: 512-byte sectors, 2 per cluster, 1 reserved, 2 FATs, 112 root entries,
    # 1440 sectors, media F9, 3 sectors per FAT, 9 per track, 2 sides.
    boot = bytearray(sector)
    boot[0:2] = b"\x60\x38"
    boot[11:13] = (512).to_bytes(2, "little")
    boot[13] = 2
    boot[14:16] = (1).to_bytes(2, "little")
    boot[16] = 2
    boot[17:19] = (112).to_bytes(2, "little")
    boot[19:21] = (1440).to_bytes(2, "little")
    boot[21] = 0xF9
    boot[22:24] = (3).to_bytes(2, "little")
    boot[24:26] = (9).to_bytes(2, "little")
    boot[26:28] = (2).to_bytes(2, "little")
    image[0:sector] = boot
    clusters = (len(data) + 2 * sector - 1) // (2 * sector)
    if clusters != 1:
        raise SystemExit(f"{name} takes {clusters} clusters, not 1")
    # Both FATs: media, then cluster 2 the file's last.
    for fat in (1, 4):
        image[fat * sector:fat * sector + 5] = bytes([0xF9, 0xFF, 0xFF, 0xFF, 0x0F])
    # Root directory (sector 7): the file at cluster 2.
    stem, ext = name.split(".")
    entry = bytearray(32)
    entry[0:11] = (stem.ljust(8) + ext.ljust(3)).encode("ascii")
    entry[11] = 0x20
    entry[26:28] = (2).to_bytes(2, "little")
    entry[28:32] = len(data).to_bytes(4, "little")
    image[7 * sector:7 * sector + 32] = entry
    # Data (sector 14): cluster 2.
    image[14 * sector:14 * sector + len(data)] = data
    return bytes(image)


def main():
    target = ROOT / "core/test/st/timing.st"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(floppy("TIMING.TOS", code()))
    print(f"{target}: {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
