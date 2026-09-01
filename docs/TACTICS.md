# Tactics

Direction change (branch `tactics`): the voxel engine built in M0–M6 becomes the
base for a turn-based tactical game in the spirit of Final Fantasy Tactics —
small hand-built battle maps, isometric camera, a grid of tiles with elevation
that matters, unit turns, abilities, and Lua-driven battle logic.

`main` stays frozen as the known-good open-world voxel engine. Work happens on
`tactics`; nothing here removes engine code, it re-targets it.

## What carries over unchanged

- `src/world` — `World`, `ChunkSection`, `ChunkMesher`, material layers, block
  registry, JSON atlas. A battle map is one bounded `World`, fully loaded and
  meshed once; no streaming.
- `src/model` + `src/render/VoxelModelRenderer` — hierarchical voxel models,
  `.vxm`/`.vxa`, `Animator` with cross-fade. Units render with this.
- `src/render/ModelLoad` — `LoadModelMesh` / `LoadAnimationClip` (binary-then-
  JSON, logged failures), shared by the sandbox and the battle unit cache.
- `voxelgame_assetc` — asset compiler, extended with a MagicaVoxel `.vox`
  importer.
- Isometric camera + wheel zoom (M6 slice 3–4).
- `AssetPaths`, `BuildInfo`/HUD, `WorldSave` (a map is a flat seed + edits, so
  the existing free-cam + break/place is most of a map editor).

## Dormant (still compiles, out of the battle path)

Distance streaming, `PlayerBody`, DDA break/place (moves into a future map
editor mode), the FPS / third-person / free-cam views.

## New modules

```
src/battle/
  BattleMap.{hpp,cpp}    bounded World + spawn points; loads/builds an arena
  TileGrid.{hpp,cpp}     grid derived from the World: per tile the surface
                         height, walkable flag, terrain type, occupant
  Unit.{hpp,cpp}         thin Unit container + UnitRegistry (index+generation)
  Pathfind.{hpp,cpp}     Dijkstra (reachable set) + A* (path) over TileGrid,
                         4-connected, step allowed when |dh| <= unit.jumpHeight
  Battle.{hpp,cpp}       map + units + the player-input state machine; owns the
                         Lua state and dispatches its callbacks
  Script.{hpp,cpp}       Lua 5.1 host: load scripts, the Battle/Unit/UI/roll API
  Dice.{hpp,cpp}         deterministic seeded RNG behind Battle.roll("2d6+3")
  render/UnitRenderer.{hpp,cpp}   units (model + Animator) at tile transforms;
                                  owns the name -> VoxelModelRenderMesh cache
  render/TileOverlay.{hpp,cpp}    cursor highlight + move/attack range quads
  render/BattleUi.{hpp,cpp}       raygui-backed menus/banners Lua asks for
  BattleCamera.{hpp,cpp} iso follow + 90-degree rotation snap + pan + zoom
  src/script/LuaHost.{hpp,cpp}    Lua 5.1 state, safe stdlib, load / call / bind
assets/battle/maps/      arena data (WorldSave format, or a small .map)
assets/scripts/          rules/*.lua, abilities/*.lua, ai/*.lua, battles/*.lua
                         (under assets/ so AssetPaths::Resolve + staging apply)
third_party/raygui.h     vendored, pinned (added at S6)
```

Lua 5.1.5 is vendored via `FetchContent` from `lua.org` (hash-pinned) and built
from source, so PC and Switch link an identical library.

`TurnManager` and the stat / D&D systems are **Lua**, not C++ modules. C++ keeps
only what pathfinding and rendering need on the unit.

The existing `voxelgame` executable keeps its sandbox loop; a `--battle` flag
runs the battle app instead. Tactics becomes the default later (already the
default in the switch-release build -- ships as `voxeltactics.nro`).

## Division of responsibility

The line sits well over toward Lua: **all game logic is Lua**, C++ is only the
engine and the few perf-critical or determinism-critical computations.

