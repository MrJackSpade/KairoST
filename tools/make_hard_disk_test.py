#!/usr/bin/env python3
"""Writes the shared PC boot test's hard disk media for KairoST (KairoST#85):

- core/test/st/hdtest.img: a 16 MB hard disk image, an MBR with one empty FAT16 partition, which
  EmuTOS mounts as C: on the model's bus (ACSI on an ST).
- core/test/st/hdtools.st: a 720 KB floppy with WRITE.TOS, which writes KAIRO to C:\\KAIRO.TXT, and
  READ.TOS, which prints what C:\\KAIRO.TXT holds. KairoST types no commands, so the table starts
  each as the Startup program.

    make_hard_disk_test.py
"""
from pathlib import Path

from make_timing_st import floppy_files

ROOT = Path(__file__).resolve().parents[1]
SECTOR = 512


class Program:
    """68000 code with labels, assembled into a GEMDOS program (no relocations: PC-relative only)."""

    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []  # (offset of the displacement word, label): disp = label - offset

    def emit(self, *words):
        for word in words:
            self.code += word.to_bytes(2, "big")

    def pea(self, label):
        self.emit(0x487A)
        self.fixups.append((len(self.code), label))
        self.emit(0)

    def label(self, name):
        self.labels[name] = len(self.code)

    def data(self, name, data):
        self.label(name)
        self.code += data
        if len(self.code) % 2:
            self.code += b"\0"

    def gemdos(self, function, stack):
        self.emit(0x3F3C, function)  # move.w #function, -(sp)
        self.emit(0x4E41)            # trap #1
        stack += 2
        if stack <= 8:
            self.emit(0x508F | ((stack & 7) << 9))  # addq.l #stack, sp
        else:
            self.emit(0x4FEF, stack)                # lea stack(sp), sp

    def build(self):
        for offset, label in self.fixups:
            self.code[offset:offset + 2] = (self.labels[label] - offset).to_bytes(2, "big", signed=True)
        text = bytes(self.code)
        header = (0x601A).to_bytes(2, "big") + len(text).to_bytes(4, "big") + bytes(4 * 4) + bytes(4) + bytes(2)
        return header + text + bytes(4)


def write_program():
    p = Program()
    p.emit(0x4267)                    # clr.w -(sp): attributes
    p.pea("name")
    p.gemdos(0x3C, 6)                 # Fcreate(name, 0)
    p.emit(0x3E00)                    # move.w d0, d7: the handle
    p.pea("text")
    p.emit(0x2F3C, 0, 7)              # move.l #7, -(sp)
    p.emit(0x3F07)                    # move.w d7, -(sp)
    p.gemdos(0x40, 10)                # Fwrite(handle, 7, text)
    p.emit(0x3F07)                    # move.w d7, -(sp)
    p.gemdos(0x3E, 2)                 # Fclose(handle)
    p.pea("done")
    p.gemdos(0x09, 4)                 # Cconws(done)
    p.label("hang")
    p.emit(0x60FE)                    # bra.s hang
    p.data("name", b"C:\\KAIRO.TXT\0")
    p.data("text", b"KAIRO\r\n")
    p.data("done", b"WRITTEN\r\n\0")
    return p.build()


def read_program():
    p = Program()
    p.emit(0x4267)                    # clr.w -(sp): read only
    p.pea("name")
    p.gemdos(0x3D, 6)                 # Fopen(name, 0)
    p.emit(0x3E00)                    # move.w d0, d7
    p.pea("buffer")
    p.emit(0x2F3C, 0, 7)              # move.l #7, -(sp)
    p.emit(0x3F07)                    # move.w d7, -(sp)
    p.gemdos(0x3F, 10)                # Fread(handle, 7, buffer)
    p.pea("buffer")
    p.gemdos(0x09, 4)                 # Cconws(buffer): what the file held
    p.label("hang")
    p.emit(0x60FE)
    p.data("name", b"C:\\KAIRO.TXT\0")
    p.data("buffer", bytes(8))       # 7 bytes read, then the NUL Cconws ends at
    return p.build()


def hard_disk():
    sectors = 16 * 1024 * 1024 // SECTOR
    start = 1
    part = sectors - start
    image = bytearray(sectors * SECTOR)
    # MBR: one partition, FAT16 under 32 MB (04), from sector 1 to the end.
    entry = bytes([0x00, 0, 0, 0, 0x04, 0, 0, 0]) + start.to_bytes(4, "little") + part.to_bytes(4, "little")
    image[446:462] = entry
    image[510:512] = b"\x55\xAA"
    # FAT16 boot sector: 4 sectors a cluster, 1 reserved, 2 FATs of 32 sectors, 512 root entries.
    per_cluster, reserved, fats, fat_sectors, roots = 4, 1, 2, 32, 512
    boot = bytearray(SECTOR)
    boot[0:3] = b"\xEB\x3C\x90"
    boot[3:11] = b"KAIRO   "
    boot[11:13] = SECTOR.to_bytes(2, "little")
    boot[13] = per_cluster
    boot[14:16] = reserved.to_bytes(2, "little")
    boot[16] = fats
    boot[17:19] = roots.to_bytes(2, "little")
    boot[19:21] = part.to_bytes(2, "little")
    boot[21] = 0xF8
    boot[22:24] = fat_sectors.to_bytes(2, "little")
    boot[24:26] = (32).to_bytes(2, "little")
    boot[26:28] = (64).to_bytes(2, "little")
    boot[28:32] = start.to_bytes(4, "little")
    boot[38] = 0x29
    boot[43:54] = b"KAIRO TEST "
    boot[54:62] = b"FAT16   "
    boot[510:512] = b"\x55\xAA"
    clusters = (part - reserved - fats * fat_sectors - roots * 32 // SECTOR) // per_cluster
    if not 4085 <= clusters < 65525 or (clusters + 2) * 2 > fat_sectors * SECTOR:
        raise SystemExit(f"{clusters} clusters do not make a FAT16 of {fat_sectors} sectors")
    base = start * SECTOR
    image[base:base + SECTOR] = boot
    for fat in range(fats):
        at = base + (reserved + fat * fat_sectors) * SECTOR
        image[at:at + 4] = b"\xF8\xFF\xFF\xFF"
    return bytes(image)


def main():
    folder = ROOT / "core/test/st"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "hdtest.img").write_bytes(hard_disk())
    (folder / "hdtools.st").write_bytes(floppy_files({"WRITE.TOS": write_program(), "READ.TOS": read_program()}))
    print(f"{folder}: hdtest.img, hdtools.st")


if __name__ == "__main__":
    main()
