# Development

## Building

Build the app, and optionally install it, with the shared build script (see [Android build](../shared/docs/android-build.md)):

```powershell
shared\tools\build.ps1 -Install <adb serial>
```

## Host test

The shared PC boot test runs the core on the host from `core/test/boot-tests.json`: run
`shared\tools\test-core.ps1` (`-Test <name>` for some entries). `tools/make_timing_st.py` and
`tools/make_hard_disk_test.py` write the test's own media under `core/test/st/`.

`tools/tos_pass.py` boots the catalog's disks on EmuTOS and real TOS to find those that need the
player's TOS. Its runner, `core/test/tos_pass_run.cpp`, builds for the host with `core/` (CMake,
into `.build/host`) and runs under `shared\tools\run-limited.ps1`:

```
tos_pass_run <TOS image> <work folder> <seconds> <machine> <floppy image> <keys>
```

`keys` is a list of `frame:scancode` (or `frame:fire`) presses. It prints one JSON line with the
sound's peaks, whether the guest crashed (a TOS bomb or EmuTOS panic), whether it ends on the GEM
desktop and why the machine stopped, and leaves its last picture as `frame.bmp`.

Hatari keeps its state in process-wide variables, so the core builds it as a library of its own
(`kairost_hatari`) and loads a fresh copy of it for every machine (`core/hatari_library.h`): each
machine starts from Hatari's initial state, as a new process would.

## Measuring

`core/test/game_profile` boots a game, runs a fixed number of frames as fast as the host can with
scripted input, and reports host time per guest frame; `--hashes` writes every frame's picture and
sound hash, so two builds can be shown to run the guest identically (Hatari's random seed is fixed).
It builds for the host and, configured with the NDK and `-DKAIROST_TOOLS=ON`, for the device:

```
game_profile <TOS image> <work folder> <media> <machine> <memory KiB> <frames> <script> [--warmup n] [--hashes file]
```

The device build of the core is profile-guided (`core/pgo`, shared `cmake/KairoPgo.cmake`): the
profile was recorded on the RG DS by `tools/perf/pgo-train.ps1` from a training build of the tool
(`-DKAIROST_PGO_GENERATE=ON -DKAIRO_PGO=OFF`). A change to the core's code makes the profile out of
date, which fails the build (`-Werror=profile-instr-out-of-date`): train it again, or build with
`-DKAIRO_PGO=OFF` (gradle property `pgo=false`). `tools/perf/verify-hashes.ps1` and
`compare-hashes.ps1` check that a build runs the guest frame for frame as another does.

## Catalog passes

- `tools/boot_disks.py` reads every floppy the scans list and records how it starts (boot sector,
  AUTO folder, desktop program, or data disk) in `catalog/research/boot-disks-v1.json`.
- `tools/tos_pass.py <seconds>` boots every library disk on EmuTOS, pressing Space and fire after the
  boot, and again on the player's TOS when it fails (an error, a crash, the desktop, or silence), and
  lists the disks that need TOS in `catalog/research/tos-disks-v1.json`.

## Generated CPU emulation

The CPU emulation Hatari generates at build time is kept in `core/generated/cpu/`. After a Hatari update, regenerate it with `tools/generate_cpu.py <host C compiler>`.

<!-- kairo:ui-tests -->
## UI tests

KairoST has device tests in the style of Selenium tests, on the shared Kairo test framework: they
name the app's elements, act on them as a player would, and assert what the player would see
([how they work and how to run them](https://github.com/MrJackSpade/Kairo/blob/main/docs/ui-tests.md)). Run them from this
repository with a device connected over adb (Android 10 or later) and the `LOCAL_BUILD_TOKEN` key set:

```
shared\tools\uitest.ps1 -Device <serial>                                              every test
shared\tools\uitest.ps1 -Device <serial> -Filter 'MenuTests|ControllerTests'          by test name
shared\tools\uitest.ps1 -Device <serial> -Class com.mrjackspade.kairost.uitest.<Class>   one class
shared\tools\uitest-all.ps1 -Device <serial>                                          every Kairo app
```

The tests are in `kairost/src/androidTest/java/com/mrjackspade/kairost/uitest/`. They run in a
build of their own, `com.loxifi.kairost.uitest`, installed beside the app without touching its data, and
each run's logs, screenshots and summary land in `.build/uitest/<run id>/`.
<!-- /kairo:ui-tests -->
