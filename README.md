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

## Block textures

Blocks are textured from an atlas of 16×16 tiles kept in `assets/atlases/`. The
folder is staged next to the desktop executable and merged into the Switch NRO's
romfs, so the game loads it from:

- desktop: `<exe dir>/assets/atlases/blocks.png`
- Switch: `romfs:/atlases/blocks.png` (packed inside the `.nro`, **not** the SD card)

If the file is missing the game falls back to a procedurally generated atlas and
logs a warning. Regenerate the tracked PNG with `voxelgame --export-atlas` (run
from the repo root).

The layout — tile size, grid, and tile order — lives in
`src/world/BlockAtlasLayout.hpp`; the block registry (`src/world/Block.cpp`) maps
each block's six faces to a tile index. Replacing `blocks.png` with art of the
same size and tile order needs no code change.
