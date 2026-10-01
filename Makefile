# Host build of the portable core. Everything generated lands in the ignored
# build/ directory; the assets in it are derived from your firmware dump.

# Your own NSE-8/9 v6.00 dump (raw .fls or the swap16 image); see README.md.
DUMP ?= ../nokia-dct3-re/roms/3210f600a.fls

PYTHON ?= python3
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic

BUILD := build
ASSETS := $(BUILD)/assets
CORE_SRC := $(wildcard core/*.c)
INCLUDES := -Icore -Iplatform/host -I$(ASSETS)

.PHONY: help assets test sheet clean

help:
	@echo "make test    build and run the host checks (no firmware needed)"
	@echo "make assets  extract the game graphics from DUMP=$(DUMP) into $(ASSETS)/"
	@echo "make sheet   draw the extracted assets to $(BUILD)/sheet_*.pgm"
	@echo "make clean   remove $(BUILD)/"

assets: $(ASSETS)/game_assets.c

$(ASSETS)/game_assets.c: tools/extract_assets.py
	@test -f "$(DUMP)" || { echo "Missing $(DUMP): pass DUMP=/path/to/3210f600a.fls (see README.md)"; exit 1; }
	$(PYTHON) tools/extract_assets.py "$(DUMP)" $(ASSETS)

$(BUILD)/test_core: tests/test_core.c $(CORE_SRC) $(wildcard core/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_core.c $(CORE_SRC)

test: $(BUILD)/test_core
	$(BUILD)/test_core

$(BUILD)/asset_sheet: platform/host/asset_sheet.c platform/host/pgm.c $(CORE_SRC) $(ASSETS)/game_assets.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ platform/host/asset_sheet.c platform/host/pgm.c $(CORE_SRC) $(ASSETS)/game_assets.c

sheet: $(BUILD)/asset_sheet
	$(BUILD)/asset_sheet $(BUILD)

clean:
	rm -rf $(BUILD)
