#!/usr/bin/env python3
"""Find the catalog disks that need the player's TOS: they fail on EmuTOS and run on real TOS.

Usage: tos_pass.py <seconds> [--workers N]

Boots every disk the library lists (each game's set entries and lone disks) on the host with the
core's boot_test (.build/host/boot_test.exe, under shared/tools/run-limited.ps1) for <seconds> of
guest time on EmuTOS, on the machine its catalog record asks for, and plays it the way a player
starts a game: from 20 s on it presses Space and the joystick's fire button in turn, one every
5 s, until 15 s before the end (PRESSES). A game that crashes only once it is started (3D Pool,
KairoST#6) shows then.

A disk fails a boot when the machine stops with an error, when the guest crashes (a TOS bomb or an
EmuTOS panic, which boot_test reports), when it ends on the GEM desktop (boot_test tells by the
process TOS runs: the game quit, or never started), or when it makes no sound before the first
press and none in the run's last 10 s (a press makes TOS click, so the sound in between does not
count).
A disk that fails on EmuTOS is booted again on real TOS for that model (bios/retrobios: TOS 1.04 for an ST, 1.62 for an STE, 2.06 for a Mega STE, 3.06 for a TT, 4.04 for a Falcon), with
the same presses; if it passes there, catalog/research/tos-disks-v1.json gains it, so
tools/catalog_config.MACHINE gives it "tos": "tos". Disks already listed there are kept as they
are. Disks that fail on both are reported at the end, for review; those that end on the desktop on
both go to catalog/research/desktop-disks-v1.json, from which catalog_config names the program a
disk starts.

Results are kept per disk in .downloads/tos-pass/results-presses.json, so a stopped run carries
on, and the last frame of every disk that needs TOS is kept beside it for review. Monochrome disks,
programs and archives of several disks are reported, not tested. A disk that crashes the core is
recorded as such and listed at the end.
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
# Disks that end on the GEM desktop on both systems: catalog_config names the program they start.
DESKTOP = ROOT / "catalog/research/desktop-disks-v1.json"
# boot_test counts presses in frames; the machines run at 50 Hz (60 Hz for a US machine, whose
# presses then come a little sooner).
FRAMES_PER_SECOND = 50
SPACE = 0x39


def presses(seconds):
    """boot_test's <keys>: Space and fire in turn every 5 s from 20 s until 15 s before the end."""
    items = []
    for number, second in enumerate(range(20, seconds - 15 + 1, 5)):
        items.append(f"{second * FRAMES_PER_SECOND}:{SPACE if number % 2 == 0 else 'fire'}")
    return ",".join(items)


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
    """{"earlyPeak", "latePeak", "crashed", "desktop", "error"} of one boot."""
    work.mkdir(parents=True, exist_ok=True)
    for frame in work.glob("*.bmp"):
        frame.unlink()
    result = subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "shared/tools/run-limited.ps1"),
                             str(ROOT / ".build/host/boot_test.exe"), str(tos), str(work), str(seconds), machine,
                             str(path), "1", presses(seconds)],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    lines = result.stdout.splitlines() + result.stderr.splitlines()
    crashed = any(line.startswith("run 0: the guest crashed") for line in lines)
    for line in lines:
        if line.startswith("run 0:") and " late peak " in line:
            return {"earlyPeak": int(line.split(" early peak ")[1].split(",")[0]),
                    "latePeak": int(line.split(" late peak ")[1].split(",")[0]), "crashed": crashed,
                    "desktop": line.split(" desktop ")[1].split(",")[0] == "yes", "error": None}
        if line.startswith("run 0: could not start") or line.startswith("run 0: stopped after"):
            return {"earlyPeak": 0, "latePeak": 0, "crashed": crashed, "desktop": False,
                    "error": line.split(": ", 2)[-1]}
    # A crash of the core is that disk's result: it is listed at the end for debugging.
    crash = next((int(line.split()[1].rstrip(",")) for line in lines if line.startswith("exit -")), None)
    if crash is not None:
        return {"earlyPeak": 0, "latePeak": 0, "crashed": crashed, "desktop": False,
                "error": f"crashed with 0x{crash & 0xffffffff:08X}"}
    raise RuntimeError(f"boot_test gave no result for {path} on {tos}:\n" + "\n".join(lines[-20:]))


