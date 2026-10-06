# Builds the portable core for the host, the Game Boy and the GBA. Everything
# generated lands in the ignored build/ directory; the assets in it are
# derived from your firmware dump.

# Your own NSE-8/9 v6.00 dump (raw .fls or the swap16 image); see README.md.
DUMP ?= ../../../roms/3210f600a.fls

PYTHON ?= python3
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic

# Game Boy: SDCC (sm83 port) and its makebin.
SDCC ?= sdcc
MAKEBIN ?= makebin
SDAS ?= sdasgb
# The Game Boy gives the core a framebuffer the size of its screen, and
# shows the games played on the phone's screen in the full-screen mode at 2x.
GB_FB := -DLCD_FB_WIDTH=160 -DLCD_FB_HEIGHT=144 -DLCD_GAME_ZOOM=2
# The ROM itself also takes the core's innermost drawing loops from
# platform/gb/crt0.s (LCD_PLATFORM_COLUMNS); the host builds with this
# framebuffer keep the C ones.
SAMEBOY ?= ../tools/SameBoy
SAMEBOY_TESTER ?= $(SAMEBOY)/build/bin/tester/sameboy_tester
SAMEBOY_APP ?= /Applications/SameBoy.app
# Emulated seconds to run before the screenshot, and its scale factor.
SHOT_SECONDS ?= 2
SHOT_SCALE ?= 3

# GBA: bare arm-none-eabi GCC, no C library. The GBA gives the core a
# framebuffer the size of its screen and shows the phone's LCD and the
# full-screen board at 2x, and Memory and Rotation in the full-screen
# variant at 3x, which its display hardware scales (see platform/gba/main.c).
GBA_FB := -DLCD_FB_WIDTH=240 -DLCD_FB_HEIGHT=160 -DLCD_ZOOM=2 -DLCD_GAME_ZOOM=3
ARM_CC ?= arm-none-eabi-gcc
ARM_OBJCOPY ?= arm-none-eabi-objcopy
ARM_CFLAGS ?= -std=c99 -O2 -Wall -Wextra -mcpu=arm7tdmi -mthumb -mthumb-interwork -ffreestanding
MGBA_APP ?= /Applications/mGBA.app
# A GBA ROM you own, to copy the boot logo from (needed on real hardware).
GBA_LOGO_FROM ?=

