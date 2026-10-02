# Builds the portable core for the host and the GBA. Everything
# generated lands in the ignored build/ directory; the assets in it are
# derived from your firmware dump.

# Your own NHM-5 v6.39 dump, as `make dump` writes it; see README.md.
DUMP ?= roms/noki3310/3310f639e.fls

# The Wintesla flash files the dump is made from, and the MAME fork that
# holds the extractor and the emulator (https://github.com/lukesau/nokia-dct3-re).
FLASH_FILES ?= roms/3310-nhm5-v639
DCT3_RE ?= ../nokia-dct3-re
ROM_SET := roms/noki3310
PMM := 3310 v2 pmm.bin
# What the fork's noki3310 driver declares for BIOS 639.
DUMP_SHA1 := d5da65f417595200314eb0115bf46ca1fbf53128
PMM_SHA1 := 6bfb76a2055617e16016bf1b86efa621859efef6
PHONE_RUN ?= run_phone
PHONE_SECONDS ?= 15
# Keys the phone gets once it is up, e.g. PHONE_KEYS=enter,wait1000,down
PHONE_KEYS ?=

PYTHON ?= python3
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic

# Scale factor of the screenshot.
SHOT_SCALE ?= 3

# GBA: bare arm-none-eabi GCC, no C library. The GBA gives the core a
# framebuffer the size of its screen and shows the phone's LCD and the
# full-screen board at 2x.
GBA_FB := -DLCD_FB_WIDTH=240 -DLCD_FB_HEIGHT=160 -DLCD_ZOOM=2
ARM_CC ?= arm-none-eabi-gcc
ARM_OBJCOPY ?= arm-none-eabi-objcopy
ARM_CFLAGS ?= -std=c99 -O2 -Wall -Wextra -mcpu=arm7tdmi -mthumb -mthumb-interwork -ffreestanding
MGBA_APP ?= /Applications/mGBA.app
# A GBA ROM you own, to copy the boot logo from (needed on real hardware).
GBA_LOGO_FROM ?=