def failure(result, seconds):
    """Why a boot failed, or None when it passed."""
    if result["error"]:
        return f"stops: {result['error']}"
    if result["crashed"]:
        return "crashes (a TOS bomb or EmuTOS panic)"
    if result["desktop"]:
        return "ends on the GEM desktop"
    if result["earlyPeak"] == 0 and result["latePeak"] == 0:
        return f"is silent before the first press and in the last 10 s of {seconds} s"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("seconds", type=int)
    parser.add_argument("--workers", type=int, default=4)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    results_path = OUT / "results-presses.json"
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
            record = {"machine": machine, "seconds": args.seconds,
                      "emutos": boot(EMUTOS, path, machine, args.seconds, work)}
            if failure(record["emutos"], args.seconds):
                tos = ROOT / "bios/retrobios" / TOS[machine]
                record["tos"] = {"image": tos.name, **boot(tos, path, machine, args.seconds, work)}
                if not failure(record["tos"], args.seconds):
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
    both = []
    for content_id, record in sorted(results.items()):
        tos = record.get("tos")
        if not tos:
            continue
        # A disk that crashes, stops or ends on the desktop on EmuTOS needs only to do none of that on
        # TOS: a game can be quiet at the moments sound is measured (TOS 1.04 boots later than EmuTOS,
        # so the first press may come before its intro). One that is only silent on EmuTOS must
        # make sound on TOS.
        emutos_silent_only = failure(record["emutos"], record["seconds"]).startswith("is silent")
        tos_failure = failure(tos, record["seconds"])
        if tos_failure and (emutos_silent_only or not tos_failure.startswith("is silent")):
            both.append(content_id)
            continue
        if content_id in listed["disks"]:
            continue
        listed["disks"][content_id] = (
            f"tos_pass.py: on EmuTOS 1.4 ({record['machine']}), started with Space and fire, it "
            f"{failure(record['emutos'], record['seconds'])}; {tos['image']} runs it (early peak {tos['earlyPeak']}, late peak {tos['latePeak']})")
        found += 1
    listed["disks"] = dict(sorted(listed["disks"].items()))
    LIST.write_text(json.dumps(listed, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    desktop = {content_id: f"tos_pass.py: ends on the GEM desktop on EmuTOS 1.4 and {record['tos']['image']}"
               for content_id, record in sorted(results.items())
               if record["emutos"].get("desktop") and (record.get("tos") or {}).get("desktop")}
    DESKTOP.write_text(json.dumps({"schemaVersion": 1, "disks": desktop}, indent=1, sort_keys=True) + "\n",
                       encoding="utf-8")
    untestable = {}
    for fields in disks.values():
        if fields[2]:
            untestable[fields[2]] = untestable.get(fields[2], 0) + 1
    print(f"{len(results)} disks booted; {found} newly need TOS ({len(listed['disks'])} listed); "
          f"{len(both)} fail on both; untestable: {untestable}")
    (OUT / "fail-on-both.txt").write_text(
        "".join(f"{content_id}\t{disks[content_id][0]}\temutos: {failure(results[content_id]['emutos'], results[content_id]['seconds'])}"
                f"\ttos: {failure(results[content_id]['tos'], results[content_id]['seconds'])}\n"
                for content_id in both if content_id in disks), encoding="utf-8")
    for content_id, record in sorted(results.items()):
        for system in ("emutos", "tos"):
            error = (record.get(system) or {}).get("error") or ""
            if error.startswith("crashed"):
                print(f"CRASH on {system}: {disks[content_id][0]} ({error})")


if __name__ == "__main__":
    main()
