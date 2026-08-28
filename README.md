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

Blocks are textured from an atlas of 16×16 tiles. The current atlas is generated
procedurally at startup (`src/render/BlockAtlas.cpp`), so PC and Switch render the
same pixels with no bundled asset. The layout — tile size, grid, and tile order —
lives in `src/world/BlockAtlasLayout.hpp`; the block registry (`src/world/Block.cpp`)
maps each block's six faces to a tile index. To switch to an image atlas, adjust
those constants, point the face tiles at the right cells, and load a texture instead
of calling `GenerateBlockAtlasImage()`.
