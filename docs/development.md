# Development

## Building

Build the app, and optionally install it, with the shared build script (see [Android build](../shared/docs/android-build.md)):

```powershell
shared\tools\build.ps1 -Install <adb serial>
```

## Host test

The machine also builds for the host, without Android: build `core/` with CMake for the host, then run, under `shared\tools\run-limited.ps1`:

```
boot_test <TOS image> <work folder> <seconds> [machine [floppy image [runs [keys]]]]
```

EmuTOS is in `third_party/emutos-1024k/etos1024k.img`.

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
