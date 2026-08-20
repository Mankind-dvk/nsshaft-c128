# NS-SHAFT C128 source architecture

## Design choice

The project uses a subsystem-oriented, single-translation-unit ca65 layout.
`src/main.s` is the only assembler entry point and includes the other source
modules. This keeps the fixed C128 memory map and ca65 cheap-local labels simple,
while separating code by responsibility.

A fully independent object-file architecture would require every shared byte and
routine to be exported/imported. For this small 6502 game that adds interface
boilerplate without reducing runtime coupling. The include-module pattern keeps
the same generated machine code while making ownership and call flow explicit.

## Source map

| File | Responsibility |
| --- | --- |
| `src/main.s` | BASIC loader, C128/VIC-IIe bootstrap, new-game setup, frame loop, game-over transition |
| `src/constants.inc` | Hardware addresses, memory layout, screen codes, sprite slots, tuning constants |
| `src/video.inc` | Double buffering, pixel-scroll glyph transitions, modal screens, UI character layout |
| `src/platforms.inc` | PRNG seeding, platform type/width/position generation, platform drawing |
| `src/input.inc` | Paddle port setup and POTX sampling |
| `src/hud.inc` | Character POTX display and dial hand |
| `src/player.inc` | Player sprite, horizontal control, gravity, landing and spike collision |
| `src/fade_platforms.inc` | Disappearing-platform activation, flashing, scrolling anchor and deletion |
| `src/music.inc` | PAL raster IRQ, three SID voices, instruments, frequency and pattern tables |
| `src/assets.inc` | Custom charset, sprite bitmaps, pointer templates, text and lookup tables |
| `src/state.inc` | Mutable state bytes grouped in one visible RAM layout |

## Runtime call flow

`start` performs machine initialization once:

1. map C128 RAM bank 0 with I/O visible;
2. force visible VIC-IIe work to 1 MHz;
3. select VIC bank 0 and configure extended-color character mode;
4. initialize paddle input;
5. silence any SID state left by the previous program;
6. display the title screen and wait for an action button.

`new_game` resets subsystem state in dependency order:

1. clear both screen buffers;
2. seed and generate platforms;
3. draw the fixed UI;
4. initialize POTX display;
5. create the player and dial hand;
6. initialize the SID arrangement and install the raster IRQ;
7. copy the visible screen into the hidden buffer.

`main_loop` then runs one deterministic update sequence per frame:

1. wait for raster line 250;
2. sample POTX;
3. update the dial hand and POTX digits;
4. update horizontal player movement;
5. update gravity and platform collision;
6. update any activated disappearing platform;
7. process game over;
8. advance platform scrolling at the configured cadence.

The music does not depend on completion of `main_loop`. VIC-IIe raster line 240
dispatches `music_raster_irq`, while the main loop synchronizes at line 250.
Keeping these lines separate prevents the IRQ from making the polling loop miss
its frame boundary. The C128 KERNAL hardware entry has already saved
A/X/Y and the MMU configuration, so the music handler acknowledges the VIC,
advances the music phase, and jumps directly to the KERNAL IRQ restore path at
`$ff33`. It deliberately does not call the normal KERNAL IRQ service, which
would process the raster IRQ as a system display interrupt and disturb sprites.
This also avoids tempo drops when a character-transition frame takes longer
than one display frame.

## Memory map

| Range | Use |
| --- | --- |
| `$1c01-$1c0c` | BASIC 7 `SYS 7424` loader |
| `$1d00-$27c6` | Code, tables and mutable state |
| `$2800-$29ff` | 64-character custom character set |
| `$2a00-$2dae` | SID player, frequency tables and 16-bar arrangement |
| `$2e00-$2f37` | Disappearing-platform effect routines |
| `$3000-$303f` | Player normal frame |
| `$3040-$307f` | Player falling frame |
| `$3080-$30bf` | Player facing/moving right frame |
| `$30c0-$30ff` | Player facing/moving left frame |
| `$3100-$313f` | Writable dial-hand block |
| `$0400-$07ff` | Screen buffer A and its sprite pointers |
| `$0c00-$0fff` | Screen buffer B and its sprite pointers |
| `$d800-$dbff` | Shared VIC color RAM |

The linker configuration fixes the charset and sprite ranges. A build fails if
code grows into the reserved charset region or assets exceed their reserved
space.

## Sprite allocation

| Sprite | Use |
| --- | --- |
| 0 | Player |
| 1-3 | Free |
| 4 | Red paddle/dial hand |
| 5-7 | Free |

The four player animation blocks are memory frames selected by hardware sprite
0's pointer. They do not allocate hardware sprites 1-3.

## Important invariants

- Both screen buffers must contain the same sprite pointer values.
- `active_screen` must match the screen selected in `$d018`.
- Pixel-transition passes write every playfield cell directly to the hidden
  buffer; fixed UI cells are already mirrored and are not recopied per step.
- The status panel is never included in platform transitions or collision scans.
- Activated disappearing platforms use character indices 60-62; untriggered
  gray platforms continue to use the shared rectangular transition glyphs.
- `$d011` remains at phase 3; UI characters must never use the platform's
  software `fine_scroll` phase.
- `fine_scroll` describes the custom-glyph transition phase; it is not written
  into the low bits of `$d011`.
- Player collision and sprite Y positions use the same
  `SCREEN_ROW0_BASE_Y + fine_scroll` coordinate model.
- Paddle movement and dial direction use the same dead-zone constants.
- The music IRQ must not use shared zero-page scratch locations.
- Lead, bass and arpeggio patterns must each contain exactly 128 steps.
- The music segment must end below `$3000`, where sprite data begins.
