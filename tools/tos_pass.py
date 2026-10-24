#!/usr/bin/env python3
"""Find the catalog disks that need the player's TOS: they fail on EmuTOS and run on real TOS.

Usage: tos_pass.py <seconds> [--workers N]

Boots every disk the library lists (each game's set entries and lone disks) on the host with the
core's boot_test (.build/host/boot_test.exe, under shared/tools/run-limited.ps1) for <seconds> of
guest time on EmuTOS, on the machine its catalog record asks for. A disk that stops with an error
or makes no sound at all is booted again on real TOS for that model (bios/retrobios: TOS 1.04 for
an ST, 1.62 for an STE, 2.06 for a Mega STE, 3.06 for a TT, 4.04 for a Falcon); if it plays sound
there, catalog/research/tos-disks-v1.json gains it, so tools/catalog_config.MACHINE gives it
"tos": "tos". Disks already listed there are kept as they are.

Results are kept per disk in .downloads/tos-pass/results.json, so a stopped run carries on, and the
last frame of every disk that needs TOS is kept beside it for review. boot_test has no monochrome
monitor and boots no program folders, so monochrome disks and programs are reported, not tested.
A disk that crashes the core is recorded as such and listed at the end.
"""
import argparse
import json
import shutil
import subprocess
import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import catalog_config

EMUTOS = ROOT / "third_party/emutos-1024k/etos1024k.img"
TOS = {"st": "tos104uk.img", "megast": "tos104uk.img", "ste": "tos162uk.img", "megaste": "tos206uk.img",
       "tt": "tos306uk.img", "falcon": "tos404.img"}
OUT = ROOT / ".downloads/tos-pass"
LIST = ROOT / "catalog/research/tos-disks-v1.json"


def boot_disks():
    """{content ID: (disk path, machine, why it cannot be tested or None)} for every disk the library lists."""
    research = json.loads((ROOT / "catalog/research/games-v1.json").read_text(encoding="utf-8"))
    where = {}
    for set_name, (index_path, _) in catalog_config.SCANS.items():
        index = json.loads((ROOT / index_path).read_text(encoding="utf-8"))
        for archive in index["archives"]:
            if archive["status"] == "ready":
                for disk in archive["disks"]:
                    where.setdefault(disk["contentId"], (Path(index["root"]) / archive["path"], len(archive["disks"])))
    disks = {}
    for game in research["games"].values():
        if game.get("hidden") or not game.get("contentIds"):
            continue
        behind = {disk for fields in game.get("diskSets", {}).values() for disk in fields["setDisks"]}
        behind |= set(game.get("diskHidden", []))
        for content_id in game["contentIds"]:
            if content_id in behind:
                continue
            path, members = where[content_id]
            settings = game.get("diskMachine", {}).get(content_id, {})
            untestable = ("program" if content_id.startswith("sha256-prg-") else
                          "monochrome" if settings.get("monitor") == "mono" else
                          "archive of several disks" if members > 1 else None)
            disks[content_id] = (path, settings.get("machine", "st"), untestable)
    return disks


def boot(tos, path, machine, seconds, work):
    """(sound peak, error or None) of one boot."""
    work.mkdir(parents=True, exist_ok=True)
    for frame in work.glob("*.bmp"):
        frame.unlink()
    result = subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "shared/tools/run-limited.ps1"),
                             str(ROOT / ".build/host/boot_test.exe"), str(tos), str(work), str(seconds), machine,
                             str(path)], capture_output=True, text=True, encoding="utf-8", errors="replace")
    lines = result.stdout.splitlines() + result.stderr.splitlines()
    for line in lines:
        if line.startswith("run 0:") and " peak " in line:
            return int(line.split(" peak ")[1].split(",")[0]), None
        if line.startswith("run 0: could not start") or line.startswith("run 0: stopped after"):
            return 0, line.split(": ", 2)[-1]
    # A crash of the core is that disk's result: it is listed at the end for debugging.
    crash = next((int(line.split()[1].rstrip(",")) for line in lines if line.startswith("exit -")), None)
    if crash is not None:
        return 0, f"crashed with 0x{crash & 0xffffffff:08X}"
    raise RuntimeError(f"boot_test gave no result for {path} on {tos}:\n" + "\n".join(lines[-20:]))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("seconds", type=int)
    parser.add_argument("--workers", type=int, default=4)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    results_path = OUT / "results.json"
    results = json.loads(results_path.read_text(encoding="utf-8")) if results_path.is_file() else {}
    lock = threading.Lock()
    disks = boot_disks()
    todo = [(content_id, *fields) for content_id, fields in sorted(disks.items())
            if fields[2] is None and content_id not in results]
    slots = [OUT / f"work-{slot}" for slot in range(args.workers)]
    free = list(slots)

    def run(item):
        content_id, path, machine, _ = item
        with lock:
            work = free.pop()
        try:
            # TOSEC's long names put some disks past Windows' MAX_PATH, which Hatari cannot open.
            work.mkdir(parents=True, exist_ok=True)
            path = Path(shutil.copyfile(path, work / ("disk" + path.suffix.lower())))
            peak, error = boot(EMUTOS, path, machine, args.seconds, work)
            record = {"machine": machine, "emutos": {"peak": peak, "error": error}}
            if peak == 0:
                tos = ROOT / "bios/retrobios" / TOS[machine]
                peak, error = boot(tos, path, machine, args.seconds, work)
                record["tos"] = {"image": tos.name, "peak": peak, "error": error}
                if peak > 0 and error is None:
                    (work / "frame-0.bmp").replace(OUT / f"{content_id.split(':')[1]}.bmp")
        finally:
            with lock:
                free.append(work)
        with lock:
            results[content_id] = record
            if len(results) % 25 == 0:
                results_path.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n", encoding="utf-8")
                print(f"{len(results)} disks booted", flush=True)

    print(f"{len(disks)} library disks; {len(todo)} to boot; "
          f"{sum(fields[2] is not None for fields in disks.values())} untestable", flush=True)
    with ThreadPoolExecutor(args.workers) as pool:
        for _ in pool.map(run, todo):
            pass
    results_path.write_text(json.dumps(results, indent=1, sort_keys=True) + "\n", encoding="utf-8")

    listed = json.loads(LIST.read_text(encoding="utf-8"))
    found = 0
    for content_id, record in sorted(results.items()):
        tos = record.get("tos")
        if content_id in listed["disks"] or not tos or tos["peak"] == 0 or tos["error"]:
            continue
        failure = (f"stops: {record['emutos']['error']}" if record["emutos"]["error"]
                   else f"makes no sound in {args.seconds} s")
        listed["disks"][content_id] = (f"tos_pass.py: on EmuTOS 1.4 ({record['machine']}) it {failure}; "
                                       f"{tos['image']} plays sound (peak {tos['peak']})")
        found += 1
    listed["disks"] = dict(sorted(listed["disks"].items()))
    LIST.write_text(json.dumps(listed, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    untestable = {}
    for fields in disks.values():
        if fields[2]:
            untestable[fields[2]] = untestable.get(fields[2], 0) + 1
    print(f"{len(results)} disks booted; {found} newly need TOS ({len(listed['disks'])} listed); "
          f"untestable: {untestable}")
    for content_id, record in sorted(results.items()):
        for system in ("emutos", "tos"):
            error = (record.get(system) or {}).get("error") or ""
            if error.startswith("crashed"):
                print(f"CRASH on {system}: {disks[content_id][0]} ({error})")


if __name__ == "__main__":
    main()
