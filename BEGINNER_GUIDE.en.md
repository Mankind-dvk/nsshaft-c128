# Reading the NS-SHAFT C128 Assembly Source

This guide is for readers who already understand registers, memory, and
hexadecimal numbers, but have not yet written a complete 6502/8502 game. It
explains why the project is organized this way and where to begin reading. The
comments in the source describe local details; this guide connects the modules
into one complete execution path.

## 1. Three facts to establish first

### 1.1 This is a native C128 program

- The CPU is an 8502, but the source uses only the standard 6502 instruction set.
- The game runs in native C128 mode and never switches to C64 mode.
- Video uses the VIC-IIe 40-column output, not the VDC 80-column output.
- The CPU remains at 1 MHz while the display is visible to avoid unsafe VIC-IIe
  and 2 MHz CPU memory contention.

### 1.2 `main.s` is the only assembler entry point

The project uses a single translation unit assembled from several include files:

```text
main.s
  -> video.inc
  -> platforms.inc
  -> input.inc
  -> hud.inc
  -> player.inc
  -> fade_platforms.inc
  -> spring_platforms.inc
  -> conveyor_platforms.inc
  -> music.inc
  -> assets.inc
  -> state.inc
```

These files are not assembled into independent objects. ca65 processes them in
`.include` order and produces one object file, so every module can refer directly
to labels defined by another module.

### 1.3 There is no automatic register-saving convention

`JSR routine` only pushes the return address onto the hardware stack. Changes to
A, X, Y, and the processor flags are not restored automatically. Unless a
routine comment says otherwise, assume that all of them may be changed. Values
that must survive calls are stored in named bytes in `state.inc`.

## 2. Recommended reading order

Do not start with hundreds of lines of music tables or bitmap data. Read the
project in this order:

1. `src/main.s`: startup, new-game initialization, and per-frame call order.
2. `src/constants.inc`: hardware addresses and tuning values referenced elsewhere.
3. `src/state.inc`: the mutable state the program actually stores.
4. `src/input.inc` and `src/hud.inc`: the shortest complete input-to-display path.
5. `src/platforms.inc`: PRNG use, address tables, and `(pointer),Y` screen writes.
6. `src/player.inc`: 9-bit coordinates, fixed-point speed, and collision states.
7. `src/video.inc`: double buffering and custom-character pixel scrolling.
8. `src/fade_platforms.inc`, `src/spring_platforms.inc`, and
   `src/conveyor_platforms.inc`: platform lifecycles and runtime glyph reuse.
9. `src/music.inc`: raster IRQ handling and SID playback.
10. `src/assets.inc`: binary glyph and sprite data when graphics need editing.

## 3. ca65 and 6502 syntax quick reference

### 3.1 Numbers and addresses

```asm
lda #10          ; immediate decimal value 10
lda #$0a         ; immediate hexadecimal value $0a, still decimal 10
lda #%00001010   ; immediate binary value, also decimal 10
lda $d020        ; read address $d020, not an immediate value
```

`#` means the value itself. Without `#`, an operand is normally treated as a
memory address.

### 3.2 Low and high address bytes

The 6502 loads only 8 bits at a time. A 16-bit pointer must be split into two
bytes:

```asm
lda #<SCREEN_A
sta ROW_PTR
lda #>SCREEN_A
sta ROW_PTR+1
```

`<` selects the low eight bits of an address and `>` selects the high eight bits.

### 3.3 Cheap-local labels

A label beginning with `@` belongs to the nearest preceding non-local label:

```asm
routine_a:
@loop:
    ; ...

routine_b:
@loop:
    ; this is a different @loop
```

### 3.4 The carry flag

Carry participates in arithmetic and is also commonly used as a Boolean result:

```asm
clc              ; clear carry before addition
adc #40

sec              ; set carry before subtraction: no incoming borrow
sbc #8

sec              ; some routines use C=1 to mean true
rts
```

`BCC` branches when C=0; `BCS` branches when C=1.

## 4. What happens during build and load

Run:

```powershell
.\build.ps1
```

The script performs two steps:

