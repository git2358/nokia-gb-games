# Builds the portable core for the host, the Game Boy and the GBA. Everything
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

# Game Boy: SDCC (sm83 port) and its makebin.
SDCC ?= sdcc
MAKEBIN ?= makebin
SDAS ?= sdasgb
# The Game Boy gives the core a framebuffer the size of its screen.
GB_FB := -DLCD_FB_WIDTH=160 -DLCD_FB_HEIGHT=144
SAMEBOY_TESTER ?= tools/SameBoy/build/bin/tester/sameboy_tester
SAMEBOY_APP ?= /Applications/SameBoy.app
# Emulated seconds to run before the screenshot, and its scale factor.
SHOT_SECONDS ?= 2
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

GB_ROM := $(BUILD)/nokia3310.gb
GBA_ROM := $(BUILD)/nokia3310.gba
# Keys the Game Boy ROM presses at power-on (see platform/gb/main.c), for scripted
# screenshots; check-gb compares the result with the host's frame for them.
KEYS ?=
GB_FRAME := menu-$(KEYS)
GB_SRC := $(CORE_SRC) platform/gb/main.c
GB_REL := $(patsubst %.c,$(BUILD)/gb/%.rel,$(notdir $(GB_SRC))) $(BUILD)/gb/game_assets.rel
GBA_SRC := platform/gba/crt0.s platform/gba/main.c platform/gba/libc.c $(CORE_SRC) $(ASSET_SRC)

vpath %.c core platform/gb

.PHONY: help dump install-roms phone phone-window assets fonts test test-snake sheet frames check-golden gb gba check-gb shot-gb check-gba shot-gba run-gb run-gba clean

help:
	@echo "make dump      rebuild $(DUMP) from the Wintesla files in FLASH_FILES=$(FLASH_FILES)"
	@echo "make phone     boot the dump headlessly in MAME; LCD frames land in $(PHONE_RUN)/"
	@echo "make phone-window open the dump in a MAME window"
	@echo "make test      build and run the host checks (no firmware needed)"
	@echo "make test-snake check Snake's incremental drawing against full redraws"
	@echo "make assets    extract the game graphics from DUMP=$(DUMP) into $(ASSETS)/"
	@echo "make fonts     write the phone's fonts as ASCII-art sheets to $(BUILD)/fonts/"
	@echo "make sheet     draw the extracted assets to $(BUILD)/sheet_*.pgm"
	@echo "make frames    write the host reference frames to $(BUILD)/frame_*.pgm"
	@echo "make check-golden compare host frames with MAME frames in $(GOLDEN)/"
	@echo "make gb        build $(GB_ROM)"
	@echo "make gba       build $(GBA_ROM)"
	@echo "make check-gb  run the Game Boy ROM headlessly and compare its frame with the host's"
	@echo "make shot-gb   run the Game Boy ROM headlessly and write $(BUILD)/nokia3310-gb.png"
	@echo "make check-gba run the GBA ROM headlessly and compare its frame with the host's"
	@echo "make shot-gba  run the GBA ROM headlessly and write $(BUILD)/nokia3310-gba.png"
	@echo "make run-gb    open the Game Boy ROM in SameBoy"
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

# The phone's fonts as ASCII-art sheets, for other projects. Derived from
# the firmware, so they stay under the ignored build directory.
fonts:
	$(PYTHON) tools/export_fonts.py "$(DUMP)" $(BUILD)/fonts

$(BUILD)/test_core: tests/test_core.c core/lcd.c core/rand.c $(CORE_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_core.c core/lcd.c core/rand.c

test: $(BUILD)/test_core
	$(BUILD)/test_core

# Needs the extracted assets, unlike `test`.
$(BUILD)/test_snake: tests/test_snake.c core/lcd.c core/rand.c core/snake.c core/sound.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_snake.c core/lcd.c core/rand.c core/snake.c core/sound.c $(ASSET_SRC)

$(BUILD)/test_snake_gb: tests/test_snake.c core/lcd.c core/rand.c core/snake.c core/sound.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GB_FB) $(INCLUDES) -o $@ tests/test_snake.c core/lcd.c core/rand.c core/snake.c core/sound.c $(ASSET_SRC)