**C++ (engine + critical compute):**

- tile grid derivation, **pathfinding** (Dijkstra reachable set + A* path),
  line-of-sight
- meshing, rendering, animation playback, camera, the frame loop
- **model / asset loading** and a name -> `VoxelModelRenderMesh` cache (6 goblins
  share one upload); Lua only sets `unit.model = "goblin"`
- deterministic seeded RNG — Lua calls `Battle.roll("2d6+3")` so rolls replay
  from a save
- the *player input* state machine (select unit -> tile -> target), calling into
  Lua at every rules decision
- UI **rendering + input** via vendored `raygui.h` (`third_party/`, pinned); Lua
  supplies menu content and flow

**Lua 5.1 (PLAN.md sec 9 — must be 5.1; devkitPro NX has no newer Lua, no
LuaJIT):**

- **TurnManager** — initiative / charge-time from DEX / stats
- **stats system** — the D&D-style block (STR/DEX/CON/INT/WIS/CHA, level, class,
  AC, HP dice, proficiency) lives in a Lua table keyed by unit handle
- **D&D rules** — attack rolls (d20 + mod vs AC), damage dice, saving throws,
  conditions / statuses
- abilities (data + a `resolve` fn), enemy AI, win / lose, scripted intros
- menu content + flow (`UI.menu{...}` returns the choice, `UI.banner(text)`, …)

Handles are validated (index + generation); Lua never sees raw pointers or raylib.

**Bridge — the unit handle.** C++ `Unit` is a thin container: `team, tileX, tileZ,
facing, model`, plus `hp` / `hpMax` (a Lua-written mirror, for the HP bar and the
alive check) and `moveTiles` / `jumpHeight` (Lua writes them from the stat block
before a move; C++ pathfinding reads them). The full stat block is Lua's. Save =
Lua serialises its tables, C++ serialises positions + handles + the RNG seed.

Lua API sketch: `Battle.units()`, `Battle.unit_at(x,z)`, `Battle.height(x,z)`,
`Battle.terrain(x,z)`, `Battle.reachable(unit)`, `Battle.path(unit, tile)`,
`Battle.los(a, b)`, `Battle.roll(dice)`, `Battle.move_unit(unit, tile)` (runs the
C++ walk), `Battle.play_anim(unit, clip)`, `Battle.set_hp(unit, hp)`,
`Battle.spawn_fx(...)`, `Battle.end_turn()`; `Unit` handle with `.team`, `.tile`,
`.facing`. Lua callbacks: `on_battle_start()`, `on_turn_begin(unit)`,
`resolve_ability(caster, target, ability)`, `ai_take_turn(unit)`, `check_victory()`.

## Tile model

`TileGrid` covers a rectangular footprint. Per tile:

- `height` — Y where a unit's feet sit (top solid block + 1), or "no floor".
- `walkable` — top block solid-topped and collidable, not water/hazard, with two
  air blocks of headroom.
- `terrain` — from the top `BlockId`: movement cost and cover.
- `occupant` — unit handle or none.

Built by scanning columns top-down; rebuilt only if terrain changes.
`TileCenterWorld(tx, tz) = { tx + 0.5, height, tz + 0.5 }`.

Start with integer heights and 4-connected movement. Slopes / half-height tiles
are a later extension.

## Slice plan

Each slice: local PC build + tests, Switch `.nro` build, one described commit.

- **S1 — arena + tile grid.** `BattleMap` builds a small arena in code (~24x24
  tiles: grass platform, stone steps, a raised area, a water strip). `TileGrid`
  derivation + unit tests on that known layout. `--battle` renders it with the
  iso camera and a wireframe on every walkable tile top.
  *Done when:* walkable tiles are marked correctly; runs on PC and NX.