1. `ca65` converts `src/main.s` and all included files into an object file.
2. `ld65` uses `c128-prg.cfg` to place every segment at a fixed address and
   write the PRG.

The first two PRG bytes contain the load address `$1c01`. After loading, BASIC
sees this line:

```basic
10 SYS 7424
```

Decimal 7424 is hexadecimal `$1d00`, the address of `start`.

## 5. Current memory map

| Address range | Purpose |
| --- | --- |
| `$0400-$07ff` | Screen buffer A and its sprite pointer table |
| `$0c00-$0fff` | Screen buffer B and its sprite pointer table |
| `$1c01-$1c0c` | BASIC 7 loader line |
| `$1d00-$269a` | Main loop, video, HUD, platform generation, and state |
| `$2800-$29ff` | 64-character custom character set |
| `$2a00-$2dae` | SID player and arrangement data |
| `$2e00-$2f37` | Disappearing-platform effect code |
| `$3000-$30ff` | Four player sprite frames |
| `$3100-$313f` | Writable dial-hand sprite |
| `$3140-$37a7` | Player physics, spring, and conveyor code |
| `$d800-$dbff` | Color RAM |

These fixed addresses are not arbitrary. VIC-IIe character sets and sprite
blocks have alignment requirements. The linker script ensures that code growth
cannot silently overwrite graphics data.

## 6. From `start` to `main_loop`

### 6.1 `start`

`start` runs once:

1. `SEI` temporarily disables IRQs.
2. The MMU maps RAM bank 0 with I/O visible.
3. The 8502 is forced to 1 MHz.
4. CIA2 selects VIC bank 0, covering `$0000-$3fff`.
5. VIC-IIe character mode, colors, and screen A are configured.
6. Paddle input is initialized and stale SID state is silenced.
7. The title screen is displayed and waits for an action button.

### 6.2 `new_game`

`new_game` runs at the beginning of every round:

1. Disable sprites and reset screen-flip and game-over state.
2. Clear both screen matrices and Color RAM.
3. Reset fade and spring state, then install runtime conveyor glyphs.
4. Seed the PRNG and create five initial platforms.
5. Draw the fixed UI, POTX display, and dial.
6. Place the player on the lowest safe platform.
7. Initialize SID music.
8. Copy the complete screen A image into screen B.

### 6.3 `main_loop`

The call order is fixed for every PAL frame:

```text
wait for the frame boundary
  -> read POTX
  -> update the five-position hand and digits
  -> update horizontal movement
  -> update vertical physics and collision
  -> update disappearing-platform timers
  -> update spring timers
  -> select the player sprite frame
  -> test game over
  -> advance platforms by one pixel when the speed counter expires
```

Platform scrolling occurs after physics deliberately. The player first resolves
collision against the current screen, then the platform world and any supported
player move upward together by one pixel.

## 7. Screen matrices, Color RAM, and ECM

A 40-column text screen contains 40x25=1000 screen codes. The address of a cell is:

```text
screen_address = screen_base + row * 40 + column
```

Extended Color Mode splits each screen code as follows:

```text
bits 7..6: select $d021/$d022/$d023/$d024
bits 5..0: select glyph 0..63
```

The project can therefore address only 64 custom glyphs. White normal platforms,
red spikes, and gray disappearing platforms use ECM background colors. Purple
springs and green conveyors use Color RAM foreground colors instead. A conveyor
clears glyph bits at the arrow pixels so that the black background shows through
the green block as a cut-out symbol.

## 8. Why the renderer is double-buffered

If code edits the visible screen one character at a time, the player can see a
partly old and partly new image. This project maintains two matrices:

- `active_screen` identifies the visible matrix.
- Transition loops write only the hidden matrix.
- One write to `$d018` displays the completed matrix on the next frame.

Color RAM has only one physical copy and does not switch with `$d018`. Purple
spring and green conveyor colors must therefore be moved explicitly when their
screen cells move to another row.

## 9. Pixel-smooth upward motion with custom characters

A platform remains attached to a logical character row, but is split between
two cells while crossing a character boundary:

- `UPPER`: pixels that have entered the preceding character row.
- `LOWER`: pixels that remain in the original row.