$(BUILD)/test_snake_gba: tests/test_snake.c core/lcd.c core/rand.c core/snake.c core/sound.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GBA_FB) $(INCLUDES) -o $@ tests/test_snake.c core/lcd.c core/rand.c core/snake.c core/sound.c $(ASSET_SRC)

test-snake: $(BUILD)/test_snake $(BUILD)/test_snake_gb $(BUILD)/test_snake_gba
	$(BUILD)/test_snake
	$(BUILD)/test_snake_gb
	$(BUILD)/test_snake_gba

$(BUILD)/asset_sheet: platform/host/asset_sheet.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/asset_sheet.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

sheet: $(BUILD)/asset_sheet
	$(BUILD)/asset_sheet $(BUILD)

$(BUILD)/frame: platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/frame_%.pgm: $(BUILD)/frame
	$(BUILD)/frame $* $@

# The same tool with the Game Boy's framebuffer, for comparing its screen.
$(BUILD)/frame_gb: platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GB_FB) $(INCLUDES) -o $@ platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/gbframe_%.pgm: $(BUILD)/frame_gb
	$(BUILD)/frame_gb $* $@

# And with the GBA's.
$(BUILD)/frame_gba: platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GBA_FB) $(INCLUDES) -o $@ platform/host/frame_main.c platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/gbaframe_%.pgm: $(BUILD)/frame_gba
	$(BUILD)/frame_gba $* $@

# Frames captured from the original firmware in MAME, named after the host
# frame they must equal. Derived from the firmware, so the directory is ignored.
GOLDEN ?= golden

check-golden: $(BUILD)/frame
	$(PYTHON) tools/check_golden.py $(BUILD)/frame $(GOLDEN)

frames: $(BUILD)/frame_testcard.pgm $(BUILD)/frame_outline.pgm $(BUILD)/frame_snake-start.pgm

# Game Boy

$(BUILD)/gb/%.rel: %.c $(CORE_HDR) $(ASSET_SRC)
	@mkdir -p $(BUILD)/gb
	$(SDCC) -msm83 --opt-code-speed $(GB_FB) -Icore -I$(ASSETS) -c $< -o $@

# Always rebuilt, so a change of KEYS takes effect.
$(BUILD)/gb/main.rel: platform/gb/main.c $(CORE_HDR) FORCE
	@mkdir -p $(BUILD)/gb
	$(SDCC) -msm83 --opt-code-speed $(GB_FB) -Icore -I$(ASSETS) '-DSTART_KEYS="$(KEYS)"' -c $< -o $@

FORCE:

$(BUILD)/gb/game_assets.rel: $(ASSETS)/game_assets.c
	@mkdir -p $(BUILD)/gb
	$(SDCC) -msm83 -Icore -I$(ASSETS) -c $< -o $@

$(BUILD)/gb/crt0.rel: platform/gb/crt0.s
	@mkdir -p $(BUILD)/gb
	$(SDAS) -o $@ $<

$(GB_ROM): $(BUILD)/gb/crt0.rel $(GB_REL) FORCE
	$(SDCC) -msm83 --no-std-crt0 -o $(BUILD)/gb/nokia3310.ihx $(BUILD)/gb/crt0.rel $(GB_REL)
	$(MAKEBIN) -Z -yn NOKIA3310 -yt 0x03 -ya 1 $(BUILD)/gb/nokia3310.ihx $@

gb: $(GB_ROM)

check-gb: $(GB_ROM) $(BUILD)/gbframe_$(GB_FRAME).pgm
	@test -x "$(SAMEBOY_TESTER)" || { echo "Missing $(SAMEBOY_TESTER): run scripts/setup-sameboy.sh"; exit 1; }
	$(SAMEBOY_TESTER) --dmg --length $(SHOT_SECONDS) $(GB_ROM)
	$(PYTHON) tools/check_gb_frame.py $(BUILD)/nokia3310.bmp $(BUILD)/gbframe_$(GB_FRAME).pgm

