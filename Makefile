# Builds the portable core for the host and the Game Boy. Everything
# generated lands in the ignored build/ directory; the assets in it are
# derived from your firmware dump.

# Your own NHM-2 v5.46 dump, as `make dump` writes it; see README.md.
DUMP ?= roms/noki3410/3410f546e.fls

# The Wintesla flash files the dump is made from, and the MAME fork that
# holds the extractor and the emulator (https://github.com/lukesau/nokia-dct3-re).
# This repository is kept in the fork's ports/ directory.
FLASH_FILES ?= roms/3410-nhm2-v546
DCT3_RE ?= ../..
ROM_SET := roms/noki3410
PMM := 3410 virgin eeprom 005f0000.fls
# What the fork's noki3410 driver declares for BIOS 546e.
DUMP_SHA1 := e650b8a289b434f2c8260c68e44e70e84e41b4cc
PMM_SHA1 := c1cb3a37efc11ea57b96969d2b01ca0f3b0f6bbe

PYTHON ?= python3
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic

# Scale factor of the screenshot.
SHOT_SCALE ?= 3

# Game Boy: SDCC (sm83 port) and its makebin. The Game Boy gives the core a
# framebuffer the size of its screen and shows the phone's LCD in the
# middle of it, on whole tiles, the game as it is (see platform/gb/main.c).
SDCC ?= sdcc
MAKEBIN ?= makebin
SDAS ?= sdasgb
GB_FB := -DLCD_FB_WIDTH=160 -DLCD_FB_HEIGHT=144 -DLCD_PHONE_X=32 -DLCD_PHONE_Y=40
GB_CFLAGS = -msm83 --opt-code-speed $(GB_FB) -DSNAKE2_STATE_AT=0xa100 -DSNAKE2_STATE_END=0xc000 -DLCD_PLATFORM_COLUMNS -DSPRITE_PLATFORM_PRESENT -Icore -Iplatform/gb -I$(ASSETS)
# The SameBoy clone scripts/setup-sameboy.sh makes: its boot ROM, and its
# core as a library (`make -C tools/SameBoy lib`) for tools/gb_run.
SAMEBOY ?= tools/SameBoy
SAMEBOY_BOOT ?= $(SAMEBOY)/build/bin/tester/dmg_boot.bin
SAMEBOY_APP ?= /Applications/SameBoy.app
# Screen frames from power-on at which check-gb takes the ROM's screen;
# shot-gb takes it at the last.
GB_FRAMES ?= 700 1000

