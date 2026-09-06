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
| `src/macros.inc` | Short inline operations with explicit register/flag contracts; no runtime state |
| `src/video.inc` | Double buffering, pixel-scroll glyph transitions, modal screens, UI character layout |
| `src/platforms.inc` | PRNG seeding, platform type/width/position generation, platform drawing |
| `src/input.inc` | Paddle port setup and POTX sampling |
| `src/hud.inc` | Character POTX display and dial hand |
| `src/score.inc` | First-landing platform score, row claim flags and difficulty scheduling |
| `src/health.inc` | Every-third-point HP reward, HUD rendering, spike/top damage recovery |
| `src/character_select.inc` | Three-character title selection, paddle-neutral prompt/error feedback, frame/color lookup tables |
| `src/player.inc` | Player sprite, horizontal control, gravity, landing and spike collision |
| `src/fade_platforms.inc` | Disappearing-platform activation, flashing, scrolling anchor and deletion |
| `src/spring_platforms.inc` | Spring glyph transitions, compression lifecycle and upward launch |
| `src/conveyor_platforms.inc` | Left/right conveyor glyph transitions and restoration of reused menu-font slots |
| `src/music.inc` | PAL raster IRQ, three SID voices, instruments, frequency and pattern tables |
| `src/assets.inc` | Custom charset, sprite bitmaps, pointer templates, text and lookup tables |
| `src/state.inc` | Mutable state bytes grouped in one visible RAM layout |

## Macro layer

`main.s` includes `constants.inc` then `macros.inc` before executable modules.
Uppercase macro calls expand inline; lowercase routines still use `jsr`/`jmp`.
The seven helpers cover immediate word stores, D018 selection, paired screen
writes, paired sprite-pointer writes, register-bit updates and raster IRQ ACK.
They replace existing instruction sequences, not existing subroutine calls.
Consequently this refactor preserves the PRG bytes, addresses and cycle counts.

`STORE_SCREEN_PAIR` and `STORE_SPRITE_POINTER` consume A and preserve all CPU
registers and flags; the other helpers overwrite A and N/Z. Optional X/Y indices
are runtime inputs, whereas addresses, masks and offsets are compile-time inputs.
Color RAM writes, `active_screen`, MMU mapping and interrupt masking remain the
caller's responsibility. Read/modify/write helpers must not be used for IRQ
status registers or D011. The music IRQ still returns through the C128 KERNAL;
there is deliberately no PUSH/POP wrapper or replacement RTI sequence.

See section 21 of both beginner guides for expansion examples and debugging.

## Runtime call flow

`start` performs machine initialization once:

1. map C128 RAM bank 0 with I/O visible;
2. force visible VIC-IIe work to 1 MHz;
3. select VIC bank 0 and configure extended-color character mode;
4. initialize paddle input;
5. silence any SID state left by the previous program;
6. display the title screen and preview Elien, Ember, and Wasser;
7. use POTX thirds to highlight a character and lock it with Fire;
8. show the locked character facing left/right until POTX reaches the gameplay
   neutral range under `PRESS FIRE WHEN CHARACTER FACES YOU`;
9. accept a second Fire only from the forward-facing frame; off-center Fire
   shows the selected hurt frame for at least 12 PAL frames.

`new_game` resets subsystem state in dependency order:

1. clear both screen buffers;
2. reset disappearing- and spring-platform state, then install conveyor glyphs;
3. seed the PRNG, reset score/HP/claim state, and generate platforms;
4. draw the fixed UI, six-digit score and three-digit HP;
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
7. award a point if an airborne landing reached any unclaimed platform,
   including a spike, and award one HP on every third point;
8. on scores 20/40/60/80, apply the same 1.25x/1.5x/1.75x/2x level to world
   scrolling and the SID event sequencer;
9. consume HP and detach/bounce on spike or top-frame damage, while bottom exit
   requests game over immediately;
