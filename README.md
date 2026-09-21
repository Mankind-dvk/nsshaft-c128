# Native Commodore 128 NS-Shaft prototype

This is a native C128/8502 port using the VIC-IIe 40-column output. It starts
from a BASIC 7 program at `$1c01` (`SYS 7424`), runs entirely in C128 mode, and
does not enter C64 compatibility mode. The program forces 1 MHz operation,
maps the 8502 and VIC-IIe to RAM bank 0, and uses the 40-column display.

Short repeated assembly operations live in `src/macros.inc`; gameplay remains
in subsystem subroutines. See `ARCHITECTURE.md` and section 21 of both beginner
guides for macro contracts, expansion examples and byte-identical verification.

The game implements the core NS-Shaft descent loop. Platforms are custom
character tiles and move upward one pixel at a time. A player standing on a
platform is carried upward with it; walking beyond an edge starts a
gravity-driven fall. The player lands on the next supporting platform below.
The red-brick/white-mortar character frame has a fixed row of white/green downward spikes beneath its
top edge, over the playfield only. Head contact consumes 1 HP and drops the
player clear of the ceiling; contact with zero HP is fatal. Ordinary spike
platforms also consume HP when available; falling through the bottom ends the
game immediately regardless of HP. Disappearing platforms start as red bricks
with gray mortar. Stepping on one removes part of the mortar; after 25 frames
only the bricks remain, and after 50 frames the platform disappears. This halves
the previous 100-frame lifetime from about two seconds to one on PAL. Purple spring
platforms use a zigzag coil tile: landing compresses it into a solid lower-half block, then
launches the player upward before normal gravity resumes. White-and-blue conveyor
platforms scroll their repeating texture left or right by two pixels every three
frames while the platform itself continues upward. Their horizontal boundaries
stay fixed; a supported player is pushed two pixels every three frames in the
texture's direction.

The program opens on an `NS-SHAFT / PRESS FIRE` character-selection screen.
Three normal sprites are shown side by side: Elien, Ember, and Wasser. POTX
0..84, 85..170, and 171..255 select them respectively; the selected character
keeps its native color while the other two turn gray. Paddle button 1, paddle
button 2, or joystick FIRE on control port 1 locks that choice.

The locked character then moves to the center. It faces left below POTX 108,
faces right above 148, and uses its normal forward frame inside 108..148. This
is accompanied by the instruction `PRESS FIRE WHEN CHARACTER FACES YOU`. A
second released-and-pressed FIRE starts the round only while the sprite is
facing forward. Pressing FIRE while it faces left or right shows that
character's hurt frame for at least 12 PAL frames and does not start the round.
After `GAME OVER`, FIRE returns to the selection screen so the next round may
use another character. Scrolling and gameplay are paused throughout these modal
screens.

Platform positions and widths use a PRNG seeded from the KERNAL clock, CIA
timers/TOD and raster timing, mixed with two bytes inside the loaded PRG. Those
bytes are not power-up RAM entropy, and the KERNAL clock stops updating once
the game takes over IRQ service. Timing can vary the seed; uniqueness is not
guaranteed. The VICE launcher also supplies a host-time-derived emulator seed.

Each generated platform also uses the same PRNG stream to select from six
custom-character styles:

- normal platform
- white/green multicolor upward-pointing spike platform
- red-brick/gray-mortar disappearing platform
- purple zigzag spring platform
- white-and-blue conveyor with a left-scrolling texture
- white-and-blue conveyor with a right-scrolling texture

Position, width, and type are reproducible for the same seed, difficulty changes,
and PRNG call order.
Type selection uses score-gated weighted pools rather than exposing every hazard
at the start:

| Score | Unlocked types | Normal probability |
| ---: | --- | ---: |
| 0..4 | normal, spring | 75% |
| 5..9 | normal, spring, disappearing | 50% |
| 10..19 | previous types plus both conveyors | 37.5% |
| 20..39 | all types, including spikes | 25% |
| 40+ | all types with extra spike/fade weight | 12.5% |

Normal platforms may repeat freely. Every other individual type is limited to
two consecutive generated platforms; a third identical candidate consumes the
next deterministic PRNG byte and is selected again.
The display mixes multicolor and hires characters, with ECM disabled. Normal
platforms use the same red-brick/white-mortar texture as the frame; blank fragment
rows remain black while the pattern moves upward one pixel at a time. Shared
colors are black/white/gray; Color RAM=10 selects multicolor with red bricks.
Disappearing platforms use three independent intact/cracked/broken glyph sets
with Color RAM=10 throughout. Gray mortar progressively becomes black gaps;
neither the red bricks nor the shared palette flash. Conveyors use Color RAM=14
for white-and-blue multicolor textures. Spikes use white/green multicolor;
purple springs, the white dial and text remain hires. Row transitions clear
only the hidden playfield and draw contiguous spans from platform descriptions,
using one material lookup per span and synchronizing occupied cells' Color RAM.
Conveyor texture animation runs even without a passenger and on frames when the
world does not move upward; its four horizontal phases repeat every 12 frames.