BUILD := build
ASSETS := $(BUILD)/assets
ASSET_SRC := $(ASSETS)/game_assets.c $(ASSETS)/snake2_data.c $(ASSETS)/game_tables.c
CORE_SRC := $(wildcard core/*.c)
CORE_HDR := $(wildcard core/*.h)
INCLUDES := -Icore -Iplatform/host -I$(ASSETS)

GB_ROM := $(BUILD)/nokia3410.gb
GB_TEST_ROM := $(BUILD)/nokia3410-keys.gb
# The banks of the Game Boy ROM (see platform/gb/far.h).
GB_BANK0 := platform/gb/main.c platform/gb/far.c platform/gb/screen.c core/lcd.c core/sprite.c core/games.c core/rand.c core/sound.c $(ASSETS)/game_tables.c
GB_BANK1 := core/menu.c core/font.c platform/gb/save.c $(ASSETS)/game_assets.c
GB_BANK2 := core/snake2.c core/title.c $(ASSETS)/snake2_data.c
gb_rels = $(patsubst %.c,$(BUILD)/gb/$(1)/%.rel,$(notdir $(2)))
GB_RELS := $(call gb_rels,0,$(filter-out platform/gb/main.c,$(GB_BANK0))) $(call gb_rels,1,$(GB_BANK1)) \
	$(call gb_rels,2,$(GB_BANK2))

# Keys a test ROM presses at power-on (see menu_script in core/menu.c), for
# scripted screenshots and the checks against the host's frame for them.
# The default opens Snake II and starts a game. The ROM `make gb` builds
# presses none.
KEYS ?= sssss
# The game without the menus, for tools that drive it directly.
GAME_SRC := $(filter-out core/menu.c core/font.c,$(CORE_SRC))

.PHONY: help dump install-roms assets test frames golden-snake check-golden check-menus gb check-gb shot-gb run-gb cards clean

help:
	@echo "make dump      rebuild $(DUMP) from the Wintesla files in FLASH_FILES=$(FLASH_FILES)"
	@echo "make test      build and run the host checks (no firmware needed)"
	@echo "make assets    extract the game data from DUMP=$(DUMP) into $(ASSETS)/"
	@echo "make frames    write the host reference frames to $(BUILD)/frame_*.pgm"
	@echo "make golden-snake record Snake II games in MAME into golden/snake2-*/"
	@echo "make check-golden replay the recorded runs through the core and compare every frame"
	@echo "make check-menus compare the menu pages with the phone's in golden/menus/"
	@echo "make gb        build $(GB_ROM)"
	@echo "make check-gb  run the Game Boy ROM headlessly and compare its screen with the host's"
	@echo "make shot-gb   run the Game Boy ROM headlessly and write $(BUILD)/nokia3410-gb.png"
	@echo "make run-gb    open the Game Boy ROM in SameBoy"
	@echo "make cards     copy the built ROM to the flash carts' SD cards"
	@echo "make clean     remove $(BUILD)/"

# The flash image: MCU, PPM E and the PMM at their addresses, as the MAME
# fork's `make normalize-3410` makes it. Derived from the firmware, so
# roms/ is ignored.
dump:
	@mkdir -p $(ROM_SET)
	$(PYTHON) $(DCT3_RE)/tools/extract_dct3_wintesla.py \
		--mcu $(FLASH_FILES)/NHM2NX05.460 --ppm $(FLASH_FILES)/NHM2NX05.46E \
		--pmm "$(FLASH_FILES)/3410 virgin eeprom.pmm" \
		--flash-output $(DUMP) --eeprom-output "$(ROM_SET)/$(PMM)" \
		--expect-flash-sha1 $(DUMP_SHA1) --expect-eeprom-sha1 $(PMM_SHA1)

# MAME reads its ROM sets from the fork's ignored mame/roms/. The DSP files
# are the fork's placeholders, shared by every phone.
install-roms:
	@test -x $(DCT3_RE)/mame/mame || { echo "Missing $(DCT3_RE)/mame/mame: build the fork first"; exit 1; }
	@test -f $(DUMP) || { echo "Missing $(DUMP): run make dump"; exit 1; }
	@mkdir -p $(DCT3_RE)/mame/roms/noki3410
	cp $(DUMP) "$(ROM_SET)/$(PMM)" $(DCT3_RE)/mame/roms/noki3410/
	cp $(DCT3_RE)/mame/roms/noki3210/dsp_prom $(DCT3_RE)/mame/roms/noki3210/dsp_drom $(DCT3_RE)/mame/roms/noki3210/dsp_pdrom $(DCT3_RE)/mame/roms/noki3410/

assets: $(ASSET_SRC)

$(ASSETS)/game_assets.c: tools/extract_assets.py
	$(PYTHON) tools/extract_assets.py "$(DUMP)" $(ASSETS)

$(ASSETS)/snake2_data.c $(ASSETS)/game_tables.c: $(ASSETS)/game_assets.c

$(BUILD)/test_core: tests/test_core.c core/lcd.c core/rand.c core/sprite.c $(CORE_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Icore -o $@ tests/test_core.c core/lcd.c core/rand.c core/sprite.c

test: $(BUILD)/test_core
	$(BUILD)/test_core

$(BUILD)/frame: platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/frame_%.pgm: $(BUILD)/frame
	$(BUILD)/frame $* $@

frames: $(BUILD)/frame_testcard.pgm $(BUILD)/frame_start.pgm $(BUILD)/frame_run-300.pgm

$(BUILD)/replay: platform/host/replay_main.c platform/host/pgm.c $(GAME_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/replay_main.c platform/host/pgm.c $(GAME_SRC) $(ASSET_SRC)

# Snake II's reference runs: the firmware's own game played by the fork's
# probe, which steers the snake to the food and the creatures and, after
# so many meals, into itself. Its steering is written to the game's state
# and logged; the replay hands the game the key for each turn instead. A
# run is NAME:LEVEL_DOWNS:MAZE_DOWNS:MEALS:SECONDS[:SWITCHES]: how many
# times Down is pressed on the Level page and in the list of mazes (the
# phone starts at level 1 and No maze), and the switches more of the
# probe's, comma-separated.
SNAKE_GOLDENS ?= a:4:0:20:150 b:8:2:10:90 c:1:0:6:150:S3_IGNORE_CREATURE=1,S3_SAVE_ONCE=1

golden-snake:
	@test -x $(DCT3_RE)/mame/mame || { echo "Missing $(DCT3_RE)/mame/mame: build the fork first"; exit 1; }
	@for run in $(SNAKE_GOLDENS); do \
		set -- $$(echo $$run | tr : ' '); dir=golden/snake2-$$1; \
		levels=; i=0; while [ $$i -lt $$2 ]; do levels="$$levels,up,wait400"; i=$$((i + 1)); done; \
		mazes=; i=0; while [ $$i -lt $$3 ]; do mazes="$$mazes,down,wait400"; i=$$((i + 1)); done; \
		keys="enter,wait1500,up,wait300,up,wait300,up,wait300,up,wait1000,enter,wait1500,enter,wait1500,enter,wait6000,down,wait600,down,wait600,enter,wait1500,enter,wait1500$$mazes,enter,wait1500,down,wait600,enter,wait1500$$levels,enter,wait1500,c,wait1200,up,wait600,up,wait600,enter,wait1000"; \
		rm -rf run_golden_snake && mkdir -p run_golden_snake $$dir && \
		$(MAKE) -C $(DCT3_RE) run-keys GAMES_PRODUCT=3410 RUN_DIR=$(abspath run_golden_snake) SECONDS=$$5 KEYS=$$keys \
			RUN_NVRAM_DIR=$(abspath run_golden_snake)/nvram \
			RUN_ENV="S3_AUTO=1 S3_DIE_AFTER=$$4 $$(echo $$6 | tr , ' ')" \
			RUN_EXTRA_ARGS="-autoboot_script $(abspath $(DCT3_RE))/mame_nokia_3410_snake2_probe.lua -debug -debugger none" || exit 1; \
		cp run_golden_snake/nokia_dct3_lcdmirror_*.pgm $$dir/ && \
		$(PYTHON) tools/snake2_events.py run_golden_snake/error.log $$dir; \
	done

# Skipped when the reference runs have not been recorded.
check-golden: $(BUILD)/replay
	@for dir in golden/snake2-*; do \
		if [ -f $$dir/events.txt ]; then \
			out=$$(mktemp -d) && $(BUILD)/replay $$dir/events.txt $$out $$(cat $$dir/setup.txt) && \
			$(PYTHON) tools/check_replay.py $$out $$dir || exit 1; \
		else echo "no $$dir/events.txt: run make golden-snake; skipped"; fi; \
	done

# The menu pages against frames of the phone's own, captured in MAME and
# named after the host frame they must equal. Derived from the firmware, so
# the directory is ignored; skipped when empty.
check-menus: $(BUILD)/frame
	$(PYTHON) tools/check_golden.py $(BUILD)/frame golden/menus

FORCE:

# Game Boy

vpath %.c core platform/gb $(ASSETS)

# games.c reaches the game in bank 2 through far.c, and menu.c the title
# and the game-over picture.
$(BUILD)/gb/0/games.rel: GB_EXTRA := -Dsnake2_handler=far_snake2_handler -Dsnake2_redraw=far_snake2_redraw
$(BUILD)/gb/1/menu.rel: GB_EXTRA := -Dtitle_start=far_title_start -Dtitle_elapse=far_title_elapse -Dtitle_draw=far_title_draw \
	-Dover_start=far_over_start -Dover_elapse=far_over_elapse -Dover_draw=far_over_draw \
	-Dscores_start=far_scores_start -Dscores_elapse=far_scores_elapse -Dscores_draw=far_scores_draw

$(BUILD)/gb/0/%.rel: %.c $(CORE_HDR) $(ASSET_SRC)
	@mkdir -p $(BUILD)/gb/0
	$(SDCC) $(GB_CFLAGS) $(GB_EXTRA) -c $< -o $@

$(BUILD)/gb/1/%.rel: %.c $(CORE_HDR) $(ASSET_SRC)
	@mkdir -p $(BUILD)/gb/1
	$(SDCC) $(GB_CFLAGS) $(GB_EXTRA) --codeseg CODE_1 -c $< -o $@

$(BUILD)/gb/2/%.rel: %.c $(CORE_HDR) $(ASSET_SRC)
	@mkdir -p $(BUILD)/gb/2
	$(SDCC) $(GB_CFLAGS) --codeseg CODE_2 -c $< -o $@

# The layer itself twice: once pressing no keys, once pressing KEYS.
$(BUILD)/gb/0/main.rel $(BUILD)/gb/0/main-keys.rel: platform/gb/main.c $(CORE_HDR) $(ASSET_SRC) FORCE
	@mkdir -p $(BUILD)/gb/0
	$(SDCC) $(GB_CFLAGS) '-DSTART_KEYS="$(if $(findstring keys,$@),$(KEYS))"' -c $< -o $@

$(BUILD)/gb/%.rel: platform/gb/%.s
	@mkdir -p $(BUILD)/gb
	$(SDAS) -o $@ $<

# MBC5 with its rumble pin and battery-backed RAM, four ROM banks (three
# used) and 8 KiB of RAM. The linker does not mind the first bank running
# over into the second, so its end is checked here.
$(BUILD)/nokia3410%gb: $(BUILD)/gb/crt0.rel $(BUILD)/gb/blocks.rel $(BUILD)/gb/0/main%rel $(GB_RELS)
	$(SDCC) -msm83 --no-std-crt0 -Wl-b_CODE_1=0x14000 -Wl-b_CODE_2=0x24000 \
		-o $(BUILD)/gb/$(basename $(notdir $@)).ihx $^
	@$(PYTHON) tools/gb_bank_check.py $(BUILD)/gb/$(basename $(notdir $@)).map
	$(MAKEBIN) -Z -yn NOKIA3410 -yt 0x1e -yo 4 -ya 1 $(BUILD)/gb/$(basename $(notdir $@)).ihx $@

.SECONDARY: $(GB_RELS) $(BUILD)/gb/crt0.rel $(BUILD)/gb/blocks.rel

gb: $(GB_ROM)

# Headless Game Boy runs with tools/gb_run, built on SameBoy's core.
$(BUILD)/gb_run: tools/gb_run.c
	@test -f "$(SAMEBOY)/build/lib/libsameboy.a" || { echo "Missing $(SAMEBOY)/build/lib/libsameboy.a: run scripts/setup-sameboy.sh, then make -C $(SAMEBOY) lib"; exit 1; }
	@mkdir -p $(BUILD)
	$(CC) -O2 -I$(SAMEBOY) -DGB_VERSION='"x"' -o $@ $< $(SAMEBOY)/build/lib/libsameboy.a -lm

# The host's frame tool with the Game Boy's framebuffer.
$(BUILD)/frame_gb: platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GB_FB) $(INCLUDES) -o $@ platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

# The ROM presses KEYS at power-on; its screen at each of GB_FRAMES must be
# one of the host's frames for those keys, to the pixel. With the default
# keys that is a game of Snake II under way. Also says how fast the ROM
# ran the game between the screenshots, 100% being the phone's speed.
check-gb: $(GB_TEST_ROM) $(BUILD)/gb_run $(BUILD)/frame_gb
	$(PYTHON) tools/check_gb_run.py $(BUILD)/gb_run $(GB_TEST_ROM) $(SAMEBOY_BOOT) $(BUILD)/frame_gb "$(KEYS)" $(GB_FRAMES)

# Headless screenshot of the Game Boy ROM after KEYS, as a PNG.
shot-gb: $(GB_TEST_ROM) $(BUILD)/gb_run
	$(BUILD)/gb_run $(GB_TEST_ROM) $(SAMEBOY_BOOT) $(lastword $(GB_FRAMES)) shot:$(BUILD)/nokia3410-gb.pgm
	magick $(BUILD)/nokia3410-gb.pgm -filter point -resize $(SHOT_SCALE)00% $(BUILD)/nokia3410-gb.png

run-gb: $(GB_ROM)
	open -a "$(SAMEBOY_APP)" $(GB_ROM)

# The ROMs to the root of the flash carts' SD cards; see the script.
cards:
	scripts/copy-to-cards.sh

clean:
	rm -rf $(BUILD)
