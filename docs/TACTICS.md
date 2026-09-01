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
  Unit.{hpp,cpp}         Unit struct + UnitRegistry (index+generation handles)
  Pathfind.{hpp,cpp}     Dijkstra (reachable set) + A* (path) over TileGrid,
                         4-connected, step allowed when |dh| <= unit.jump
  TurnManager.{hpp,cpp}  initiative / charge-time; whose turn it is
  Battle.{hpp,cpp}       map + units + turn manager + the player-turn state machine
  render/UnitRenderer.{hpp,cpp}   units (model + Animator) at tile transforms
  render/TileOverlay.{hpp,cpp}    cursor highlight + move/attack range quads
  BattleCamera.{hpp,cpp} iso follow + 90-degree rotation snap + pan + zoom
assets/battle/maps/      arena data (WorldSave format, or a small .map)
assets/battle/units/     unit / job stats (JSON)
scripts/                 abilities/*.lua, ai/*.lua, battles/*.lua  (from S6)
```

The existing `voxelgame` executable keeps its sandbox loop; a `--battle` flag
runs the battle app instead. Tactics becomes the default later.

## Division of responsibility

C++: tile grid, pathfinding, movement/animation, meshing, rendering, camera,
low-level save.

Lua 5.1 (PLAN.md sec 9 — must be 5.1: devkitPro NX has no newer Lua and no
LuaJIT): turn order and charge-time, ability resolution (damage, range shapes,
height/facing modifiers),
enemy AI target/action choice, win/lose conditions, scripted battle intros.
Handles are validated (index + generation); Lua never sees raw pointers or raylib.

Lua API sketch: `Battle.units()`, `Battle.unit_at(x,z)`, `Battle.height(x,z)`,
`Battle.terrain(x,z)`, `Battle.damage(unit, n)`, `Battle.move_unit(unit, tile)`,
`Battle.play_anim(unit, clip)`, `Battle.end_turn()`; `Unit` handle with `.hp`,
`.team`, `.tile`, `.facing`, `.stats`, `.status`.

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

- **S3 — cursor + selection + movement range.** Tile cursor (camera ray to tile
  tops, snapped). Select a unit to flood-fill `Pathfind::reachable` (move points
  + jump) and draw translucent quads on reachable tiles. Pathfind unit tests
  (a ledge the unit can and cannot cross).
  *Done when:* selecting a unit shows the correct range, height rules included.

- **S4 — move execution.** Pick a reachable tile, A* the path, walk the unit
  through the tile centres (position lerp + walk clip + per-segment facing turn
  via `LerpAngle`), arrive to idle, update `occupant` and the grid.
  *Done when:* a unit walks a multi-tile path around an obstacle and up a step
  with matching animation.

- **S5 — turn order + basic attack (C++).** `TurnManager` with simple initiative
  (charge-time later). Active unit highlighted, camera focuses it. Action menu
  (Move / Attack / Wait). Attack: pick an adjacent enemy, attack clip, flat
  damage, HP bar, death frees the tile. End turn advances; a team wipe wins.
  *Done when:* a full manual skirmish is playable, all in C++.

- **S6 — Lua 5.1 integration.** Static Lua on PC and NX (verify the pinned
  devkitPro portlibs first). `Battle` / `Unit` bindings as validated handles.
  Move turn resolution and the win condition into `scripts/battles/skirmish01.lua`.
  *Done when:* editing the script changes placement / win rules with no rebuild;
  the NX package loads the same script.

- **S7 — abilities in Lua.** `Ability` data (JSON) + `abilities/*.lua` `resolve`
  functions: range shapes, targeting, height and facing modifiers. Three or four
  abilities: melee, ranged (with a projectile), heal, a small area effect.
  *Done when:* combat is driven entirely by data plus Lua.

- **S8 — enemy AI in Lua.** `ai/basic.lua`: for each enemy unit pick a target,
  a move, and an ability (greedy — hit the lowest-HP unit in range, else
  approach the nearest).
  *Done when:* the red team plays itself; a 3v3 can be won or lost.

- **S9 — `.vox` importer + scenery.** MagicaVoxel `.vox` to `.vxm` in
  `voxelgame_assetc` (single volume, 256-colour palette maps onto `VoxelGrid` +
  `ModelMaterial`). Import props (crates, banners, a tree). More arena block
  types (brick, plank, roof, path).
  *Done when:* a hand-decorated map with imported props renders on PC and NX.

- **S10 — vertical slice polish.** Damage numbers, hit flash, death animation, a
  turn banner, a simple UI frame, one scripted intro line. One designed map, one
  designed 3v3 encounter. This is the first build we call a game seed.

Later: a formal map editor mode (free-cam + break/place + save), the charge-time
turn system, status effects, more jobs, larger encounters.

## Open decisions

- Map authoring: build in-engine (free-cam + break/place + save), author in
  MagicaVoxel and import, or a small dedicated `.map` format. Leaning in-engine.
- Tile size: 1 tile = 1 block to start; revisit 2x2 if scenery needs finer detail.
- Keep `PlayerBody` and the streaming code compiling for a future editor mode.
