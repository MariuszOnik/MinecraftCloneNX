# VoxelGame

Cross-platform voxel game in C++17 and raylib. M0 is complete; development now follows the accepted voxel-first order. The CPU mesher is greedy: coplanar faces of the same tile merge into one quad (a solid 16³ section is six quads), and a tiling shader repeats each tile across the merged quad via `tileOrigin + fract(uv) * tileExtent`, with a 1-px gutter around every atlas tile.

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
next to the desktop executable and into the romfs image.

### Running on a real Switch

The Switch needs homebrew (Atmosphère). Copy the build onto the SD card:

```powershell
powershell -ExecutionPolicy Bypass -File ./scripts/deploy-switch-sd.ps1 -SdRoot "E:\"
```

`-SdRoot` is a card-reader drive, a staging folder, or the Yuzu "SD Card"
directory. It writes `switch\voxelgame\voxelgame.nro` and
`switch\voxelgame\assets\`. Put the card back in the Switch and launch
**voxelgame** from the homebrew menu.

The CI `VoxelGame-Switch` artifact already contains this tree under `sdcard/` —
drop its contents onto the SD root for the same result. (MTP — *This PC → Nintendo
Switch → SD Card* — works too, just drag the folders manually; it can't be
scripted.)

### Block atlas

An atlas is an image plus a `.json` descriptor (grid size, named tiles by grid
coordinate, per-block face mapping). The engine loads `atlases/blocks.json` and
`atlases/blocks.png`; see [assets/atlases/README.md](assets/atlases/README.md) for
the schema. A missing/invalid descriptor falls back to the compiled defaults in
`src/world/Block.cpp`; a missing image falls back to a procedural atlas
(`src/render/BlockAtlas.cpp`). Regenerate the tracked PNG with
`voxelgame --export-atlas` (from the repo root). Swapping atlases is data-only:
ship a new image + descriptor, no code change. JSON parsing uses the vendored
`third_party/nlohmann/json.hpp` (v3.11.3, MIT).
