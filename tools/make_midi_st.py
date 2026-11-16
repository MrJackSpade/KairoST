"""Writes core/test/st/midi.st, the shared PC boot test's MIDI program for KairoST (KairoST#93): a 720 KB
FAT12 floppy holding MIDI.TOS, 68000 code that sends the MIDI bytes of SEQUENCE one at a time through the
BIOS (Bconout to device 3, the MIDI port: TOS waits for the ACIA at $FFFC04 and writes $FFFC06), and ends
(Pterm0). The table starts it as the game's Startup program ("program": "MIDI.TOS").

    make_midi_st.py

The sequence holds every kind of message the shared cutter (kairo/midi_bytes.h) tells apart: a SysEx,
channel messages of two and three bytes, data bytes under running status, and a real-time byte.
MESSAGES is what the shared sender gets for it.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "shared/tools"))
sys.path.insert(0, str(ROOT / "tools"))
from m68k_assembler import Assembler  # noqa: E402
from make_timing_st import floppy  # noqa: E402

MESSAGES = [
    "F0 7E 7F 09 01 F7",  # General MIDI on
    "C1 05",              # program change, channel 2
    "91 3C 64",           # note on
    "3C 00",              # under running status: note off as a note on of velocity 0
    "3E 64",              # and another note on
    "F8",                 # timing clock, alone
    "B1 07 7F",           # volume
    "81 3E 00",           # note off
]
SEQUENCE = bytes(int(byte, 16) for message in MESSAGES for byte in message.split())
MIDI_DEVICE, BCONOUT = 3, 3


def text():
    a = Assembler()
    e = a.emit
    a.lea_pc("sequence", 3)                # lea sequence(pc), a3
    e(0x363C, len(SEQUENCE) - 1)           # move.w #length-1, d3
    a.label("next")
    e(0x7000, 0x101B)                      # moveq #0, d0; move.b (a3)+, d0
    e(0x3F00)                              # move.w d0, -(sp): the byte
    e(0x3F3C, MIDI_DEVICE)                 # move.w #3, -(sp): the MIDI port
    e(0x3F3C, BCONOUT)                     # move.w #3, -(sp): Bconout
    e(0x4E4D, 0x5C8F)                      # trap #13; addq.l #6, sp
    a.dbra(3, "next")
    e(0x4267, 0x4E41)                      # clr.w -(sp); trap #1: Pterm0
    a.data("sequence", SEQUENCE)
    return a.build()


def program():
    """MIDI.TOS: a GEMDOS program header, the text, and no relocations."""
    body = bytearray(text())
    if len(body) % 2:
        body += b"\0"
    header = (0x601A).to_bytes(2, "big") + len(body).to_bytes(4, "big") + bytes(4 * 4) + bytes(4) + bytes(2)
    return header + bytes(body) + bytes(4)


def main():
    target = ROOT / "core/test/st/midi.st"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(floppy("MIDI.TOS", program()))
    print(f"{target}: {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
