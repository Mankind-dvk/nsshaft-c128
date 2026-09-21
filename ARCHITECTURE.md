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
| `src/platform_rows.inc` | Row descriptions, hidden-playfield clearing, span rendering and effect-state synchronization |
| `src/input.inc` | Paddle port setup and POTX sampling |
| `src/hud.inc` | Character POTX display and dial hand |
| `src/score.inc` | First-landing platform score, row claim flags and difficulty scheduling |
| `src/health.inc` | Every-third-point HP reward, HUD rendering, spike/top damage recovery |
| `src/character_select.inc` | Three-character title selection, paddle-neutral prompt/error feedback, frame/color lookup tables |
| `src/player.inc` | Player sprite, horizontal control, gravity, landing and spike collision |
| `src/fade_platforms.inc` | Disappearing-platform activation, crumbling stages, scrolling anchor and deletion |
| `src/spring_platforms.inc` | Spring glyph transitions, compression lifecycle and upward launch |
| `src/conveyor_platforms.inc` | Left/right texture animation, vertical fragments and full-texture restoration |
| `src/music.inc` | PAL raster IRQ, three SID voices, instruments, frequency and pattern tables |
| `src/charset_ids.inc` | Game glyph IDs/screen codes and separate text-layout codes |
| `src/platform_materials.inc` | Multicolor fragments, colors, transition and collision lookup tables |
| `src/charset.inc` | Writable 66-slot output and resource includes |
| `src/graphics_charset.inc` | Source charset 1: only custom graphics, unused slots zero |
| `src/text_charset.inc` | Source charset 2: imported uppercase font |
| `src/charset_loader.inc` | Eight-byte range copies and scene-entry composition |
| `src/assets.inc` | Includes the charset; sprite bitmaps, pointer templates, text and lookup tables |
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
3. select VIC bank 0 and configure mixed hires/multicolor text with ECM disabled;
4. initialize paddle input;
5. silence any SID state left by the previous program;
6. display the title screen and preview Elien, Ember, and Wasser;
7. use POTX thirds to highlight a character and lock it with Fire;
8. show the locked character facing left/right until POTX reaches the gameplay
   neutral range under `PRESS FIRE WHEN CHARACTER FACES YOU`;
9. accept a second Fire only from the forward-facing frame; off-center Fire
   shows the selected hurt frame for at least 12 PAL frames.

`new_game` resets subsystem state in dependency order:

1. compose the game charset with display blanked, then clear both screen buffers,
   Color RAM and platform descriptions;
2. reset disappearing- and spring-platform state, then reset conveyor texture
   patterns/counter and install their runtime glyphs;
3. seed the PRNG, reset score/HP/claim state, and generate platforms;
4. draw the fixed UI, six-digit score and three-digit HP;
5. initialize POTX display;
6. create the player and dial hand;
7. initialize the SID arrangement and install the raster IRQ;
8. copy the visible screen into the hidden buffer and enable the display.

`main_loop` targets one update sequence per PAL frame. If work misses raster 250,
polling waits for a later frame; it does not run catch-up updates:

1. wait for raster line 250;
2. sample POTX;
3. update the dial hand and POTX digits;
4. update horizontal player movement;
5. update gravity, upward spring motion and platform collision;
6. update activated disappearing and compressed spring platforms, then call
   `update_conveyor_animation` independently of passenger or world-scroll state;
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
| `$1d00-$27d5` | Main loop, video, platform descriptions/rendering, HUD, score, tables and state |
| `$2800-$2a0f` | Writable 66-glyph mixed-mode output charset |
| `$2a10-$2dd2` | SID player, frequency tables and 16-bar arrangement |
| `$2e00-$2f2c` | Disappearing-platform effect routines |
| `$3000-$303f` | Player normal frame |
| `$3040-$307f` | Player falling frame |
| `$3080-$30bf` | Player facing/moving right frame |
| `$30c0-$30ff` | Player facing/moving left frame |
| `$3100-$313f` | Writable dial-hand block |
| `$3140-$3779` | Player physics, ceiling recovery and spring/conveyor routines |
| `$3800-$39da` | Ceiling drawing, weighted generation, HP display and spike recovery |
| `$3a00-$3cbf` | Elien hurt plus all five Ember and five Wasser frames |
| `$3cc0-$3e64` | Character-selection code, prompt text and frame/color lookup tables |
| `$4000-$47ff` | Immutable graphics-only source charset, 2 KB |
| `$4800-$4fff` | Immutable uppercase text source charset, 2 KB |
| `$5000-$5358` | Material/collision tables, descriptor effect updates, conveyor animation and charset composition |
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
- Layout transitions clear only the hidden playfield, then draw platform spans
  from three 20-byte row-indexed arrays: start, width, and canonical FULL code.
  Width zero marks an empty row; FULL encodes both material and fade/spring state.
  There is at most one platform per row, with blank rows separating platforms.
  Fixed UI cells and sprite pointers are already mirrored and are not cleared.
- `draw_platform` registers the description and preserves X for initial seeding.
  A scene clear resets all descriptions. Coarse scrolling shifts descriptions
  with score rows before rebuilding FULL spans and registering the new bottom row.
  Fade/spring effects update the descriptor before transforming both matrices;
  deleting a fade platform clears its width so it cannot reappear on a redraw.
  Collision remains screen-based; direct writes of platform cells alone are no
  longer sufficient to create persistent world geometry.