BUILD := build
ASSETS := $(BUILD)/assets
ASSET_SRC := $(ASSETS)/game_assets.c
CORE_SRC := $(wildcard core/*.c)
CORE_HDR := $(wildcard core/*.h)
INCLUDES := -Icore -Iplatform/host -I$(ASSETS)

GBA_ROM := $(BUILD)/nokia3310.gba
GBA_SRC := platform/gba/crt0.s platform/gba/main.c platform/gba/libc.c $(CORE_SRC) $(ASSET_SRC)
HOST_CORE := core/lcd.c core/rand.c core/sprite.c core/si.c core/games.c

# The recorded Space Impact run the core is checked against: the keys, the
# seed the title animation leaves the games' random generator with, and
# where its frames and events are kept. See "Golden run" in README.md.
GOLDEN ?= golden/space-impact-events1
GOLDEN_SEED := a335
GOLDEN_SECONDS := 40
GOLDEN_KEYS := enter,wait1000,enter,wait1200,up,up,up,up,up,wait800,enter,wait1500,down,wait600,enter,wait2500,enter,wait4000,1,wait400,1,wait400,8,8,8,1,wait400,0,0,0,0,1,wait300,3,wait300,4,wait2000,1,wait500,1,wait6000

.PHONY: help dump install-roms phone phone-window assets test sheet frames golden check-golden gba check-gba shot-gba run-gba clean

help:
	@echo "make dump      rebuild $(DUMP) from the Wintesla files in FLASH_FILES=$(FLASH_FILES)"
	@echo "make phone     boot the dump headlessly in MAME; LCD frames land in $(PHONE_RUN)/"
	@echo "make phone-window open the dump in a MAME window"
	@echo "make test      build and run the host checks (no firmware needed)"
	@echo "make assets    extract the game data from DUMP=$(DUMP) into $(ASSETS)/"
	@echo "make sheet     draw the extracted sprites and tiles to $(BUILD)/sheet_*.pgm"
	@echo "make frames    write the host reference frames to $(BUILD)/frame_*.pgm"
	@echo "make golden    record the reference Space Impact run in MAME into $(GOLDEN)/"
	@echo "make check-golden replay that run through the core and compare every frame"
	@echo "make gba       build $(GBA_ROM)"
	@echo "make check-gba run the GBA ROM headlessly and compare its screen with the host's"
	@echo "make shot-gba  run the GBA ROM headlessly and write $(BUILD)/nokia3310-gba.png"
	@echo "make run-gba   open the GBA ROM in mGBA"
	@echo "make clean     remove $(BUILD)/"

# The 2 MiB flash image: MCU, PPM E and the PMM tail at their addresses.
# Derived from the firmware, so roms/ is ignored.
dump:
	$(PYTHON) $(DCT3_RE)/tools/extract_dct3_wintesla.py \
		--mcu $(FLASH_FILES)/NHM5NY06.390 --ppm $(FLASH_FILES)/NHM5NY06.39E \
		--pmm $(FLASH_FILES)/v.2.pmm \
		--flash-output $(ROM_SET)/3310f639e_mcu_ppm.fls \
		--eeprom-output "$(ROM_SET)/$(PMM)" --expect-eeprom-sha1 $(PMM_SHA1) \
		--combined-output $(DUMP)
	@test "$$(shasum $(DUMP) | cut -d' ' -f1)" = $(DUMP_SHA1) || { echo "$(DUMP) is not NHM-5 v6.39 PPM E"; exit 1; }

# MAME reads its ROM sets from the fork's ignored mame/roms/. The DSP files
# are the fork's placeholders, shared by every phone.
install-roms:
	@test -x $(DCT3_RE)/mame/mame || { echo "Missing $(DCT3_RE)/mame/mame: build the fork first"; exit 1; }
	@test -f $(DUMP) || { echo "Missing $(DUMP): run make dump"; exit 1; }
	@mkdir -p $(DCT3_RE)/mame/roms/noki3310
	cp $(DUMP) "$(ROM_SET)/$(PMM)" $(DCT3_RE)/mame/roms/noki3310/
	cp $(DCT3_RE)/mame/roms/noki3210/dsp_prom $(DCT3_RE)/mame/roms/noki3210/dsp_drom $(DCT3_RE)/mame/roms/noki3210/dsp_pdrom $(DCT3_RE)/mame/roms/noki3310/

phone: install-roms
	$(MAKE) -C $(DCT3_RE) run-prebuilt PHONE=noki3310 BIOS=639 RUN_DIR=$(abspath $(PHONE_RUN)) \
		SECONDS=$(PHONE_SECONDS) FRAME_PNG=$(abspath $(PHONE_RUN))/latest.png \
		RUN_ENV='$(if $(PHONE_KEYS),NOKIA_DCT3_POST_READY_KEYS=$(PHONE_KEYS) NOKIA_DCT3_POST_READY_KEY_DELAY_MS=6000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=200 NOKIA_DCT3_POST_READY_KEY_GAP_MS=200 NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=1200)'

phone-window: install-roms
	@mkdir -p $(PHONE_RUN)_window/nvram
	cd $(PHONE_RUN)_window && $(abspath $(DCT3_RE))/mame/mame noki3310 -bios 639 \
		-rompath $(abspath $(DCT3_RE))/mame/roms -nvram_directory nvram \
		-window -resolution 672x384 -keepaspect -skip_gameinfo

assets: $(ASSET_SRC)

$(ASSET_SRC): tools/extract_assets.py
	$(PYTHON) tools/extract_assets.py "$(DUMP)" $(ASSETS)

$(BUILD)/test_core: tests/test_core.c core/lcd.c core/rand.c core/sprite.c $(CORE_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Icore -o $@ tests/test_core.c core/lcd.c core/rand.c core/sprite.c

test: $(BUILD)/test_core
	$(BUILD)/test_core

$(BUILD)/asset_sheet: platform/host/asset_sheet.c core/si.c core/sprite.c core/lcd.c core/rand.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/asset_sheet.c core/si.c core/sprite.c core/lcd.c core/rand.c $(ASSET_SRC)

sheet: $(BUILD)/asset_sheet
	$(BUILD)/asset_sheet $(BUILD)

$(BUILD)/frame: platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/frame_%.pgm: $(BUILD)/frame
	$(BUILD)/frame $* $@

# The same tool with the GBA's framebuffer, for comparing its screen.
$(BUILD)/frame_gba: platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GBA_FB) $(INCLUDES) -o $@ platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/gbaframe_%.pgm: $(BUILD)/frame_gba
	$(BUILD)/frame_gba $* $@

frames: $(BUILD)/frame_testcard.pgm $(BUILD)/frame_start.pgm $(BUILD)/frame_run-300.pgm

# The reference run: the firmware playing Space Impact in MAME with scripted
# keys, every LCD frame kept and every event the game was handed logged.
# Derived from the firmware, so the directory is ignored. The MAME fork is
# asked for GAMES_PRODUCT=3310; see its docs/games_applications_3310.md.
golden:
	@test -x $(DCT3_RE)/mame/mame || { echo "Missing $(DCT3_RE)/mame/mame: build the fork first"; exit 1; }
	@mkdir -p $(GOLDEN) run_golden
	DCT3_RE=$(abspath $(DCT3_RE)) $(MAKE) -C $(DCT3_RE) run-keys GAMES_PRODUCT=3310 RUN_DIR=$(abspath run_golden) \
		SECONDS=$(GOLDEN_SECONDS) KEYS=$(GOLDEN_KEYS) FRAME_PNG=$(abspath run_golden)/latest.png \
		RUN_EXTRA_ARGS='-autoboot_script $(abspath tools/mame_event_log.lua) -debug -debugger none'
	cp run_golden/nokia_dct3_lcdmirror_*.pgm $(GOLDEN)/
	awk '/^GEV 1 2b/{on=1} on && /^GEV 1 /{print $$3}' run_golden/error.log | tr '\n' ' ' > $(GOLDEN)/events.txt

$(BUILD)/replay: platform/host/replay_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/replay_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

# Skipped when the reference run has not been recorded.
check-golden: $(BUILD)/replay
	@if [ -f $(GOLDEN)/events.txt ]; then \
		out=$$(mktemp -d) && $(BUILD)/replay $(GOLDEN)/events.txt $$out $(GOLDEN_SEED) && \
		$(PYTHON) tools/check_replay.py $$out $(GOLDEN); \
	else echo "no $(GOLDEN)/events.txt: run make golden; skipped"; fi

# GBA

FORCE:

$(BUILD)/gba/nokia3310.elf: $(GBA_SRC) $(CORE_HDR) platform/gba/gba.ld FORCE
	@mkdir -p $(BUILD)/gba
	$(ARM_CC) $(ARM_CFLAGS) $(GBA_FB) -Icore -I$(ASSETS) -Iplatform/gba/include -nostdlib -T platform/gba/gba.ld -Wl,-Map,$(BUILD)/gba/nokia3310.map -o $@ $(GBA_SRC) -lgcc

$(GBA_ROM): $(BUILD)/gba/nokia3310.elf tools/gbafix.py FORCE
	$(ARM_OBJCOPY) -O binary $< $@
	$(PYTHON) tools/gbafix.py $@ $(if $(GBA_LOGO_FROM),--logo-from "$(GBA_LOGO_FROM)")

gba: $(GBA_ROM)

# Headless GBA runs use mGBA's core library; scripts/setup-mgba.sh builds it.
MGBA ?= tools/mgba
# Screen frames to run before the screenshot; the game starts at power-on.
SHOT_FRAMES ?= 300

$(BUILD)/gba_shot: tools/gba_shot.c
	@test -f "$(MGBA)/build/libmgba.a" || { echo "Missing $(MGBA)/build/libmgba.a: run scripts/setup-mgba.sh"; exit 1; }
	@mkdir -p $(BUILD)
	$(CC) -O2 -I$(MGBA)/include -I$(MGBA)/build/include -o $@ $< $(MGBA)/build/libmgba.a -lm -framework CoreFoundation

# The ROM's screen after SHOT_FRAMES frames must be the host's picture of the
# game after about as much time. The ROM takes some 35 frames to put up its
# first picture and starts the game's clock then, so the host's frames up
# to 48 earlier are accepted; a tick is 5 or 6 frames, so that is one
# picture out of nine.
check-gba: $(GBA_ROM) $(BUILD)/gba_shot $(BUILD)/frame_gba
	$(BUILD)/gba_shot $(GBA_ROM) $(BUILD)/nokia3310-gba.bmp $(SHOT_FRAMES)
	@for back in $$(seq 0 48); do \
		n=$$(($(SHOT_FRAMES) - back)); $(BUILD)/frame_gba run-$$n $(BUILD)/gbaframe_run-$$n.pgm >/dev/null; \
		if $(PYTHON) tools/check_gb_frame.py $(BUILD)/nokia3310-gba.bmp $(BUILD)/gbaframe_run-$$n.pgm >/dev/null 2>&1; then \
			echo "ok: the ROM's screen after $(SHOT_FRAMES) frames is the host's after $$n"; exit 0; fi; \
	done; echo "FAIL: the ROM's screen after $(SHOT_FRAMES) frames matches no host frame near it"; exit 1

shot-gba: $(GBA_ROM) $(BUILD)/gba_shot
	$(BUILD)/gba_shot $(GBA_ROM) $(BUILD)/nokia3310-gba.bmp $(SHOT_FRAMES)
	$(PYTHON) tools/bmp_to_png.py $(BUILD)/nokia3310-gba.bmp $(BUILD)/nokia3310-gba.png $(SHOT_SCALE)

run-gba: $(GBA_ROM)
	open -a "$(MGBA_APP)" $(GBA_ROM)

clean:
	rm -rf $(BUILD)
