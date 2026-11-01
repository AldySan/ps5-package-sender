# PS5 Package Sender

A portable desktop client that installs PlayStation 5 packages (`.pkg`) and
copies disc images (`.exfat`, `.ffpkg`, `.ffpfs`, `.ffpfsc`) to a jailbroken
PS5 over your home network — no USB drive, no FTP, no installer on the PC.

Built with C++17, Qt 6 (QML / Qt Quick Controls), CMake and SQLite. Builds
are published for **Windows** and **Linux** and are fully portable: unpack
and run, settings live in a `data` folder next to the executable.

## How it works

1. The console runs [pkg-receiver.elf](https://github.com/AldySan/ps5-package-receiver-elf)
   (grab it from that project's releases and load it with your usual payload
   loader). It listens on port `12800` and announces itself on the LAN.
2. This app finds the console automatically, serves packages from a local
   HTTP file server (port `9898`, byte-range capable) and pushes install
   tasks to the receiver — the console downloads and installs through
   Sony's own installer.
3. Disc images are copied straight into a folder your console-side mounter
   scans, with live progress, pause and resume.

## Features

- **Zero-config networking** — UDP beacon discovery with LAN sweep fallback,
  wired-first PC address selection, live receiver status, build and free
  space readout
- **Library** — scan folders or drop files/folders onto the window; PKG
  metadata (title, title ID, version, cover art, header digest) is parsed
  straight from the package header; updates and DLC stay grouped with their
  base game
- **Send queue** — per-item progress, pause, cancel, retry, retry-all-failed,
  reorder while queued; every finished item is recorded in transfer history
- **Scan-path destinations** — image copies can target any folder the
  console mounter scans: `/data/homebrew`, `/data/etaHEN/games`,
  `/mnt/ext0`/`/mnt/ext1`, `/mnt/usb0`–`/mnt/usb7` (and their `homebrew` /
  `etaHEN/games` subfolders), `/mnt/shadowmnt` — or any custom path that
  matches your [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus)
  `scanpath` configuration
- **Share to console** — while enabled, the library is published as a
  catalog the receiver's browser page can browse, with covers and
  install buttons

## Getting started

### On the console

Jailbreak your PS5 and load
[pkg-receiver.elf](https://github.com/AldySan/ps5-package-receiver-elf).
Wait for the *listening on port 12800* notification. That is the only
console-side setup.

### On the PC

1. Download the latest portable archive for your OS from the
   [Releases](../../releases) page and unpack it anywhere.
   - Windows: unzip and run `ps5-package-sender.exe`
   - Linux: `tar -xzf` and run `./run.sh`
2. Put the PC and the console on the same network (a direct Ethernet cable
   with manual addresses on both ends also works — avoid link-local
   auto-assignment).
3. Press **Detect**. Pick the console from the list, or type its IP and
   press **Test**.
4. **+ Add folder** (or drag packages onto the window), select what you
   want and press **Send PKG**. Disc images use **Copy images** — pick the
   destination from the scan-path list first.

## Building from source

Requires Qt 6.5 or newer (Quick, QuickControls2, Sql, Network, Concurrent),
CMake 3.21+, and a C++17 compiler.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CI builds Windows and Linux portable archives on every push to `dev`; tag
pushes (`v*`) publish them as GitHub releases.

## Data location

Everything (settings, library cache, transfer history, custom scan paths)
is kept in `data/store.db` next to the executable. Set
`PS5PKGSENDER_DATA_DIR` to override the location.

## License

GPL-3.0 — see [LICENSE](LICENSE).