As `fine_scroll` decreases from 7 to 0:

1. Phase 7 replaces each full platform with UPPER and LOWER screen codes.
2. Intermediate phases rewrite only the eight rows of the dynamic glyphs.
3. At phase 0, UPPER becomes FULL in the preceding row and LOWER is cleared.
4. `fine_scroll` returns to 7 and a new logical row is generated at the bottom.

The code never writes this software phase into `$d011`, so the status panel does
not move vertically.

## 10. Random platform generation

`random_state_low/high` form a 16-bit Galois LFSR. `seed_random` mixes the KERNAL
clock, CIA timers, time-of-day registers, the current raster line, and RAM data,
so each startup receives a different layout. `random_byte` advances eight LFSR
bits and then XORs the high and low state bytes. Advancing only one bit and
treating the result as a new byte would make adjacent values share seven bits
and would bias platform types severely.

Every platform consumes random bytes in this order:

1. Width from 7 through 12 characters.
2. A legal horizontal starting column.
3. Type from 0 through 5.

There are six generated types plus one collision state for an activated fade
platform:

| Type | Collision result | Behavior |
| --- | ---: | --- |
| Normal | 1 | Safe support |
| Spikes | 2 | Immediate game over |
| Inactive fade | 3 | Activate its timer and support the player |
| Active fade | 4 | Continue support until deletion |
| Spring | 5 | Compress and then launch the player |
| Left conveyor | 6 | Support and push left by 2 pixels every 3 frames |
| Right conveyor | 7 | Support and push right by 2 pixels every 3 frames |

## 11. Why player coordinates use several bytes

VIC sprite X coordinates range from 0 through 511 and require 9 bits:

- `player_x_low` is written to `$d000`.
- Bit 0 of `player_x_high` is written to bit 0 of `$d010`.

The dial hand is sprite 4, whose ninth X bit is bit 4 of `$d010`. Player position
code must modify only bit 0 instead of overwriting the complete register.

The Y coordinate needs only 8 bits. `player_y` is the sprite's top edge, so the
bottom edge is:

```text
player_y + PLAYER_HEIGHT
```

## 12. Vertical speed in 1/8-pixel fixed point

The 6502 has no floating-point hardware. This project measures vertical speed
in units of 1/8 pixel:

- `player_fall_velocity=8` means one pixel per frame.
- `player_fall_fraction` stores the accumulated remainder below one pixel.

Falling speed increases each frame. Y changes only when the accumulator reaches
8. A spring begins with upward speed 24, initially moving as much as three pixels
in one frame, then loses one unit per frame until gravity resumes.

Separate unsigned upward and downward speed variables are easier to follow on a
6502 than repeated signed arithmetic.

## 13. Collision detection

`find_platform_below_player` performs four conversions:

1. Add 21 to `player_y` to obtain the feet Y coordinate.
2. Subtract the screen-matrix top and software scroll phase.
3. Shift right three times, equivalent to dividing by 8, to obtain a row.
4. Sample screen codes at sprite-relative X offsets 5 and 18.

A collision is returned only when the feet are 0 through 2 pixels from the
platform top. This prevents attachment from the side or underside. During a
scroll transition, only LOWER is treated as the logical platform anchor; UPPER
is a visual fragment only.

## 14. Stateful platform types

### 14.1 Disappearing platforms

After activation, the platform uses independent glyphs 60 through 62. The timer
changes only the top two screen-code bits to flash red and gray. At the end of
its lifetime, the active glyphs are removed from both screen buffers.

Only one activated disappearing platform is tracked at a time. This is a
deliberate tradeoff between limited ECM glyph slots and simple state handling.

### 14.2 Spring platforms

When the player lands:

1. Save `collision_row`.
2. Replace normal spring codes with compressed codes in both buffers.
3. Wait 8 frames.
4. Restore the normal glyph.
5. Remove support and assign upward speed 24.

If the player leaves during those 8 frames, `cancel_spring_platform` restores
the image without launching the player.

### 14.3 Left and right conveyors

