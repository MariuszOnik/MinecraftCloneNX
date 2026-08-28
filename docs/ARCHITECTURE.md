# Architecture

M0 establishes only the platform and build boundary. Gameplay systems and chunks are intentionally absent.

- `src/app` owns the application entry point and raylib drawing loop.
- `src/core` contains platform-neutral engine data, starting with build diagnostics.
- `src/platform` is the only source directory allowed to branch on the target platform.
- `romfs` contains files packaged into the Switch NRO and distributed beside it.
- CMake fetches the exact raylib-nx 6.0 commit recorded in `build-info.txt`.

GPU calls stay in the application/render path on the main thread. This boundary will be retained as later milestones add workers and gameplay logic.

Switch status must always be reported as one of: `compiled`, `emulator-tested`, or `hardware-tested`.

