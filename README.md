# Native Commodore 128 NS-Shaft prototype

This is a native C128/8502 port using the VIC-IIe 40-column output. It starts
from a BASIC 7 program at `$1c01` (`SYS 7424`), runs entirely in C128 mode, and
does not enter C64 compatibility mode. The program forces 1 MHz operation,
maps the 8502 and VIC-IIe to RAM bank 0, and uses the 40-column display.

The game implements the core NS-Shaft descent loop. Platforms are custom
character tiles and move upward one pixel at a time. A player standing on a
platform is carried upward with it; walking beyond an edge starts a
gravity-driven fall. The player lands on the next supporting platform below,
while touching spikes or leaving the play area ends the current game. Gray
platforms flash red and disappear after being stepped on. Purple spring
platforms use a temporary `工`-shaped tile: landing compresses the tile, then
launches the player upward before normal gravity resumes. Green conveyor
platforms repeat a black cut-out `<` or `>` symbol and push a supported player
two pixels every three frames in the indicated direction.

The program opens on an `NS-SHAFT / PRESS FIRE` title screen. Paddle button 1,
paddle button 2, or joystick FIRE on control port 1 starts the game. After
`GAME OVER`, releasing and pressing the button starts a newly randomized round.
Scrolling and all gameplay sprites are paused on both menu screens.

Platform positions and widths are generated at runtime from a PRNG seeded with
the KERNAL clock, CIA timers/TOD, the current raster line, and power-up RAM. On
real hardware, load timing and RAM power-up state vary naturally. The VICE
launchers also pass a host-time-derived emulator seed so autostart launches do
not repeat VICE's deterministic power-up state.

Each generated platform also uses the same PRNG stream to select one of six
custom-character styles:

- normal platform
- red upward-pointing spike platform
- gray disappearing platform
- purple `工`-shaped spring platform
- green left conveyor (`<`)
- green right conveyor (`>`)

Position, width, and type are therefore reproducible together for a given seed.
The moving cells use VIC-IIe extended-color character mode. Normal platform
screen codes select `$d022` (white) and fill the complete 8x8 character.
Spike screen codes select `$d023` (red), while a black foreground mask shapes
each cell into a hires isosceles triangle. Disappearing platforms use `$d024`
(gray) until activated. Spring tiles use a black character background and
purple Color RAM foreground for their normal and compressed forms. Transition
code moves the spring color together with its screen cells. Conveyors use green
Color RAM foreground over black and invert the arrow pattern, producing a green
tile with a black cut-out symbol without consuming another ECM background slot.

A yellow 24x21 hires player sprite starts centered over the lowest initial
platform. A paddle connected to control port 1 controls horizontal movement:

- POTX 108..148: stop
- POTX 64..107: move left at the normal speed
- POTX 149..191: move right at the normal speed
- POTX 0..63 or 192..255: move in that direction at 1.5x speed

Normal movement is two pixels every three frames. The two extreme paddle ranges
move two pixels every two frames, exactly 1.5 times the normal average speed.
Movement remains clamped inside the playfield.
The hardware border is black. Cyan characters draw an inset frame on rows 1/23
and columns 1/38, while column 29 separates the game area from the eight-column
status panel at columns 30..37. `$d011` stays at the natural 25-row phase 3, so
the full 200-pixel character matrix is visible and no border-gap sprite is
required. `POTX:xxx` is rendered directly as eight fixed status-panel
characters.

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
frames, keeping the measured PAL tempo at approximately 129.2 BPM. Music stops
on the game-over screen and restarts from bar one with a new game.

The renderer combines:

- a fixed `$d011` phase of 3 so the status panel never bobs vertically and the
  25-row matrix exactly covers the display window
- runtime-updated fragment glyphs for pixel-smooth rectangular, spike, fade,
  spring, and conveyor platforms
- a character-row transition after every eight pixels
- two screen buffers at `$0400` and `$0c00`
- native code at `$1d00`
- a 64-character custom character set at `$2800-$29ff`
- SID player and pattern data at `$2a00-$2dae`
- sprite storage at `$3000-$313f`: player, three reserved/free slots, and the
  dial hand; only hardware sprites 0 and 4 are enabled
- player physics, spring and conveyor-platform code at `$3140-$37a7`

## Source layout

The source follows a subsystem-oriented ca65 layout. `src/main.s` contains the
BASIC loader, C128 bootstrap, new-game setup, and main frame loop. It includes
small modules for video, platform generation, input, HUD, player physics,
assets, and mutable state:

```text
src/
  main.s            program entry and frame orchestration
  constants.inc     hardware addresses and shared layout constants
  video.inc         scrolling, double buffering, modal screens, UI characters
  platforms.inc     PRNG and platform generation
  input.inc         paddle/POTX sampling
  hud.inc           character POTX display and dial hand
  player.inc        movement, gravity, landing and spike collision
  fade_platforms.inc disappearing-platform lifecycle and flashing
  spring_platforms.inc spring compression, restoration and upward launch
  conveyor_platforms.inc conveyor glyph transitions and modal-font restoration
  music.inc         raster IRQ, SID instruments and 16-bar music patterns
  assets.inc        custom characters, sprite bitmaps and immutable tables
  state.inc         mutable game state bytes
```

`tools/analyze_music.py` and `tools/render_spectrogram.py` document the offline
MP3-to-SID analysis step. They are development tools only and are not required
to build or run the PRG.

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

Build directly with the cc65 toolchain:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
& 'D:\C64Tools\cc65-snapshot-win64\bin\ca65.exe' src\main.s -g `
  -l build\nsshaft-c128.lst -o build\nsshaft-c128.o
& 'D:\C64Tools\cc65-snapshot-win64\bin\ld65.exe' -C c128-prg.cfg `
  -Ln build\nsshaft-c128.lbl -m build\nsshaft-c128.map `
  -o build\nsshaft-c128.prg build\nsshaft-c128.o
```

On Windows, build and run with:

```powershell
.\build.ps1
.\run-x128.ps1
```

On a real C128, use the 40-column video output, load the PRG normally from
BASIC 7, and run it:

```basic
LOAD"nsshaft-c128.prg",8
RUN
```
