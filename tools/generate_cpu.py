#!/usr/bin/env python3
"""Writes the 68000-family emulation sources Hatari's build generates into core/generated/cpu/.

Usage: generate_cpu.py <host C compiler> [--check]

Hatari's CPU core (third_party/hatari/src/cpu, from WinUAE) is mostly generated: build68k turns the
instruction table (table68k) into cpudefs.c, and gencpu turns that into the instruction handlers
(cpuemu_*.c, cpustbl.c, cputbl.h). Both tools must run on the build host, so the output is kept
in the repository and every build, Android or host, compiles it without a host compiler. The
host compiler only builds the two tools. --check regenerates into a temporary folder and fails
when the committed files differ.
"""
import filecmp
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPU = ROOT / 'third_party/hatari/src/cpu'
OUT = ROOT / 'core/generated/cpu'
# KairoST's stand-ins for the uae/ headers left out of the snapshot.
KAIRO = ROOT / 'core/kairo'


def generate(cc, work):
    exe = '.exe' if sys.platform == 'win32' else ''
    build68k = work / f'build68k{exe}'
    gencpu = work / f'gencpu{exe}'
    subprocess.run([cc, '-O1', f'-I{CPU}', f'-I{KAIRO}', str(CPU / 'build68k.c'), str(CPU / 'writelog.c'), '-o', str(build68k)],
                   check=True)
    with open(CPU / 'table68k', 'rb') as table, open(work / 'cpudefs.c', 'wb') as defs:
        subprocess.run([str(build68k)], stdin=table, stdout=defs, check=True)
    subprocess.run([cc, '-O1', f'-I{CPU}', f'-I{KAIRO}', str(work / 'cpudefs.c'), str(CPU / 'gencpu.c'), str(CPU / 'readcpu.c'),
                    '-o', str(gencpu)], check=True)
    # gencpu writes its files to the current folder.
    subprocess.run([str(gencpu)], cwd=work, check=True, stdout=subprocess.DEVNULL)
    files = sorted(p for p in work.iterdir() if p.suffix in ('.c', '.h'))
    if not files:
        raise SystemExit('gencpu wrote no sources')
    return files


def main():
    args = sys.argv[1:]
    check = '--check' in args
    args = [a for a in args if a != '--check']
    if len(args) != 1:
        raise SystemExit(__doc__)
    with tempfile.TemporaryDirectory() as temp:
        files = generate(args[0], Path(temp))
        if check:
            names = {p.name for p in files}
            committed = {p.name for p in OUT.iterdir()} if OUT.is_dir() else set()
            stale = sorted(names ^ committed)
            stale += [p.name for p in files if p.name in committed and not filecmp.cmp(p, OUT / p.name, shallow=False)]
            if stale:
                raise SystemExit('core/generated/cpu is out of date: ' + ', '.join(stale))
            return
        if OUT.exists():
            shutil.rmtree(OUT)
        OUT.mkdir(parents=True)
        for p in files:
            shutil.copyfile(p, OUT / p.name)
        print(f'wrote {len(files)} files to {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
