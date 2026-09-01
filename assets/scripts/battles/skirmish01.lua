-- Skirmish 01 -- a 3v3 warm-up.
-- Everything here is read from the SD card:
--   sdmc:/switch/voxeltactics/assets/scripts/battles/skirmish01.lua
-- Edit and relaunch -- no rebuild.

-- Scene setup runs when the file loads.
set_atlas("atlases/blocks")   -- looks for atlases/blocks.json + its PNG

function on_battle_start()
  print("[lua] skirmish01: placing units")

  -- team 0 = player (south), team 1 = enemy (north)
  spawn(0,  9, 6)
  spawn(0, 11, 6)
  spawn(0, 13, 6)

  spawn(1,  9, 20)
  spawn(1, 11, 20)
  spawn(1, 13, 20)
end