The selected 24x21 hires character starts centered over the lowest initial
platform. Elien is yellow, Ember is red, and Wasser is cyan. A paddle connected
to control port 1 controls horizontal movement:

- POTX 108..148: stop
- POTX 64..107: move left at the normal speed
- POTX 149..191: move right at the normal speed
- POTX 0..63 or 192..255: move in that direction at 1.5x speed

Normal movement is two pixels every three frames. The two extreme paddle ranges
move two pixels every two frames, exactly 1.5 times the normal average speed.
Movement remains clamped inside the playfield.
The hardware border is black. Brick characters draw an inset frame on rows 1/23
and columns 1/38, while column 29 separates the game area from the eight-column
status panel at columns 30..37. `$d011` stays at the natural 25-row phase 3, so
the full 200-pixel character matrix is visible and no border-gap sprite is
required. `POTX:xxx` is rendered directly as eight fixed status-panel
characters. The score uses two fixed rows: `SCORE:` followed by `000000`. The
six digits count distinct platforms reached. Each platform carries a claim flag
that moves upward with its character row. The first airborne landing sets that
flag and awards one point; bouncing or returning to the same platform does not.
First contact with an unclaimed spike platform also awards its point before HP
damage or a fatal result is processed, so spikes remain risky but are not
scoreless.
The next centered row displays `HP:000`. Every three awarded platform points add
one HP without reducing score. A spike hit consumes one HP and bounces the
player upward; top-frame contact consumes one HP and drops the player back into
the shaft. Either hazard is fatal when HP is already zero. An absorbed hit gives
the selected character's hurt frame priority for 16 PAL frames. The same
selected hurt frame is centered between `GAME OVER` and `PRESS FIRE` on the
modal game-over screen.

World scrolling uses an 8-bit phase accumulator instead of a whole-frame delay.
The initial rate is 128/256 pixel per frame, matching the previous actual rate
of one pixel every two PAL frames. At 20, 40, 60, and 80 points it moves
through rates 160, 192, and 224, then an exact one-pixel-per-frame mode. These
are 1.25x, 1.5x, 1.75x, and 2x the starting speed. Because 256 cannot fit in an
8-bit rate, zero is reserved as the final 256/256 sentinel. Speed follows newly
reached platforms rather than scroll distance and cannot accelerate itself
through a score feedback loop. The SID sequencer uses the same score-level index
and therefore accelerates by the same five multipliers:

| Score | Scroll | Music tempo |
| ---: | ---: | ---: |
| 0..19 | 1.00x | about 129 BPM |
| 20..39 | 1.25x | about 161 BPM |
| 40..59 | 1.50x | about 193 BPM |
| 60..79 | 1.75x | about 226 BPM |
| 80+ | 2.00x | about 258 BPM |

Only event timing changes; SID note frequencies are not transposed. A modulo-512
music phase accumulator represents all five ratios exactly, including 1.25x and
1.75x.

The bottom of the status panel contains a fixed semicircular dial assembled
from custom characters and centered to the pixel in its eight columns. Only
sprite 4 is used for its red hand; no sprite is spent on the dial face. The
hand bitmap has five discrete angles: far left, left, straight up, right, and
far right. Its thresholds are 64, 108, 149, and 192, matching the two movement
speeds and the existing neutral dead zone.
The divider, POTX display, dial face, and hand remain stationary while the left
playfield scrolls. The supplied VICE launcher attaches paddles to control port
1 and maps POTX to the host mouse.

Gameplay now includes a three-voice SID arrangement derived from
`Quarter_Slot.mp3`. The MP3 is analysis/reference material only and is not
decoded by the C128. The runtime arrangement uses pulse-wave lead, triangle
bass, and a third voice shared by sawtooth arpeggios and short noise drums. A
raster IRQ at line 240 advances the music independently of expensive scrolling
frames. Its base PAL tempo is approximately 129 BPM and follows the current
score-speed tier up to 2x. Music stops on the game-over screen and restarts from
bar one at base tempo with a new game.

The renderer combines:

- a fixed `$d011` phase of 3 so the status panel never bobs vertically and the
  25-row matrix exactly covers the display window
- runtime-updated fragment glyphs for pixel-smooth rectangular, spike, fade,
  spring, and conveyor platforms
- row-indexed platform descriptions: start, width and canonical FULL code
  (including fade/spring state), totaling 60 bytes; width zero marks an empty row
- a hidden-playfield clear and descriptor-driven redraw at the two character
  layout transitions; the six intermediate pixel steps still only update glyphs
- two screen buffers at `$0400` and `$0c00`
- native code and state at `$1d00-$27d5`
- a writable 66-glyph mixed-mode output charset at `$2800-$2a0f`
- SID player and pattern data at `$2a10-$2dd2`
- sprite storage at `$3000-$313f`: Elien's four movement frames and the writable
  dial hand
- player physics, ceiling recovery and spring/conveyor code at `$3140-$3779`
- ceiling drawing, platform generation and HP/damage code at `$3800-$39da`
- the remaining eleven character frames at `$3a00-$3cbf`: Elien hurt plus all
  five Ember and five Wasser frames