- **S2 — units on tiles + battle camera.** `Unit` + `UnitRegistry`. Three blue
  and three red humanoids on spawn tiles (shared humanoid model; a team-coloured
  ring on the ground for now). `UnitRenderer` (model + idle Animator at the tile
  transform + facing). `BattleCamera`: iso follow of a focus tile, Q/E rotate 90
  degrees, zoom, pan within map bounds.
  *Done when:* six units stand on the arena; the camera rotates, zooms, pans.

- **S3 — Lua 5.1 integration.** Static Lua on PC and NX (verify the pinned
  devkitPro portlibs first — may need vendoring). `Battle` / `Unit` handle
  bindings, deterministic `Battle.roll`. `scripts/battles/skirmish01.lua` places
  the units and logs "battle start"; C++ calls `on_battle_start()`.
  *Done when:* editing the script changes unit placement with no rebuild; the NX
  package loads the same script.

- **S4 — cursor + selection + movement range.** Tile cursor (camera ray to tile
  tops, snapped). Select a unit; Lua sets `moveTiles` / `jumpHeight` from the
  stat block; C++ `Pathfind::reachable` flood-fills and draws translucent quads.
  Pathfind unit tests (a ledge the unit can and cannot cross).
  *Done when:* selecting a unit shows the correct range, height rules included.

- **S5 — move execution.** Pick a reachable tile, C++ A* the path, walk the unit
  through the tile centres (position lerp + walk clip + per-segment facing turn
  via `LerpAngle`), arrive to idle, update `occupant`; `Battle.move_unit` is the
  Lua entry point, `on_move_done` the callback.
  *Done when:* a unit walks a multi-tile path around an obstacle and up a step
  with matching animation.

- **S6 — turn manager in Lua + action menu.** Vendor `raygui.h`. TurnManager in
  Lua (initiative from stats). C++ runs the loop, focuses the camera on the
  active unit, and renders `UI.menu{"Move","Attack","Wait"}` / `UI.banner`; Lua
  drives the flow and `check_victory()`.
  *Done when:* a manual skirmish is playable with the turn order and menus driven
  from Lua.

- **S7 — D&D combat in Lua.** Attack rolls (d20 + mod vs AC), damage dice, saving
  throws — all in Lua via `Battle.roll`. C++ side: `Battle.damage`, attack
  animation, hit fx, floating numbers. Abilities as data (JSON) + a Lua
  `resolve`: melee, ranged (with a projectile), heal, a small area effect.
  *Done when:* combat is driven entirely by Lua rules + data.

- **S8 — conditions + more abilities (Lua).** Status effects (poison, stun,
  guard), turn-start/end hooks, a few more abilities and a job or two.
  *Done when:* at least two statuses and four abilities work end to end.

- **S9 — enemy AI in Lua.** `ai/basic.lua`: for each enemy unit pick a target, a
  move, and an ability (greedy — hit the lowest-HP unit in range, else approach
  the nearest).
  *Done when:* the red team plays itself; a 3v3 can be won or lost.

- **S10 — `.vox` importer + scenery.** MagicaVoxel `.vox` to `.vxm` in
  `voxelgame_assetc` (single volume, 256-colour palette maps onto `VoxelGrid` +
  `ModelMaterial`). Import props (crates, banners, a tree). More arena block
  types (brick, plank, roof, path).
  *Done when:* a hand-decorated map with imported props renders on PC and NX.

- **S11 — vertical slice polish.** Death animation, turn banner polish, a
  front-end (battle select) as Lua-scripted screens, one scripted intro line.
  One designed map, one designed 3v3 encounter. First build we call a game seed.

Later: a formal map editor mode (free-cam + break/place + save), the charge-time
turn system, status effects, more jobs, larger encounters.

## Open decisions

- Map authoring: build in-engine (free-cam + break/place + save), author in
  MagicaVoxel and import, or a small dedicated `.map` format. Leaning in-engine.
- Tile size: 1 tile = 1 block to start; revisit 2x2 if scenery needs finer detail.
- Keep `PlayerBody` and the streaming code compiling for a future editor mode.
