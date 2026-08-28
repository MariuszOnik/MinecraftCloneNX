# Architecture

M0 established the platform and build boundary. The accepted post-M0 priority is voxel-first.

- `src/app` owns the application entry point and raylib drawing loop.
- `src/core` contains platform-neutral engine data, starting with build diagnostics.
- `src/world` owns block data, chunk sections, and CPU-only meshing.
- `src/render` owns conversion of CPU mesh data into raylib GPU resources.
- `src/platform` is the only source directory allowed to branch on the target platform.
- `romfs` contains files packaged into the Switch NRO and distributed beside it.
- CMake fetches the exact raylib-nx 6.0 commit recorded in `build-info.txt`.

GPU calls stay in the application/render path on the main thread. This boundary will be retained as later milestones add workers and gameplay logic.

The first voxel slice uses a naive face-culling mesher intentionally. Neighboring sections, greedy meshing, material layers, queues, culling, and streaming are separate follow-up slices with their own tests.

Switch status must always be reported as one of: `compiled`, `emulator-tested`, or `hardware-tested`.
