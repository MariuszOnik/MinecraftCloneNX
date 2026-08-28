# VoxelGame

Cross-platform voxel game in C++17 and raylib. M0 is complete; development now follows the accepted voxel-first order with a test section and CPU mesher before later gameplay systems.

## PC

```powershell
powershell -ExecutionPolicy Bypass -File ./scripts/run-pc-build.ps1
./build/pc-debug/voxelgame.exe
```

The first configure downloads a SHA-256-pinned raylib-nx 6.0 archive.

## Nintendo Switch homebrew

Install devkitPro with devkitA64, libnx, Switch tools, Mesa/EGL/GLES2, and CMake integration. Then run:

```powershell
powershell -ExecutionPolicy Bypass -File ./scripts/build-switch.ps1
```

The output is `build/switch-release/voxelgame.nro`. Compilation alone gives it `compiled` status; see [Switch artifact testing](docs/SWITCH_TESTING.md) before calling it `emulator-tested` or `hardware-tested`.

Every build displays its platform, project version, and commit hash. CI publishes `VoxelGame-Windows`, `VoxelGame-Switch`, and `VoxelGame-Test-Report` artifacts.

## Runtime assets

Atlases, and later models and Lua scripts, live in `assets/`. They are resolved
at runtime in priority order (`src/platform/Assets.cpp`):

| platform | first | fallback |
|----------|-------|----------|
| Switch   | `sdmc:/switch/voxelgame/assets/<rel>` | `romfs:/assets/<rel>` (bundled in the `.nro`) |
| desktop  | `<exe dir>/assets/<rel>` | — |

The SD-card location lets you edit assets and scripts without rebuilding the NRO;
the bundled romfs copy keeps the `.nro` self-contained. CMake stages `assets/`
next to the desktop executable and into the romfs image; CI also ships a
ready-to-copy `sdcard/` tree in the `VoxelGame-Switch` artifact.

Copy the assets onto a Switch SD card (or the Yuzu SD-card directory) with:

```powershell
powershell -ExecutionPolicy Bypass -File ./scripts/deploy-switch-assets.ps1 -SdRoot "D:\path\to\sdcard"
```

### Block atlas

`assets/atlases/blocks.png` is a 4×1 grid of 16×16 tiles (grass top, grass side,
dirt, stone). If it cannot be loaded the game generates one procedurally and logs
a warning. Regenerate the tracked PNG with `voxelgame --export-atlas` (run from
the repo root). The layout lives in `src/world/BlockAtlasLayout.hpp`; the block
registry (`src/world/Block.cpp`) maps each block's six faces to a tile index.
Replacing the PNG with art of the same size and tile order needs no code change.
