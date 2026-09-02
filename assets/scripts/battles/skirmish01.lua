-- Skirmish 01 -- a 3v3 warm-up.
-- Everything here is read from the SD card:
--   sdmc:/switch/voxeltactics/assets/scripts/battles/skirmish01.lua
-- Edit and relaunch -- no rebuild.

set_atlas("atlases/blocks")   -- looks for atlases/blocks.json + its PNG

local players = {}
local enemies = {}

function on_battle_start()
  print("[lua] skirmish01: placing units")
  players = { spawn(0,  9, 6), spawn(0, 11, 6), spawn(0, 13, 6) }
  enemies = { spawn(1,  9, 20), spawn(1, 11, 20), spawn(1, 13, 20) }
end

-- Turn order: alternate a player then an enemy.
function initiative()
  local order = {}
  for i = 1, math.max(#players, #enemies) do
    if players[i] then order[#order + 1] = players[i] end
    if enemies[i] then order[#order + 1] = enemies[i] end
  end
  return order
end

function on_turn_begin(idx)
  local team = unit_team(idx) == 0 and "blue" or "red"
  print("[lua] turn: " .. team .. " unit " .. idx)
end

function check_victory()
  if count_team(1) == 0 then return "player" end
  if count_team(0) == 0 then return "enemy" end
  return nil
end
