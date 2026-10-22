# Third-party source snapshots

These directories contain copied, pinned source snapshots. They are not Git submodules or forks, and this repository has no upstream remote.

- `hatari/`: Hatari 2.6.1, with its license notices preserved. Its test suite, CI scripts and three FS-UAE headers KairoST replaces were omitted.
- `emutos/`, `emutos-1024k/`: the EmuTOS 1.4 source and the EmuTOS team's 1024 KB release image the app ships.
- `zlib/`, `sdl2/`: zlib 1.3.2 and SDL2 2.32.10, the libraries Hatari links, without their documentation, tests and other platforms' build files.

`tools/local_patches.py` applies KairoST's few local changes to these snapshots; the source import lists them.

See [source import](../docs/source-import.md) for URLs, hashes, dates, and what was included. Do not replace these snapshots through a Git remote or automated sync.
