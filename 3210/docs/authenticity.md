# Notes: making the code as authentic as the output

Written 2026-10-01. The Game Boy build already looks and plays like the
phone: its frames match the firmware's in MAME and Snake's logic follows the
firmware's routines. These notes record where the code's structure differs
from the firmware's, and how to close the gap without losing speed. None of
it changes what the player sees.

The standard to aim for: the game code in `core/` mirrors the firmware
function for function, and every workaround for a console lives in that
console's `platform/` directory. The menus stay pixel-exact imitations; the
firmware's widget and layout framework is not worth re-implementing.

## What already mirrors the firmware

- Snake's state and routines: the ring of 2-bit directions, the occupancy
  bitmap, the collision check and its grace move, head and tail movement,
  food placement, scoring (`core/snake.c` against `snake_*` in the fork's
  `docs/games_applications.md`).
- `rand`, the speed table and the tick arithmetic.
- Fonts, text and bitmaps, read from the dump and drawn in the firmware's
  formats.

## Where the structure differs, and what to do

1. **Event interface.** The firmware's games are one handler each, taking an
   event code (`0x49` init, `0x53` resume, `0x54` tick, `0x57` draw, keys as
   ASCII) and re-arming their own timer. Ours is `snake_init`, `snake_key`,
   `snake_step`, driven by `core/menu.c`. The interface is declared in
   `core/game.h` and unused.
   *To do:* give Snake a `snake_handler(event)` that dispatches to the
   existing functions and asks the platform for its next tick with
   `platform_request_tick`. Have the menu code call handlers through a table
   indexed by game, as the firmware's plugin table does. No speed cost.

2. **Framebuffer layout.** The phone keeps one byte per column in strips of
   eight rows (bit `y & 7` of byte `x + 84 * (y / 8)`), 504 bytes. The core
   uses packed rows, 11 bytes each, because a Game Boy tile row is then one
   framebuffer byte.
   *To do:* this is a Game Boy convenience that leaked into the core. Either
   restore the phone's layout in the core and transpose 8x8 cells in the
   Game Boy layer (64 bit moves per changed cell, only for changed cells),
   or keep the packed rows and say plainly that the layout is the port's.
   Measure the transposition before choosing; the GBA does not care.

3. **Whole-board redraw.** On every tick the firmware clears and redraws the
   board (`snake_draw_241198`). The Game Boy is about ten times slower than
   the phone and cannot do that at level 9, so `snake_draw_step` draws only
   what a move changed.
   *To do:* keep `snake_draw` as the reference and treat `snake_draw_step`
   as an optional fast path a platform may use. `make test-snake` already
   proves the two give the same picture. The GBA should use the plain
   redraw.

4. **Dirty-cell tracking.** `lcd_dirty` in the core exists so a platform can
   update only what changed. The firmware has nothing like it in the games;
   its LCD driver sends what it is given.
   *To do:* acceptable as it stands, since it is the port's LCD driver
   interface, but it could move behind a platform hook so the drawing
   primitives match the firmware's.

5. **Menus.** The firmware builds them from widgets and a layout pass. Ours
   draws the same pixels directly.
   *To do:* nothing structural. Keep adding golden frames. Known gaps: the
   main menu's Games icon animation and
   the transitions the phone shows part-drawn.

6. **Timing.** The firmware counts scheduler ticks of 7.78125 ms. The core
   turns frames of 1/60 s into those ticks.
   *To do:* pass the platform's real frame length (59.73 Hz on both
   consoles) instead of assuming 60.

7. **Settings.** The firmware keeps five 4-byte records in EEPROM (top score
   big-endian, level, one unused byte). The Game Boy save uses the same
   record shape with a check byte in the unused position.
   *To do:* nothing; note that the check byte is the port's.

## Memory and Rotation

Both follow the firmware's handlers (`memory_handler_24075c`,
`rotation_handler_241fd0`) rule for rule: the deal and the shuffle draw from
`rand` in the firmware's order, so a seed gives the firmware's board; the
cursor and frame movement, the turn animation's steps and delays, the
opening turns, the clock and both score formulas are the firmware's.
Differences in structure:

- Each has a `_draw_changes` beside its full `_draw`, for the same reason
  as `snake_draw_step`; `make test-boards` proves the two agree.
- Memory's cursor blinks because the firmware draws that card into a second
  bit plane which the display code inverts every 64 ticks. The core has no
  such plane: `core/menu.c` keeps the phase and `memory_draw` inverts the
  one card.
- The phone never seeds `rand`, so its first deal after power-on is always
  the same. The port seeds it from the time at New game.
- Rotation's keys 7, 9 and 5, duplicates of 1 and 3, have no button.

## Things not yet matched to the firmware

- What happens when the snake fills the board (the firmware adds 100 points
  and ends the game).
- The seed `rand` has when a game starts.
- How the Game over and Last view pages are dismissed, and frames after
  eating food. Capture them in MAME.
- The beep on eating.

## Drawing speed on the Game Boy

The compiler's code for the drawing primitives' innermost loops (a column of
a bitmap, a column of a filled rectangle) was many times too slow for
Rotation's animation on its bigger boards. `core/lcd.c` keeps those loops as
two small functions in C, `lcd_column_fill` and `lcd_column_blit`, and the
Game Boy ROM replaces them with assembly in `platform/gb/crt0.s`
(`LCD_PLATFORM_COLUMNS`). The pixels are the same; the game code is not
involved. The 2x view of the phone's screen is likewise the Game Boy
layer's own: it keeps a copy of what is shown and converts only the tiles
whose pixels changed.

## Screen updates on the Game Boy

The phone writes to its LCD at any time and its screen changes in place. The
Game Boy only accepts video memory writes in short windows, so whole-screen
changes are copied with the LCD off, which blinks. To remove the blink
without touching the core: write the new screen into a second set of tiles
over several vertical blanks, then switch the tile map in one.
