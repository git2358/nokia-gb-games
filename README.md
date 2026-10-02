# Nokia 3310 games for Game Boy and Game Boy Advance

The Nokia 3310 (NHM-5 v6.39) follow-up to
<https://github.com/lukesau/nokia-3210-games>. So far this holds the tooling
carried over from that project, a reproducible firmware dump and a way to
boot it in MAME. No game code yet.

## Firmware policy

No Nokia firmware, no extracted code or data from it, and nothing derived
from a firmware image is committed here. `roms/`, run output and build
output are ignored.

That being said I got my firmware at firmware center
<https://firmware.center/firmware/Nokia/3310%20(NHM-5)/Flash%20Files/NHM-5%20v.06.39%203310.rar>

## Dump

Put the Wintesla flash files of NHM-5 v6.39 in `roms/3310-nhm5-v639/`:

| file | SHA-256 |
|---|---|
| `NHM5NY06.390` (MCU) | `c3710f27f68472a70cca0b2af1f8f5048fa435d40d99a36b0f69ea3ab598ea5f` |
| `NHM5NY06.39E` (PPM E) | `3cbdb4e75289016cc4e4ccf7013bba9f7e591d5a27b381f7ad98d90bcbdaa5d9` |
| `v.2.pmm` | `b825400f99a9bd3eba161fc97915084cef8286215796f8792f0523d06f36945f` |

`make dump` strips their record headers and lays the three regions out at
their flash addresses (MCU `0x200000`, PPM `0x340000`, PMM `0x3d0000`):

| output in `roms/noki3310/` | size | SHA-256 |
|---|---:|---|
| `3310f639e.fls` | `0x200000` | `975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442` |
| `3310 v2 pmm.bin` | `0x30000` | `dcb2212579f2a2a7059ed85ef81174d337003566ce2f83f284f20bc70aef8bf4` |

These are the files the `noki3310` driver of the MAME fork
<https://github.com/lukesau/nokia-dct3-re> declares as BIOS `639`. The
extractor and the emulator are the fork's; it is expected next to this
directory (`DCT3_RE=../nokia-dct3-re`) with its MAME already built.

## Running the phone

```
make phone                                   # headless, 15 emulated seconds
make phone PHONE_SECONDS=13 PHONE_KEYS=enter,wait1000,enter
make phone-window                            # a MAME window
```

`make phone` leaves every LCD frame as PGM in the ignored `run_phone/` and
the last one as `run_phone/latest.png`. PPM E starts in Russian.

## Carried over from the 3210 project

`tools/`, `scripts/` and the `Makefile` are copies. The Game Boy and GBA
targets in the `Makefile` expect `core/`, `platform/` and `tests/`, which
are not here yet, and `tools/extract_assets.py` and `tools/export_fonts.py`
still hold the 3210 v6.00 hash and addresses.
