# Builds the portable core for the host, the Game Boy and the GBA. Everything
# generated lands in the ignored build/ directory; the assets in it are
# derived from your firmware dump.

# Your own NSE-8/9 v6.00 dump (raw .fls or the swap16 image); see README.md.
DUMP ?= ../nokia-dct3-re/roms/3210f600a.fls

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

# GBA: bare arm-none-eabi GCC, no C library.
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

GB_ROM := $(BUILD)/nokia3210.gb
GBA_ROM := $(BUILD)/nokia3210.gba
# Keys the Game Boy ROM presses at power-on (see platform/gb/main.c), for scripted
# screenshots; check-gb compares the result with the host's frame for them.
KEYS ?=
GB_FRAME := menu-$(KEYS)
GB_SRC := $(CORE_SRC) platform/gb/main.c
GB_REL := $(patsubst %.c,$(BUILD)/gb/%.rel,$(notdir $(GB_SRC))) $(BUILD)/gb/game_assets.rel
GBA_SRC := platform/gba/crt0.s platform/gba/main.c platform/gba/libc.c $(CORE_SRC) $(ASSET_SRC)

vpath %.c core platform/gb

.PHONY: help assets fonts test test-snake sheet frames check-golden gb gba check-gb shot-gb check-gba shot-gba run-gb run-gba clean

help:
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
	@echo "make shot-gb   run the Game Boy ROM headlessly and write $(BUILD)/nokia3210-gb.png"
	@echo "make check-gba run the GBA ROM headlessly and compare its frame with the host's"
	@echo "make shot-gba  run the GBA ROM headlessly and write $(BUILD)/nokia3210-gba.png"
	@echo "make run-gb    open the Game Boy ROM in SameBoy"
	@echo "make run-gba   open the GBA ROM in mGBA"
	@echo "make clean     remove $(BUILD)/"

assets: $(ASSET_SRC)

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
$(BUILD)/test_snake: tests/test_snake.c core/lcd.c core/rand.c core/snake.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_snake.c core/lcd.c core/rand.c core/snake.c $(ASSET_SRC)

$(BUILD)/test_snake_gb: tests/test_snake.c core/lcd.c core/rand.c core/snake.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GB_FB) $(INCLUDES) -o $@ tests/test_snake.c core/lcd.c core/rand.c core/snake.c $(ASSET_SRC)

test-snake: $(BUILD)/test_snake $(BUILD)/test_snake_gb
	$(BUILD)/test_snake
	$(BUILD)/test_snake_gb

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
	$(SDCC) -msm83 --no-std-crt0 -o $(BUILD)/gb/nokia3210.ihx $(BUILD)/gb/crt0.rel $(GB_REL)
	$(MAKEBIN) -Z -yn NOKIA3210 -yt 0x03 -ya 1 $(BUILD)/gb/nokia3210.ihx $@

gb: $(GB_ROM)

check-gb: $(GB_ROM) $(BUILD)/gbframe_$(GB_FRAME).pgm
	@test -x "$(SAMEBOY_TESTER)" || { echo "Missing $(SAMEBOY_TESTER): run scripts/setup-sameboy.sh"; exit 1; }
	$(SAMEBOY_TESTER) --dmg --length $(SHOT_SECONDS) $(GB_ROM)
	$(PYTHON) tools/check_gb_frame.py $(BUILD)/nokia3210.bmp $(BUILD)/gbframe_$(GB_FRAME).pgm

# Headless screenshot of the Game Boy ROM as a PNG.
shot-gb: $(GB_ROM)
	@test -x "$(SAMEBOY_TESTER)" || { echo "Missing $(SAMEBOY_TESTER): run scripts/setup-sameboy.sh"; exit 1; }
	$(SAMEBOY_TESTER) --dmg --length $(SHOT_SECONDS) $(GB_ROM)
	$(PYTHON) tools/bmp_to_png.py $(BUILD)/nokia3210.bmp $(BUILD)/nokia3210-gb.png $(SHOT_SCALE)

run-gb: $(GB_ROM)
	open -a "$(SAMEBOY_APP)" $(GB_ROM)

# GBA

$(BUILD)/gba/nokia3210.elf: $(GBA_SRC) $(CORE_HDR) platform/gba/gba.ld FORCE
	@mkdir -p $(BUILD)/gba
	$(ARM_CC) $(ARM_CFLAGS) -Icore -I$(ASSETS) '-DSTART_KEYS="$(KEYS)"' -nostdlib -T platform/gba/gba.ld -Wl,-Map,$(BUILD)/gba/nokia3210.map -o $@ $(GBA_SRC) -lgcc

$(GBA_ROM): $(BUILD)/gba/nokia3210.elf tools/gbafix.py FORCE
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

check-gba: $(GBA_ROM) $(BUILD)/gba_shot $(BUILD)/frame_$(GB_FRAME).pgm
	$(BUILD)/gba_shot $(GBA_ROM) $(BUILD)/nokia3210-gba.bmp $(SHOT_FRAMES)
	$(PYTHON) tools/check_gb_frame.py $(BUILD)/nokia3210-gba.bmp $(BUILD)/frame_$(GB_FRAME).pgm

shot-gba: $(GBA_ROM) $(BUILD)/gba_shot
	$(BUILD)/gba_shot $(GBA_ROM) $(BUILD)/nokia3210-gba.bmp $(SHOT_FRAMES)
	$(PYTHON) tools/bmp_to_png.py $(BUILD)/nokia3210-gba.bmp $(BUILD)/nokia3210-gba.png 2

run-gba: $(GBA_ROM)
	open -a "$(MGBA_APP)" $(GBA_ROM)

clean:
	rm -rf $(BUILD)
