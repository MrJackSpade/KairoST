<!-- kairo:discord -->
<p align="center">
  <a href="https://discord.gg/ZkYQ3bPTTq"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo/main/docs/assets/discord-banner.svg" alt="Join the Kairo Discord" width="49%"></a>
  <a href="https://play.google.com/store/apps/details?id=com.loxifi.kairost"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo/main/docs/assets/donate-banner.svg" alt="Donate: buying KairoST on Google Play is the donation; it adds no features" width="49%"></a>
</p>
<!-- /kairo:discord -->

<!-- kairo:family -->
<h3 align="center">Other Kairo apps</h3>
<table align="center">
  <tr>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/Kairo98"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo98/main/kairo98/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="Kairo98"><br><b>Kairo98</b></a><br>PC-98</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/KairoDos"><img src="https://raw.githubusercontent.com/MrJackSpade/KairoDos/main/kairodos/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="KairoDos"><br><b>KairoDos</b></a><br>DOS</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/Kairo88"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo88/main/kairo88/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="Kairo88"><br><b>Kairo88</b></a><br>PC-88</td>
  </tr>
  <tr>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/Kairo68"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo68/main/kairo68/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="Kairo68"><br><b>Kairo68</b></a><br>X68000</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/KairoTowns"><img src="https://raw.githubusercontent.com/MrJackSpade/KairoTowns/main/kairotowns/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="KairoTowns"><br><b>KairoTowns</b></a><br>FM Towns</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/KairoMSX"><img src="https://raw.githubusercontent.com/MrJackSpade/KairoMSX/main/kairomsx/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="KairoMSX"><br><b>KairoMSX</b></a><br>MSX</td>
  </tr>
  <tr>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/Kairo64"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo64/main/kairo64/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="Kairo64"><br><b>Kairo64</b></a><br>C64</td>
  </tr>
</table>
<p align="center">Every Kairo app is built on <a href="https://github.com/MrJackSpade/Kairo">Kairo</a>, the shared frontend for the game library, controls, and settings.</p>
<!-- /kairo:family -->

# KairoST

**Play your own Atari ST games on Android phones and handhelds.** KairoST is an Android emulator for the Atari ST, STE, TT and Falcon, built on the [Kairo shared frontend](https://github.com/MrJackSpade/Kairo): the same game library with artwork, controller and touch controls, on-screen keyboard, save states, and per-game settings as [Kairo98](https://github.com/MrJackSpade/Kairo98) and [KairoDos](https://github.com/MrJackSpade/KairoDos). It runs a pinned copy of [Hatari](https://www.hatari-emu.org/). It is an independent project, not an official Hatari release.

## Download

<!-- kairo:download -->
- **[Google Play](https://play.google.com/store/apps/details?id=com.loxifi.kairost)** (paid): updates automatically. Buying it supports development.
- **[GitHub Releases](https://github.com/MrJackSpade/KairoST/releases)** (free): install the APK yourself. The larger `-with-images` APK also bundles catalog artwork.

Both editions are built from the same source and have the same features. Requires an ARM64 device running Android 8.0 or newer.
<!-- /kairo:download -->

## Quick start

1. Install KairoST from [GitHub Releases](https://github.com/MrJackSpade/KairoST/releases).
2. Open KairoST and choose the folder that holds your Atari ST games. You can change it later in **Menu › Manage library**.
3. Select a game and tap **Play**. Games start on EmuTOS, which comes with the app; a few need the original TOS (see [Firmware](#firmware)).

Games and TOS images are not supplied.

## Supported formats

| Type | Formats |
| --- | --- |
| Floppies | ST, MSA, STX, DIM |
| Programs | PRG, TOS, TTP, APP, GTP, in the game's own folder |
| Archives | Any of the above inside ZIP or 7z archives |

A program runs from its folder as the ST's hard disk. A floppy that does not boot by itself starts its program, when it has just one. A game's disks show as one library entry, and **Menu › Disks** changes them during play.

## Firmware

KairoST comes with [EmuTOS](https://emutos.sourceforge.io/), the free TOS replacement, which runs most ST games without any firmware from you. For the few that need the original TOS, import TOS images dumped from your own machine in **Menu › Machine**: **Import all from a folder…** recognizes TOS images by the version in their header, under any name, and keeps one for each model (ST, STE, TT, Falcon); when a folder holds several for a model, it takes the latest English one.

## Machine settings

**Menu › Machine** sets these for all games; each game can override them in its **Game settings**, and the catalog sets them for games that need something else.

- **Machine:** Atari ST (the default), Mega ST, STE, Mega STE, TT or Falcon.
- **Memory:** 512 KB to 4 MB (1 MB by default).
- **Monitor:** color (the default) or monochrome.
- **Region:** Europe, 50 Hz (the default), or USA, 60 Hz.
- **System:** EmuTOS (the default), your TOS for the game's model, or any one of the TOS images you have imported.
- **Skip memory check** (on by default): starts games without TOS's memory test.

## Documentation

- [Create a game](docs/create-a-game.md): make a new game on a blank floppy
- [Licensing and credits](docs/licensing.md): code license and third-party components
- [Source provenance](docs/source-import.md): where the emulator source came from and what was copied

## Help and community

<!-- kairo:community -->
- **Report a game problem** in [Issues](https://github.com/MrJackSpade/KairoST/issues). Include the title, file format, and what happened.
- **Chat** with other players on [Discord](https://discord.gg/ZkYQ3bPTTq).
<!-- /kairo:community -->

## Building from source

<!-- kairo:clone -->
Clone with submodules to get the pinned shared frontend:

```sh
git clone --recurse-submodules https://github.com/MrJackSpade/KairoST.git
```
<!-- /kairo:clone -->

Build the app, and optionally install it, with the shared build script:

```powershell
shared\tools\build.ps1 -Install <adb serial>
```

Host tests, generated code and UI tests: see [development](docs/development.md).

## License

<!-- kairo:license -->
First-party code is [GPL-2.0-or-later](LICENSE.md). The app as distributed is GPLv3 because it bundles GPLv3 components. Third-party components keep their own licenses; see [licensing and credits](docs/licensing.md) and [source provenance](docs/source-import.md).
<!-- /kairo:license -->