# Headless screenshot of the Game Boy ROM as a PNG.
shot-gb: $(GB_ROM)
	@test -x "$(SAMEBOY_TESTER)" || { echo "Missing $(SAMEBOY_TESTER): run scripts/setup-sameboy.sh"; exit 1; }
	$(SAMEBOY_TESTER) --dmg --length $(SHOT_SECONDS) $(GB_ROM)
	$(PYTHON) tools/bmp_to_png.py $(BUILD)/nokia3310.bmp $(BUILD)/nokia3310-gb.png $(SHOT_SCALE)

# Headless Game Boy sound capture; needs `make -C tools/SameBoy lib`.
$(BUILD)/gb_audio: tools/gb_audio.c
	@test -f tools/SameBoy/build/lib/libsameboy.a || { echo "Missing SameBoy's library: run make -C tools/SameBoy lib"; exit 1; }
	@mkdir -p $(BUILD)
	$(CC) -O2 -Itools/SameBoy -DGB_VERSION='"x"' -o $@ $< tools/SameBoy/build/lib/libsameboy.a -lm

run-gb: $(GB_ROM)
	open -a "$(SAMEBOY_APP)" $(GB_ROM)

# GBA

$(BUILD)/gba/nokia3310.elf: $(GBA_SRC) $(CORE_HDR) platform/gba/gba.ld FORCE
	@mkdir -p $(BUILD)/gba
	$(ARM_CC) $(ARM_CFLAGS) $(GBA_FB) -Icore -I$(ASSETS) '-DSTART_KEYS="$(KEYS)"' -nostdlib -T platform/gba/gba.ld -Wl,-Map,$(BUILD)/gba/nokia3310.map -o $@ $(GBA_SRC) -lgcc

$(GBA_ROM): $(BUILD)/gba/nokia3310.elf tools/gbafix.py FORCE
	$(ARM_OBJCOPY) -O binary $< $@
	$(PYTHON) tools/gbafix.py $@ $(if $(GBA_LOGO_FROM),--logo-from "$(GBA_LOGO_FROM)")

gba: $(GBA_ROM)

# Headless GBA runs use mGBA's core library; scripts/setup-mgba.sh builds it.
MGBA ?= tools/mgba
SHOT_FRAMES ?= 60

$(BUILD)/gba_shot: tools/gba_shot.c
	@test -f "$(MGBA)/build/libmgba.a" || { echo "Missing $(MGBA)/build/libmgba.a: run scripts/setup-mgba.sh"; exit 1; }
	@mkdir -p $(BUILD)
	$(CC) -O2 -I$(MGBA)/include -I$(MGBA)/build/include -o $@ $< $(MGBA)/build/libmgba.a -lm -framework CoreFoundation

check-gba: $(GBA_ROM) $(BUILD)/gba_shot $(BUILD)/gbaframe_$(GB_FRAME).pgm
	$(BUILD)/gba_shot $(GBA_ROM) $(BUILD)/nokia3310-gba.bmp $(SHOT_FRAMES)
	$(PYTHON) tools/check_gb_frame.py $(BUILD)/nokia3310-gba.bmp $(BUILD)/gbaframe_$(GB_FRAME).pgm

shot-gba: $(GBA_ROM) $(BUILD)/gba_shot
	$(BUILD)/gba_shot $(GBA_ROM) $(BUILD)/nokia3310-gba.bmp $(SHOT_FRAMES)
	$(PYTHON) tools/bmp_to_png.py $(BUILD)/nokia3310-gba.bmp $(BUILD)/nokia3310-gba.png 2

run-gba: $(GBA_ROM)
	open -a "$(MGBA_APP)" $(GBA_ROM)

clean:
	rm -rf $(BUILD)