- title-selection code, prompt text and character tables at `$3cc0-$3e64`
- graphics-only source charset at `$4000-$47ff`
- uppercase text source charset at `$4800-$4fff`, imported from the teacher's font
- material/collision tables, descriptor effect updates, conveyor animation and charset composition at `$5000-$5358`

Each logical row holds at most one platform, separated from the next platform by
blank rows. `draw_platform` records its description; coarse scrolling shifts
descriptions with score claims, and fade/spring changes update the stored FULL
code as well as both screen matrices. Collision still samples the visible
matrix. Clearing a scene also clears descriptions, preventing old platforms
from reappearing on the next round. Hidden matrices retain old contents until
cleared; the fixed frame, ceiling, HUD and sprite pointers are never cleared by
the playfield renderer. Glyph RAM and Color RAM remain shared, not double-buffered.

The loop targets one update per PAL frame; polling does not catch up missed
frames. CPU-cycle comparisons are not a substitute for VIC/IRQ timing checks.

Gameplay enables only hardware sprites 0 (selected character) and 4 (dial
hand). The title selector temporarily enables sprites 0..2 to preview the three
characters; the fifteen bitmap frames are memory blocks, not fifteen hardware
sprites.

## Source layout

The source follows a subsystem-oriented ca65 layout. `src/main.s` contains the
BASIC loader, C128 bootstrap, new-game setup, and main frame loop. It includes
small modules for video, platform generation, input, HUD, scoring/difficulty,
player physics, assets, and mutable state:

```text
src/
  main.s            program entry and frame orchestration
  constants.inc     hardware addresses and shared layout constants
  charset_ids.inc   game glyph IDs and separate modal text codes
  charset.inc       writable output charset and resource includes
  graphics_charset.inc source 1: custom graphics only
  text_charset.inc  source 2: imported uppercase font
  charset_loader.inc range copies and scene-entry composition
  video.inc         scrolling, double buffering, modal screens, UI characters
  platform_materials.inc fragments, colors, transition and collision lookup tables
  platforms.inc     PRNG and platform generation
  platform_rows.inc row descriptions, hidden-playfield clear and span rendering
  input.inc         paddle/POTX sampling
  hud.inc           character POTX display and dial hand
  score.inc         first-landing score, claim rows and scroll-rate scheduler
  health.inc        HP rewards, decimal HUD and hazard recovery
  character_select.inc title selection, neutral check and character frame tables
  player.inc        movement, gravity, landing and spike collision
  fade_platforms.inc disappearing-platform lifecycle and staged crumbling
  spring_platforms.inc spring compression, restoration and upward launch
  conveyor_platforms.inc horizontal texture animation and vertical glyph fragments
  music.inc         raster IRQ, SID instruments and 16-bar music patterns
  assets.inc        charset include, sprite bitmaps and immutable tables
  state.inc         mutable game state bytes
```

See [CHARSET_LAYOUT.md](CHARSET_LAYOUT.md) for the bilingual two-source charset
design and slot maps. Gameplay imports only
the HUD text it needs; modal screens use the complete basic uppercase text page.
Both inputs ship inside the PRG; no additional disk load or raster split is used.

The repository contains game source, font and sprite assets, build/launch
scripts, and project documentation. Local tests, audio-analysis tools,
presentations, archives, caches, and generated build output are excluded.

See `ARCHITECTURE.md` for the call flow, memory map, sprite allocation, and
cross-module invariants.

English-speaking contributors should start with `BEGINNER_GUIDE.en.md`. Every
source module also includes an English responsibility summary and English
routine contracts alongside the detailed Chinese comments, so both language
groups work from the same buildable source tree.

如果团队中有第一次接触 6502/8502 汇编的成员，建议先阅读
`BEGINNER_GUIDE.zh-CN.md`。该文档按实际运行顺序说明 BASIC 启动桩、主循环、
双缓冲、平滑滚动、平台碰撞、paddle 输入、sprite 和 SID 中断音乐；源码内也已
补充各子程序的输入、输出、寄存器破坏范围和关键硬件寄存器说明。

## Build and run

Install the [cc65 toolchain](https://cc65.github.io/getting-started.html) and
[VICE](https://vice-emu.sourceforge.io/). Use the C128 emulator (`x128`) in PAL
mode. Add the cc65 `bin` directory and VICE `bin` directory to PATH, then run:

```powershell
.\build.ps1
.\run-x128.ps1
```

Alternatively, supply your own installation paths (no fixed drive is required):

```powershell
.\build.ps1 -Cc65Bin 'C:\cc65\bin'
.\run-x128.ps1 -Cc65Bin 'C:\cc65\bin' -VicePath 'C:\VICE\bin\x128.exe'
```

The scripts also accept the `CC65_BIN` and `VICE_X128` environment variables.
The output is `build/nsshaft-c128.prg`; build output is generated locally.
All font data required for assembly is included under `assets/fonts/`.

On a real C128, use the 40-column video output, load the PRG normally from
BASIC 7, and run it:

```basic
LOAD"nsshaft-c128.prg",8
RUN
```
