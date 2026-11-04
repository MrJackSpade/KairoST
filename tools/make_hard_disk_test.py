#!/usr/bin/env python3
"""Writes the shared PC boot test's hard disk media for KairoST (KairoST#85):

- core/test/st/hdtest.img: a 16 MB hard disk image, an MBR with one empty FAT16 partition (the
  shared fat_disk.py), which EmuTOS mounts as C: on the model's bus (ACSI on an ST).
- core/test/st/hdtools.st: a 720 KB floppy with WRITE.TOS, which writes KAIRO to C:\\KAIRO.TXT, and
  READ.TOS, which prints what C:\\KAIRO.TXT holds. KairoST types no commands, so the table starts
  each as the Startup program.

    make_hard_disk_test.py
"""
import sys
from pathlib import Path

from make_timing_st import floppy_files

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "shared/tools"))
from fat_disk import mbr_fat16  # noqa: E402
from m68k_assembler import Assembler  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]


class Program(Assembler):
    """A GEMDOS program from the shared 68000 assembler (no relocations: PC-relative only)."""

    def gemdos(self, function, stack):
        self.emit(0x3F3C, function)  # move.w #function, -(sp)
        self.emit(0x4E41)            # trap #1
        stack += 2
        if stack <= 8:
            self.emit(0x508F | ((stack & 7) << 9))  # addq.l #stack, sp
        else:
            self.emit(0x4FEF, stack)                # lea stack(sp), sp

    def build(self):
        text = super().build()
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


def main():
    folder = ROOT / "core/test/st"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "hdtest.img").write_bytes(mbr_fat16(16))
    (folder / "hdtools.st").write_bytes(floppy_files({"WRITE.TOS": write_program(), "READ.TOS": read_program()}))
    print(f"{folder}: hdtest.img, hdtools.st")


if __name__ == "__main__":
    main()
