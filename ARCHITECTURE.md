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
| `src/spring_platforms.inc` | Spring glyph transitions, compression lifecycle and upward launch |
| `src/conveyor_platforms.inc` | Left/right conveyor glyph transitions and restoration of reused menu-font slots |
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
2. reset disappearing- and spring-platform state, then install conveyor glyphs;
3. seed and generate platforms;
4. draw the fixed UI;
5. initialize POTX display;
6. create the player and dial hand;
7. initialize the SID arrangement and install the raster IRQ;
8. copy the visible screen into the hidden buffer.

`main_loop` then runs one deterministic update sequence per frame:

1. wait for raster line 250;
2. sample POTX;
3. update the dial hand and POTX digits;
4. update horizontal player movement;
5. update gravity, upward spring motion and platform collision;
6. update activated disappearing and compressed spring platforms;
7. select the player animation frame and process game over;
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
| `$1d00-$269a` | Main loop, video, platform generation, HUD, tables and mutable state |
| `$2800-$29ff` | 64-character custom character set |
| `$2a00-$2dae` | SID player, frequency tables and 16-bar arrangement |
| `$2e00-$2f37` | Disappearing-platform effect routines |
| `$3000-$303f` | Player normal frame |
| `$3040-$307f` | Player falling frame |
| `$3080-$30bf` | Player facing/moving right frame |
| `$30c0-$30ff` | Player facing/moving left frame |
| `$3100-$313f` | Writable dial-hand block |
| `$3140-$37a7` | Player physics, spring-platform and conveyor-platform routines |
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
- Spring platforms reuse six character slots that were blank inside the dial
  tiles. The dial screen-code tables substitute character 0 at those cells.
- Spring cells use purple Color RAM foreground; fragment transitions move that
  color one row upward with the spring while all other platform masks stay black.
- Conveyor cells use green Color RAM foreground and black cut-out arrows. During
  gameplay they reuse character slots 19-22; menu entry restores the original
  N/S/-/H glyphs before drawing any title or prompt text.
- Each conveyor direction shares its FULL character with its LOWER transition
  fragment. The full arrow bitmap must be restored before a completed coarse
  scroll is flipped to the visible screen.
- Conveyor support is resolved inside the vertical collision state machine and
  uses an independent delay counter to push two pixels every three frames after
  the normal paddle movement.
- A compressed spring is transformed in both screen buffers and across the
  three-row transition window; walking off restores it without launching.
- `$d011` remains at phase 3; UI characters must never use the platform's
  software `fine_scroll` phase.
- `fine_scroll` describes the custom-glyph transition phase; it is not written
  into the low bits of `$d011`.
- Player collision and sprite Y positions use the same
  `SCREEN_ROW0_BASE_Y + fine_scroll` coordinate model.
- Paddle movement and the five dial angles share the same dead-zone and outer
  speed thresholds. The two extreme ranges use a one-frame movement delay;
  inner left/right ranges retain the two-frame delay.
- The music IRQ must not use shared zero-page scratch locations.
- Lead, bass and arpeggio patterns must each contain exactly 128 steps.
- The music segment must end below `$3000`, where sprite data begins.