- Double buffering covers matrices only. Glyphs and Color RAM are shared, and
  `$d018` changes the selected matrix immediately rather than queuing a next-frame
  flip. Correct visible timing still requires checking the raster deadline.
- The status panel is never included in platform transitions or collision scans.
- Collision row pointers reuse the playfield low/high address tables. Sampled
  screen codes index a 66-byte collision-type table; UPPER fragments map to none.
- Score digits are written to both screen buffers. A parallel 20-byte row table
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
  HP without changing score. Ordinary spike platforms consume HP and bounce the
  player; fixed ceiling spikes consume HP, cancel support/rise and drop the
  player 8 pixels below the tips. Either spike contact is fatal at zero HP;
  bottom exit always bypasses HP.
- The brick frame occupies rows 1/23 and columns 1/29/38. Frame 20 and normal
  platform 1 use multicolor red/white bricks; black rows outside the platform
  keep partial scrolling cells transparent-looking. White/green inverted spikes
  occupy row 2, columns 2..28. Only rows 3..22 scroll; HUD rows remain unchanged.
- Fixed geometry uses sprite-space screen origin Y=50, not the phase-biased
  platform base 43. Ceiling tips end at Y=73; the head touching that coordinate
  invokes ceiling damage. Carried motion and every spring-rise pixel test this
  immediately. Ceiling contact never awards score.
- Absorbed damage reloads a 16-frame timer. While it is nonzero, the hurt block
  overrides normal movement animation. The game-over screen also re-enables
  sprite 0 at a fixed centered position with the same hurt block.
- Disappearing platforms use intact red-brick/gray-mortar IDs 56/57/58 while
  idle, cracked IDs 7/16/17 immediately on activation, and broken IDs 59/60/61
  after 25 frames. Only the gray mortar erodes into black gaps; there is no
  color flashing. Test exact active-group membership, never a numeric interval.
  `fade_stage` selects cracked (0) or broken (1). `fade_platform_timer` starts
  at 50 and changes stage at 25; zero deletes the active platform. This is half
  the previous 100-frame lifetime: about one second instead of two on PAL.
- Game output: bodies 1..7, fragments 8..19, frame 20, ceiling 21, dial 22..39,
  HUD text 40..55, extra fade variants 56..61, SCORE letters S/C/R/E at 62..65.
  See [CHARSET_LAYOUT.md](CHARSET_LAYOUT.md).
- The dial stores only its 18 nonblank tiles, grouped at 22..39; its screen-code
  maps substitute character 0 for the six blank cells.
- D011=$1b disables ECM; D016=$18 enables mixed hires/multicolor text. Color RAM
  bit 3 selects each cell's mode. D021/D022/D023 are black/white/gray. Normal bricks
  and all fade stages use Color RAM=10; fade pairs 00/10/11 are black/gray/red.
  Springs use Color RAM=4 (purple). Conveyors use Color RAM=14 (8 | 6), with pairs 01/11 selecting
  white/blue; unused fragment rows stay black through pair 00.
- Fragment layouts use FULL-to-LOWER/UPPER tables once per platform span, not
  per screen cell. Complete layouts use the stored FULL code directly. Color
  writes cover occupied spans only: blank glyph 0 is black in both modes.
  Intermediate fine phases only rebuild glyph rows.
- Right conveyor cells use `$5f,$d7,$f5,$7d,$7d,$f5,$d7,$5f`; left cells use its
  horizontal mirror `$f5,$d7,$5f,$7d,$7d,$5f,$d7,$f5`. Mirroring reverses the four
  color pairs in each row without reversing the bits within a pair. Runtime
  `conveyor_left_pattern` and
  `conveyor_right_pattern` each hold eight mutable bytes; the source charset is
  unchanged. `conveyor_animation_counter` advances both textures by one color
  pair (two displayed pixels) every three frames, left and right respectively.
  Four horizontal phases repeat every 12 frames regardless of player support or
  whether the world scrolls that frame. Only the texture moves horizontally;
  platform boundaries and collision geometry do not.
- Each conveyor direction shares its FULL character with its LOWER transition
  fragment: FULL/LOWER slots 5/6 and UPPER slots 18/19 consume four slots total,
  without modal-letter aliases. Animation rebuilds these glyphs at the current
  vertical phase. Before a completed coarse scroll becomes visible, FULL is
  restored from the current horizontal pattern, never the initial source phase.
- Modal entry copies the first 64 glyphs from source charset 2; TEXT_* uses
  standard C64 uppercase screen codes. No modal letters alias game fragments.
- New-game entry reconstructs the output from custom graphics plus only the
  20 HUD glyphs it needs. Source charsets are never overwritten; the display
  remains hidden during composition. No new per-frame copy or raster IRQ is used.
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
- The music segment must end at or before `$2e00`, where platform effects begin.
- `stop_music` leaves IRQs disabled and does not restore the previous IRQ vector;
  the next round installs the music handler again. Returning to BASIC would need
  a separate machine-state restoration path.