Conveyors repeat a green block with a black cut-out `<` or `>`. A feet collision
returns 6 or 7, keeps the supported state, and uses an independent counter to
push two pixels every three frames after the current frame's paddle movement.
This equals the average normal paddle speed: opposite input mostly cancels the
conveyor, while input in the same direction adds to it. Once the player is
pushed beyond an edge, the next feet sample is empty and normal falling begins.

The 64-glyph ECM character set has no six consecutive free slots. The
implementation uses two techniques:

1. Each direction lets FULL also serve as LOWER during a scroll transition, so
   only one additional UPPER glyph is required.
2. Gameplay temporarily reuses the N, S, dash, and H title glyph slots. Before
   entering a title or game-over screen, `restore_modal_font_glyphs` restores
   their original bitmaps from a read-only template.

## 15. Five-position paddle control

| POTX | Dial position | Horizontal movement |
| --- | --- | --- |
| 0..63 | Far left | 2 pixels every 2 frames |
| 64..107 | Left | 2 pixels every 3 frames |
| 108..148 | Center | Stop |
| 149..191 | Right | 2 pixels every 3 frames |
| 192..255 | Far right | 2 pixels every 2 frames |

"Every three frames" and "every two frames" come from counter reload values 2
and 1. The count includes the frame that performs the action, so reloading 2
means the current frame plus two skipped frames.

## 16. Sprite pointers and the five-position dial hand

The sprite pointer table stores an address divided by 64. For example, the
normal player frame is at `$3000`, so its pointer is `$3000 / 64 = $c0`.

All four player frames remain resident and animation only changes the pointer.
The dial hand has one writable sprite block; changing position copies one of
five 63-byte templates into it. The dial face uses characters and consumes no
hardware sprite.

## 17. SID raster IRQ

Music must not depend on whether the main loop completes an expensive screen
transition during a particular frame. The VIC-IIe therefore generates an IRQ at
raster line 240, while the main loop synchronizes at line 250.

The IRQ path is:

1. Write 1 to `$d019` to acknowledge the interrupt.
2. Call `play_music_frame`.
3. Jump to the C128 KERNAL restore path at `$ff33` to restore registers and MMU
   state.

Do not place the music IRQ and main-loop synchronization on the same raster
line. The IRQ could return after the polling loop has missed that line, forcing
the main loop to wait one additional frame.

## 18. Safety checks when editing the code

After every change, run at least:

```powershell
.\build.ps1
```

Then inspect `build/nsshaft-c128.map`:

- `CODE` must end before `$2800`.
- `CHARSET` must occupy exactly `$2800-$29ff`.
- `MUSIC` and `EFFECTS` must not overlap the sprite region at `$3000`.
- `GAMEPLAY` must end inside its linker-script reservation.

Common problems:

| Symptom | Check first |
| --- | --- |
| Garbled characters | `$d018`, charset address, and low six ECM screen-code bits |
| Player flashes during screen flips | Sprite pointers in both SCREEN_A and SCREEN_B |
| HUD moves vertically | Whether `fine_scroll` was incorrectly written to `$d011` |
| Landing occurs 8 pixels too early | Whether UPPER was incorrectly accepted as support |
| Dial hand disappears | 63-byte templates and sprite 4 pointer `$3100/64` |
| No sound | `$d01a/$d019`, SID volume, and VICE Sound settings |
| Second round inherits old state | Whether the new variable is reset from `new_game` |
| N/S/-/H become arrows on a modal screen | Call `restore_modal_font_glyphs` before drawing it |

## 19. Exercises for beginning assembly programmers

1. Change `COLOR_YELLOW` and observe the player sprite color register.
2. Change `PLATFORM_MIN_WIDTH` and verify that starting columns remain legal.
3. Change `SPRING_COMPRESSION_FRAMES` and observe the compressed duration.
4. Change `POTX_FAR_LEFT_END/POTX_FAR_RIGHT_START` to tune the fast ranges.
5. Watch `player_y`, `player_rise_velocity`, and `player_fall_velocity` in the
   VICE monitor to understand spring state transitions.

Change one constant at a time and keep the previous PRG. If behavior breaks,
restore that change before modifying scrolling, collision, or memory layout at
the same time.