10. select the player animation frame and process game over;
11. add the current difficulty rate to the scroll phase accumulator;
12. on accumulator carry, advance the world one pixel.

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
| `$1d00-$27e4` | Main loop, video, platform drawing, HUD, scoring, tables and mutable state |
| `$2800-$29ff` | 64-character custom character set |
| `$2a00-$2dc2` | SID player, frequency tables and 16-bar arrangement |
| `$2e00-$2f37` | Disappearing-platform effect routines |
| `$3000-$303f` | Player normal frame |
| `$3040-$307f` | Player falling frame |
| `$3080-$30bf` | Player facing/moving right frame |
| `$30c0-$30ff` | Player facing/moving left frame |
| `$3100-$313f` | Writable dial-hand block |
| `$3140-$37d4` | Player physics, spring-platform and conveyor-platform routines |
| `$3800-$39ed` | Weighted platform generation, HP display and hazard recovery routines |
| `$3a00-$3cbf` | Elien hurt plus all five Ember and five Wasser frames |
| `$3cc0-$3e8f` | Character-selection code, prompt glyphs and frame/color lookup tables |
| `$0400-$07ff` | Screen buffer A and its sprite pointers |
| `$0c00-$0fff` | Screen buffer B and its sprite pointers |
| `$d800-$dbff` | Shared VIC color RAM |

The linker configuration fixes the charset and sprite ranges. A build fails if
code grows into the reserved charset region or assets exceed their reserved
space.

## Sprite allocation

| Sprite | Use |
| --- | --- |
| 0 | Selected player; left preview on the title selector |
| 1 | Middle preview during title selection; otherwise free |
| 2 | Right preview during title selection; otherwise free |
| 3 | Free |
| 4 | Red paddle/dial hand |
| 5-7 | Free |

Each of the three characters has normal/fall/right/left/hurt bitmap blocks.
Elien's movement frames occupy `$3000-$30ff`; the other eleven blocks occupy
`$3a00-$3cbf`. Gameplay selects one of these fifteen memory blocks through
hardware sprite 0's pointer. Only the selector temporarily uses sprites 1 and 2.

## Important invariants

- Both screen buffers must contain the same sprite pointer values.
- `selected_character` is changed only by the title selector and intentionally
  survives `new_game`. Every gameplay and game-over frame lookup uses it.
- Character-selection POTX thirds are 0..84, 85..170, and 171..255. The second
  Fire is accepted only in the gameplay-neutral 108..148 range, after a full
  press/release debounce. Off-center Fire selects the character's hurt block
  for at least 12 frames, then requires another press.
- `active_screen` must match the screen selected in `$d018`.
- Pixel-transition passes write every playfield cell directly to the hidden
  buffer; fixed UI cells are already mirrored and are not recopied per step.
- The status panel is never included in platform transitions or collision scans.
- Score digits are written to both screen buffers. A parallel 21-byte row table
  moves with each coarse scroll and marks platforms claimed on first landing.
  Spike contact claims and scores the row before damage handling, so surviving
  or dying on the same spike cannot award it again.
  Scores 20/40/60/80 select 1.25x/1.5x/1.75x/2x speed. Rates through 224 use
  the phase accumulator; rate zero is the exact 256/256 every-frame sentinel.
- The same score-level index selects SID tempo increments 44/55/66/77/88. The
  music accumulator wraps at 512, so these are exact 1x/1.25x/1.5x/1.75x/2x
  ratios rather than rounded byte increments. Pitch tables never change.
- Platform-generation levels change at scores 5/10/20/40. Their weighted pools
  reduce normal-platform probability from 75% to 12.5% while unlocking fade,
  conveyors, and spikes. Normal may repeat freely; every other type is rejected
  when it would be the third identical result in succession.
- `HP:000` is mirrored to both buffers. Every third awarded platform increments
  HP without changing score. Spikes bounce and top contact detaches the player
  after consuming HP; bottom exit bypasses HP and remains immediately fatal.
- Absorbed damage reloads a 16-frame timer. While it is nonzero, the hurt block
  overrides normal movement animation. The game-over screen also re-enables
  sprite 0 at a fixed centered position with the same hurt block.
- Activated disappearing platforms use character indices 60-62; untriggered
  gray platforms continue to use the shared rectangular transition glyphs.
- Spring platforms reuse six character slots that were blank inside the dial
  tiles. The dial screen-code tables substitute character 0 at those cells.
- Spring cells use purple Color RAM foreground; fragment transitions move that
  color one row upward with the spring while all other platform masks stay black.
- Conveyor cells use green Color RAM foreground and black cut-out arrows. During
  gameplay they reuse N/S/dash/V. H remains stable for the HP label; modal entry
  restores N/S/dash/V before drawing any title or prompt text.
- Each conveyor direction shares its FULL character with its LOWER transition
  fragment. The full arrow bitmap must be restored before a completed coarse
  scroll is flipped to the visible screen.
- The centering prompt temporarily installs W/C/Y/U in transition glyph slots
  31-34. No transition cells are visible on the modal screen, and the first
  gameplay transition rebuilds all four slots before using them.
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
