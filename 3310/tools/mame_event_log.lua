-- Logs every event the 3310's games application hands a game, as
-- "GEV <game id> <event>" in MAME's error.log, on top of the fork's input
-- exerciser. Needs -debug -debugger none. DCT3_RE names the fork.
dofile(os.getenv("DCT3_RE") .. "/mame_nokia_dct3_input_exerciser.lua")
local dbg = manager.machine.devices[":maincpu"].debug
-- games_dispatch_2dbd2a(game id, event), NHM-5 v6.39 only.
dbg:bpset(0x2dbd2a, "1", 'logerror "GEV %x %x\\n",r0,r1; g')
