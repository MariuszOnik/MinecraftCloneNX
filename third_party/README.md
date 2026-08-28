# Vendored third-party code

Pinned, unmodified copies of external dependencies. Kept in-tree so local and CI
builds are reproducible without a network fetch.

## nlohmann/json

- Version: 3.11.3
- File: `nlohmann/json.hpp` (single-header amalgamation from `single_include/`)
- Upstream: https://github.com/nlohmann/json
- SHA-256: `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6`
- Licence: MIT (`nlohmann/LICENSE.MIT`)

Used by `src/world/AtlasDescriptor.cpp` to parse atlas descriptors. Included as a
`SYSTEM` header so its warnings do not surface under our `-Wall -Wextra` /
`/W4` build.