BUILD := build
# The code the three ports share.
COMMON := ../common
ASSETS := $(BUILD)/assets
ASSET_SRC := $(ASSETS)/game_assets.c
CORE_SRC := $(wildcard core/*.c) $(wildcard $(COMMON)/core/*.c)
CORE_HDR := $(wildcard core/*.h) $(wildcard $(COMMON)/core/*.h)
INCLUDES := -Icore -I$(COMMON)/core -Iplatform/host -I$(COMMON)/platform/host -I$(ASSETS)

GB_ROM := $(BUILD)/nokia3210.gb
GBA_ROM := $(BUILD)/nokia3210.gba
# The ROMs the checks and screenshots run: the same, pressing KEYS at
# power-on (see platform/gb/main.c). The ROMs `make gb` and `make gba`
# build press none, whatever KEYS is.
GB_TEST_ROM := $(BUILD)/nokia3210-keys.gb
GBA_TEST_ROM := $(BUILD)/nokia3210-keys.gba
KEYS ?=
GB_FRAME := menu-$(KEYS)
GB_REL := $(patsubst %.c,$(BUILD)/gb/%.rel,$(notdir $(CORE_SRC))) $(BUILD)/gb/game_assets.rel \
	$(BUILD)/gb/native_tiles.rel $(BUILD)/gb/native_data.rel $(BUILD)/gb/native_data_tiles.rel
GBA_SRC := platform/gba/crt0.s platform/gba/main.c $(COMMON)/platform/gba/libc.c $(COMMON)/platform/gba/native_fb.c \
	$(CORE_SRC) $(ASSET_SRC) $(BUILD)/gba/native_data.c

vpath %.c core $(COMMON)/core platform/gb $(COMMON)/platform/gb

.PHONY: help assets fonts test test-snake test-boards sheet frames check-golden gb gba check-gb shot-gb check-gba shot-gba run-gb run-gba cards clean

help:
	@echo "make test      build and run the host checks (no firmware needed)"
	@echo "make test-snake check Snake's incremental drawing against full redraws"
	@echo "make test-boards check Memory's and Rotation's incremental drawing the same way"
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
	@echo "make cards     copy the built ROMs to the flash carts' SD cards"
	@echo "make clean     remove $(BUILD)/"

assets: $(ASSET_SRC)

$(ASSET_SRC): tools/extract_assets.py
	$(PYTHON) tools/extract_assets.py "$(DUMP)" $(ASSETS)

# The phone's fonts as ASCII-art sheets, for other projects. Derived from
# the firmware, so they stay under the ignored build directory.
fonts:
	$(PYTHON) tools/export_fonts.py "$(DUMP)" $(BUILD)/fonts

$(BUILD)/test_core: tests/test_core.c $(COMMON)/core/lcd.c core/rand.c $(CORE_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_core.c $(COMMON)/core/lcd.c core/rand.c

test: $(BUILD)/test_core
	$(BUILD)/test_core

# Needs the extracted assets, unlike `test`.
$(BUILD)/test_snake: tests/test_snake.c $(COMMON)/core/lcd.c core/rand.c core/snake.c core/sound.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_snake.c $(COMMON)/core/lcd.c core/rand.c core/snake.c core/sound.c $(ASSET_SRC)

$(BUILD)/test_snake_gb: tests/test_snake.c $(COMMON)/core/lcd.c core/rand.c core/snake.c core/sound.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GB_FB) $(INCLUDES) -o $@ tests/test_snake.c $(COMMON)/core/lcd.c core/rand.c core/snake.c core/sound.c $(ASSET_SRC)

$(BUILD)/test_snake_gba: tests/test_snake.c $(COMMON)/core/lcd.c core/rand.c core/snake.c core/sound.c $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GBA_FB) $(INCLUDES) -o $@ tests/test_snake.c $(COMMON)/core/lcd.c core/rand.c core/snake.c core/sound.c $(ASSET_SRC)

test-snake: $(BUILD)/test_snake $(BUILD)/test_snake_gb $(BUILD)/test_snake_gba
	$(BUILD)/test_snake
	$(BUILD)/test_snake_gb
	$(BUILD)/test_snake_gba

BOARDS_SRC := tests/test_boards.c $(COMMON)/core/lcd.c core/rand.c core/memory.c core/rotation.c core/sound.c

$(BUILD)/test_boards: $(BOARDS_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(BOARDS_SRC) $(ASSET_SRC)

test-boards: $(BUILD)/test_boards
	$(BUILD)/test_boards

$(BUILD)/asset_sheet: platform/host/asset_sheet.c $(COMMON)/platform/host/pgm.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/asset_sheet.c $(COMMON)/platform/host/pgm.c $(CORE_SRC) $(ASSET_SRC)

sheet: $(BUILD)/asset_sheet
	$(BUILD)/asset_sheet $(BUILD)

$(BUILD)/frame: platform/host/frame_main.c $(COMMON)/platform/host/pgm.c $(COMMON)/tools/native_gen.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/frame_main.c $(COMMON)/platform/host/pgm.c $(COMMON)/tools/native_gen.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/frame_%.pgm: $(BUILD)/frame
	$(BUILD)/frame $* $@

# The same tool with the Game Boy's framebuffer, for comparing its screen.
$(BUILD)/frame_gb: platform/host/frame_main.c $(COMMON)/platform/host/pgm.c $(COMMON)/tools/native_gen.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GB_FB) $(INCLUDES) -o $@ platform/host/frame_main.c $(COMMON)/platform/host/pgm.c $(COMMON)/tools/native_gen.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/gbframe_%.pgm: $(BUILD)/frame_gb
	$(BUILD)/frame_gb $* $@

# And with the GBA's.
$(BUILD)/frame_gba: platform/host/frame_main.c $(COMMON)/platform/host/pgm.c $(COMMON)/tools/native_gen.c $(CORE_SRC) $(CORE_HDR) $(ASSET_SRC)
	$(CC) $(CFLAGS) $(GBA_FB) $(INCLUDES) -o $@ platform/host/frame_main.c $(COMMON)/platform/host/pgm.c $(COMMON)/tools/native_gen.c $(CORE_SRC) $(ASSET_SRC)

$(BUILD)/gbaframe_%.pgm: $(BUILD)/frame_gba
	$(BUILD)/frame_gba $* $@

# Frames captured from the original firmware in MAME, named after the host
# frame they must equal. Derived from the firmware, so the directory is ignored.
GOLDEN ?= golden

check-golden: $(BUILD)/frame
	$(PYTHON) tools/check_golden.py $(BUILD)/frame $(GOLDEN)

frames: $(BUILD)/frame_testcard.pgm $(BUILD)/frame_outline.pgm $(BUILD)/frame_snake-start.pgm

# Game Boy

# The ROM's banks of 16 KiB: the first, always mapped, holds the code but
# the menus; bank 1 the menus and the firmware's pictures and text; banks 2
# and 3 the full-screen menus made at build time (native_tiles.h).
GB_CFLAGS := -msm83 --opt-code-speed $(GB_FB) -DLCD_PLATFORM_COLUMNS -DNATIVE_PLATFORM_TILES -Icore -I$(COMMON)/core \
	-I$(COMMON)/platform/gb -I$(ASSETS)
$(BUILD)/gb/menu.rel $(BUILD)/gb/game_assets.rel: GB_SEG := --codeseg CODE_1
$(BUILD)/gb/native_tiles.rel: GB_SEG := --codeseg CODE_2

$(BUILD)/gb/%.rel: %.c $(CORE_HDR) $(ASSET_SRC) $(COMMON)/platform/gb/native_gb.h
	@mkdir -p $(BUILD)/gb
	$(SDCC) $(GB_CFLAGS) $(GB_SEG) -c $< -o $@

# The full-screen menus made at build time, drawn by the host build of the
# core at the Game Boy's size: the screens in bank 2, their tiles in 3.
$(ASSETS)/native_data.c: $(BUILD)/frame_gb
	$(BUILD)/frame_gb native $@

$(BUILD)/gb/native_data.rel: $(ASSETS)/native_data.c
	@mkdir -p $(BUILD)/gb
	$(SDCC) -msm83 -DNATIVE_PART_SCREENS --codeseg CODE_2 -c $< -o $@

$(BUILD)/gb/native_data_tiles.rel: $(ASSETS)/native_data.c
	@mkdir -p $(BUILD)/gb
	$(SDCC) -msm83 -DNATIVE_PART_TILES --codeseg CODE_3 -c $< -o $@

$(BUILD)/gb/native_put.rel: $(COMMON)/platform/gb/native_put.s
	@mkdir -p $(BUILD)/gb
	$(SDAS) -o $@ $<

# The layer itself twice: once pressing no keys, once pressing KEYS. Always
# rebuilt, so a change of KEYS takes effect.
$(BUILD)/gb/main.rel $(BUILD)/gb/main-keys.rel: platform/gb/main.c $(CORE_HDR) $(ASSET_SRC) FORCE
	@mkdir -p $(BUILD)/gb
	$(SDCC) $(GB_CFLAGS) '-DSTART_KEYS="$(if $(findstring keys,$@),$(KEYS))"' -c $< -o $@

FORCE:

$(BUILD)/gb/game_assets.rel: $(ASSETS)/game_assets.c
	@mkdir -p $(BUILD)/gb
	$(SDCC) -msm83 -Icore -I$(COMMON)/core -I$(ASSETS) $(GB_SEG) -c $< -o $@

$(BUILD)/gb/crt0.rel: platform/gb/crt0.s
	@mkdir -p $(BUILD)/gb
	$(SDAS) -o $@ $<

# MBC1 with battery-backed RAM, four ROM banks. The linker does not mind
# the first bank running over into the second, so its end is checked here.
$(BUILD)/nokia3210%gb: $(BUILD)/gb/crt0.rel $(BUILD)/gb/native_put.rel $(BUILD)/gb/main%rel $(GB_REL)
	$(SDCC) -msm83 --no-std-crt0 -Wl-b_CODE_1=0x14000 -Wl-b_CODE_2=0x24000 -Wl-b_CODE_3=0x34000 \
		-o $(BUILD)/gb/$(basename $(notdir $@)).ihx $^
	@$(PYTHON) $(COMMON)/tools/gb_bank_check.py $(BUILD)/gb/$(basename $(notdir $@)).map
	$(MAKEBIN) -Z -yn NOKIA3210 -yt 0x03 -yo 4 -ya 1 $(BUILD)/gb/$(basename $(notdir $@)).ihx $@

.SECONDARY: $(GB_REL) $(BUILD)/gb/crt0.rel $(BUILD)/gb/native_put.rel

gb: $(GB_ROM)

check-gb: $(GB_TEST_ROM) $(BUILD)/gbframe_$(GB_FRAME).pgm
	@test -x "$(SAMEBOY_TESTER)" || { echo "Missing $(SAMEBOY_TESTER): run scripts/setup-sameboy.sh"; exit 1; }
	$(SAMEBOY_TESTER) --dmg --length $(SHOT_SECONDS) $(GB_TEST_ROM)
	$(PYTHON) $(COMMON)/tools/check_gb_frame.py $(BUILD)/nokia3210-keys.bmp $(BUILD)/gbframe_$(GB_FRAME).pgm

# Headless screenshot of the Game Boy ROM as a PNG.
shot-gb: $(GB_TEST_ROM)
	@test -x "$(SAMEBOY_TESTER)" || { echo "Missing $(SAMEBOY_TESTER): run scripts/setup-sameboy.sh"; exit 1; }
	$(SAMEBOY_TESTER) --dmg --length $(SHOT_SECONDS) $(GB_TEST_ROM)
	$(PYTHON) $(COMMON)/tools/bmp_to_png.py $(BUILD)/nokia3210-keys.bmp $(BUILD)/nokia3210-gb.png $(SHOT_SCALE)

# Headless Game Boy sound capture; needs `make -C $(SAMEBOY) lib`.
$(BUILD)/gb_audio: $(COMMON)/tools/gb_audio.c
	@test -f $(SAMEBOY)/build/lib/libsameboy.a || { echo "Missing SameBoy's library: run make -C $(SAMEBOY) lib"; exit 1; }
	@mkdir -p $(BUILD)
	$(CC) -O2 -I$(SAMEBOY) -DGB_VERSION='"x"' -o $@ $< $(SAMEBOY)/build/lib/libsameboy.a -lm

run-gb: $(GB_ROM)
	open -a "$(SAMEBOY_APP)" $(GB_ROM)

# GBA

$(BUILD)/gba/nokia3210.elf $(BUILD)/gba/nokia3210-keys.elf: $(GBA_SRC) $(CORE_HDR) $(COMMON)/platform/gba/gba.ld FORCE
	@mkdir -p $(BUILD)/gba
	$(ARM_CC) $(ARM_CFLAGS) $(GBA_FB) -Icore -I$(COMMON)/core -I$(ASSETS) -I$(COMMON)/platform/gba/include -DNATIVE_PLATFORM_FB '-DSTART_KEYS="$(if $(findstring keys,$@),$(KEYS))"' -nostdlib -T $(COMMON)/platform/gba/gba.ld -Wl,--no-warn-rwx-segments -Wl,-Map,$(basename $@).map -o $@ $(GBA_SRC) -lgcc

# The full-screen menus made at build time, drawn by the host build of the
# core at the GBA's size (native_tiles.h).
$(BUILD)/gba/native_data.c: $(BUILD)/frame_gba
	@mkdir -p $(BUILD)/gba
	$(BUILD)/frame_gba native $@

$(GBA_ROM): $(BUILD)/gba/nokia3210.elf $(COMMON)/tools/gbafix.py FORCE
	$(ARM_OBJCOPY) -O binary $< $@
	$(PYTHON) $(COMMON)/tools/gbafix.py $@ $(if $(GBA_LOGO_FROM),--logo-from "$(GBA_LOGO_FROM)")

$(GBA_TEST_ROM): $(BUILD)/gba/nokia3210-keys.elf $(COMMON)/tools/gbafix.py FORCE
	$(ARM_OBJCOPY) -O binary $< $@
	$(PYTHON) $(COMMON)/tools/gbafix.py $@

gba: $(GBA_ROM)

# Headless GBA runs use mGBA's core library; ../common/scripts/setup-mgba.sh builds it.
MGBA ?= ../tools/mgba
SHOT_FRAMES ?= 60

$(BUILD)/gba_shot: tools/gba_shot.c
	@test -f "$(MGBA)/build/libmgba.a" || { echo "Missing $(MGBA)/build/libmgba.a: run ../common/scripts/setup-mgba.sh"; exit 1; }
	@mkdir -p $(BUILD)
	$(CC) -O2 -I$(MGBA)/include -I$(MGBA)/build/include -o $@ $< $(MGBA)/build/libmgba.a -lm -framework CoreFoundation

check-gba: $(GBA_TEST_ROM) $(BUILD)/gba_shot $(BUILD)/gbaframe_$(GB_FRAME).pgm
	$(BUILD)/gba_shot $(GBA_TEST_ROM) $(BUILD)/nokia3210-gba.bmp $(SHOT_FRAMES)
	$(PYTHON) $(COMMON)/tools/check_gb_frame.py $(BUILD)/nokia3210-gba.bmp $(BUILD)/gbaframe_$(GB_FRAME).pgm

shot-gba: $(GBA_TEST_ROM) $(BUILD)/gba_shot
	$(BUILD)/gba_shot $(GBA_TEST_ROM) $(BUILD)/nokia3210-gba.bmp $(SHOT_FRAMES)
	$(PYTHON) $(COMMON)/tools/bmp_to_png.py $(BUILD)/nokia3210-gba.bmp $(BUILD)/nokia3210-gba.png 2

run-gba: $(GBA_ROM)
	open -a "$(MGBA_APP)" $(GBA_ROM)

# The ROMs to the root of the flash carts' SD cards; see the script.
cards:
	scripts/copy-to-cards.sh

clean:
	rm -rf $(BUILD)
