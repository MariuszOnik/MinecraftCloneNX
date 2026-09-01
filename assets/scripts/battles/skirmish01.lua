-- Skirmish 01 -- a 3v3 warm-up. The engine calls on_battle_start(); the script
-- decides where the units stand. Edit these and relaunch -- no rebuild.

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
