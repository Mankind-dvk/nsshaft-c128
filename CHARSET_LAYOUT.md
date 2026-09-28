# 双源字库与输出字库 / Two-source charset composition

## 1. 和老师示例的对应关系 / Relation to the teacher's example

老师的 `charsets/main.asm` 先装载两个输入字库，再由 `copyFromCharset` 和
`copyMemoryChunks` 抽取指定范围，拼到 `outputCharsetStart`。这是字形合并，
不是让一行里的每个字符分别选择两个硬件字库。

The example loads two inputs, selects ranges, and copies eight-byte glyphs to
one output. It merges glyphs; it does not let adjacent cells select different
hardware charsets.

本项目保留这个流程，但把交互输入的范围改为代码中的具名宏调用，把源数据
随 PRG 内嵌。没有照搬 C64 的 BASIC/KERNAL 入口、磁盘提示或演示用光栅中断。
C128 原生 MMU、VIC bank 0、双屏缓冲和 SID 中断架构保持不变。

The game uses named compile-time range selections and embeds both inputs in the
PRG. It does not copy the C64-specific BASIC/KERNAL entry, disk prompts, or demo
raster IRQs. Native C128 MMU setup, VIC bank 0, screen buffers, and SID IRQ remain.

| 老师示例 / Example | 本项目 / Project |
| --- | --- |
| `inputCharset1start` | `$4000`，第一源字库，仅自定义图形 / graphics only |
| `inputCharset2start` | `$4800`，第二源字库，大写文字 / uppercase font |
| `outputCharsetStart` | `$2800`，VIC 实际显示的合成字库 / live output |
| `copyMemoryChunks` | `copy_charset_glyphs`，逐个复制 8 字节字形 / eight-byte copies |
| `from` / `until` | `COPY_CHARSET_RANGE` 的源编号和数量 / source ID and count |

## 2. 文件与内存 / Files and memory

| 文件 / File | 内容 / Contents |
| --- | --- |
| `src/graphics_charset.inc` | 第一源库：平台、碎片初值、边框；无文字 / source 1, graphics only |
| `src/text_charset.inc` | 第二源库的二进制导入 / source 2 binary import |
| `assets/fonts/c64-upper.bin` | 老师 `c64.bin` 的前 2048 字节，未改像素 / unchanged uppercase half |
| `src/charset_ids.inc` | 游戏 `GLYPH_*` / `CHAR_*` 与菜单 `TEXT_*` 编号 / separate code namespaces |
| `src/charset.inc` | 可写输出区与资源文件引用 / output allocation and includes |
| `src/charset_loader.inc` | 范围复制、两种场景布局合成 / range copies and scene composition |
| `src/platform_materials.inc` | 砖/消失台阶碎片、颜色和转场表 / material fragments, colors and transition tables |
| `src/platform_rows.inc` | 平台描述和隐藏屏区间绘制 / platform descriptions and hidden-screen spans |
| `src/assets.inc` | 角色 Sprite、文字屏幕码 / character sprites and text codes |

| 地址 / Range | 用途 / Use |
| --- | --- |
| `$2800-$2a1f` | 544 字节可写混合模式输出，含 18 个空闲槽 / 68 slots including 18 free |
| `$4000-$47ff` | 2048 字节图形源库 / 256-slot immutable graphics source |
| `$4800-$4fff` | 2048 字节文字源库 / 256-slot immutable text source |
| `$5000-$5360` | 材质/碰撞查表、描述效果同步、传送带动画和字库合成 / material/collision tables, descriptor effects, conveyor animation and composition |

两套源库都保留完整 2 KB 格式，但不直接给 VIC 显示，因而可以放在当前
16 KB VIC bank 之外。CPU 将所需字形复制到 bank 内的 `$2800`，`$d018`
仍为 `$1a/$3a`，只随屏幕 A/B 切换。当前关闭 ECM，输出预留前 68 槽；
22–39 空闲，新增 SCORE 字母使用 62–65，传送带 LOWER 使用 66–67。
音乐从 `$2a20` 开始，不能在未调整内存布局前使用 68 及以上编号。

Both 2 KB sources are CPU-readable templates outside the active VIC bank. The
CPU copies selected glyphs into `$2800` inside that bank. `$d018` stays `$1a/$3a`
for the two screen buffers. ECM is off; 68 slots are allocated, 22..39 are free,
and music starts at $2a20. Indices 68 and above would reach adjacent music data.

## 3. 第一源库与游戏布局 / Graphics source and game layout

以下为十进制槽号。第一源库定义 0–21 和 56–61，其余全部补零；文字只能
从第二源库抽取，不能再混入第一源库。游戏输出先复制第一源库前 68 槽，
再将 20 个 HUD 字形放到 40–55 和 62–65。

Slots below are decimal. Source 1 defines 0–21 and 56–61; all other slots are zero.
Game output first copies source 1 slots 0–67, then imports 20 HUD glyphs into 40–55 and 62–65.

| 游戏输出槽 / Game slots | 内容 / Contents |
| --- | --- |
| 0 | 空白 / Blank |
| 1–7 | 普通、尖刺、弹簧、压缩弹簧、左/右传送、已激活消失台阶本体 / Bodies |
| 8–19 | 各平台专用滚动碎片 / Dedicated scrolling fragments |
| 20 | 红砖白缝 UI 外框和分隔列 / Red-brick, white-mortar frame/divider |
| 21 | 白绿多色固定倒尖刺 / Fixed white/green multicolor inverted spike |
| 22–39 | 原表盘释放的 18 个空闲槽 / 18 freed glyph slots |
| 40–49 | 从文字源库抽取的数字 0–9 / Imported digits |
| 50–55 | 从文字源库抽取的 H O P T X : / Imported HUD letters and colon |
| 56–58 | 未激活完整红砖灰缝 FULL/UPPER/LOWER / Intact idle fade |
| 59–61 | 灰缝完全脱落 FULL/UPPER/LOWER / Broken active fade |
| 62–65 | SCORE 新增字母 S C R E / Additional SCORE letters |
| 66–67 | 左右传送平台的 LOWER 碎片 / Conveyor LOWER fragments |

普通台阶槽 1 和边框槽 20 共用砖纹定义：`$f7,$f7,$55,$7f,$7f,$55,$f7,$f7`。
开启多色字符模式（D011=$1b，D016=$18），Color RAM=10；位对 00=黑、01=白、
10=灰、11=红。四列色块各占两个横向像素，图案仍是 8 行，逐行复制保持每次
上移一像素。滚动碎片未使用的行填 00，避免原 ECM 红/白背景填满空白区域。
文字和弹簧的 Color RAM 低于 8，保持单色精细图案。
尖刺使用 Color RAM=13，位对 01=白、11=绿。
弹簧使用 Color RAM=4（紫色）。传送平台使用 Color RAM=14，位对 01=白、11=蓝；裁切区的 00 仍为黑色。
该混合规则依据 [Commodore 原版手册](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_3/page_116.html)。

Normal platform 1 and frame 20 share `$f7,$f7,$55,$7f,$7f,$55,$f7,$f7`.
D011=$1b/D016=$18 enables mixed-mode characters. Color RAM=10 selects multicolor
with red local color; pairs 00/01/10/11 mean black/white/gray/red. Empty fragment
rows use 00; occupied rows copy the source pattern, preserving one-pixel vertical
motion. Text and springs keep Color RAM below 8 for hires.
Spikes use Color RAM=13: pairs 01/11 are white/green.
Springs use Color RAM=4 (purple). Conveyors use Color RAM=14: pairs 01/11 are white/blue and clipped rows use black 00.
See the [original color-pair table](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_3/page_117.html).

倒尖刺只画在顶框下方第 2 行的左侧游戏区（列 2–28），不占 sprite。
滚动从第 3 行开始，两张缓冲区始终保留倒尖刺行。尖端在 sprite 坐标 Y=73，
头顶接触扣 1 HP 并下移脱离，不计分；HP 已为 0 时才死亡。这里使用屏幕起点 Y=50，而不是平台
软件相位基值 43；起点依据 [Commodore 原版编程参考手册](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_3/page_139.html)。

The fixed ceiling occupies row 2, columns 2–28, without sprites. Only rows 3–22
scroll. Contact at the bottom tip (sprite-space Y=73) consumes 1 HP and drops
the player clear; only contact with zero HP is fatal. No score is awarded.
Fixed geometry uses origin Y=50, not the phase-biased moving-platform base 43.

平台资源规则 / Platform resource rules:

- 普通砖台阶使用 1/8/9，未激活红砖灰缝台阶使用 56/57/58，不共用碎片。
  Normal bricks use 1/8/9; intact idle fades use independent 56/57/58.
- 传送平台 FULL 使用 5/6，UPPER 使用 18/19，LOWER 独立使用 66/67。
  碎片重建不会改坏仍在可见屏上的完整平台；粗滚动完成前，FULL 从当前
  水平动画相位恢复，而不是回到初始图。
  Conveyor FULL 5/6, UPPER 18/19 and LOWER 66/67 have independent slots.
  Fragment rebuilding preserves visible FULL cells; restore the current
  horizontal texture phase at each completed coarse scroll.

向右传送平台源图为 `$5f,$d7,$f5,$7d,$7d,$f5,$d7,$5f`，向左平台使用其水平镜像
`$f5,$d7,$5f,$7d,$7d,$5f,$d7,$f5`。每行仅反转四个位对的排列，不反转位对内部
的颜色编码。每格为 4 列多色宽像素、8 行，即屏幕上的 8x8 像素。
开局时将各自源图复制到各 8 字节的
`conveyor_left_pattern` / `conveyor_right_pattern`，并重置 `conveyor_animation_counter`。
每三帧将每行循环左移/右移一个位对（两个显示像素），四相位共十二帧；
动画按当前纵向相位重建完整或上下碎片，独立于人物是否站上和本帧是否上移。
平台边界不横移，只有纹理横向滚动，推人规则仍为每三帧两像素。

The right source is `$5f,$d7,$f5,$7d,$7d,$f5,$d7,$5f`; the left is its horizontal
mirror `$f5,$d7,$5f,$7d,$7d,$5f,$d7,$f5`. Reverse the order of the four color pairs
per row, not the bits within each pair. Each tile is four wide color pixels by
eight rows, displayed as 8x8 pixels. New-game setup copies these into separate
eight-byte `conveyor_left_pattern` / `conveyor_right_pattern` buffers and resets
`conveyor_animation_counter`. Every three frames, rows rotate left/right by one
color pair (two display pixels); four phases take 12 frames. Glyphs are rebuilt
at the current vertical phase even without a passenger or an upward step that
frame. Platform boundaries do not move horizontally; passenger pushing remains
two pixels every three frames.

已激活消失台阶先用部分灰缝脱落的 7/16/17，25 帧后换成仅剩红砖的 59/60/61；
50 帧后消失，比原来 100 帧减半（PAL 约两秒缩短为一秒）。必须精确匹配对应组。
三个阶段 Color RAM=10 不变，位对 00=黑色空隙、10=灰色砂浆、11=红砖；
共享 01=白色不变，不再红灰闪烁，也不改全局调色板。
Active fade starts with cracked 7/16/17, changes to broken 59/60/61 at 25 frames,
and disappears at 50 frames, halving its previous 100-frame lifetime (about two
seconds to one on PAL). Match each group exactly. Color RAM stays 10 in all
three stages; pairs 00/10/11 select black gaps/gray mortar/red bricks. Shared 01
remains white; there is no color flashing or global palette change.

转场先清空隐藏屏的游戏区，再按平台描述绘制连续跨度。碎片阶段每个平台查
`fragment_lower_codes`、`fragment_upper_codes` 两张 68 项表；完整阶段直接使用
描述中保存的本体字符码，不再逐格识别和转换。每个平台查询一次
`game_character_colors`，只为占用格写 Color RAM；字形 0 全黑，空格无需改色。
Transitions clear the hidden playfield, then draw spans from platform descriptions.
Fragment layouts look up the two 68-entry lower/upper tables once per platform;
full layouts use the stored body code directly, without per-cell classification.
Each platform looks up its color once and writes Color RAM only for occupied cells.
Blank cells retain their color because glyph 0 is black in either mode.

## 4. 菜单文字布局 / Modal text layout

开始/结束界面调用 `load_text_charset`，直接将第二源库前 64 个字形复制到
输出。不再存在八字母补丁或字母/碎片别名。该页同时提供：

Modal entry imports source 2 slots 0–63. No partial-letter overlay remains.
The page simultaneously provides:

- `@`、A–Z；括号、英镑、方向箭头 / alphabet and several symbols
- 空格、常用标点、运算符 / space, punctuation and operators
- 0–9，以及 `: ; < = > ?`

`TEXT_A=1`、`TEXT_SPACE=32`、`TEXT_DIGIT_0=48`，这是 C64 **屏幕码**，
不是 ASCII 或 PETSCII。文字表使用 `TEXT_*`；例如菜单冒号是 `TEXT_COLON=58`，
而游戏 HUD 冒号为 `CHAR_COLON=55`。不要混用这两种布局的编号。

Use `TEXT_*` screen-code constants for modal text, not raw ASCII/PETSCII.
For example, modal colon is 58 while the game's imported colon is 55.

```asm
; 菜单文字页装载后 / After loading the modal text page
lda #TEXT_A
sta SCREEN_A+10*SCREEN_COLUMNS+12
lda #COLOR_WHITE
sta COLOR_RAM+10*SCREEN_COLUMNS+12
```

实际界面文本仍通过 `STORE_SCREEN_PAIR` 镜像到两张矩阵。菜单清屏用空格32，
游戏清屏用空白0。结束界面的排行榜已经复用这张文字页。

Actual UI routines mirror text to both matrices with `STORE_SCREEN_PAIR`.
Clear modal screens with space 32, game screens with blank 0. The game-over
leaderboard already uses this text page.

## 5. 生命周期和扩展 / Lifecycle and extension

1. 关闭可见显示，避免边复制边显示不同字形含义。
2. 游戏调用 `load_game_charset`；菜单调用 `load_text_charset`。
3. 清屏、绘制当前场景、重建所需 sprite 指针。
4. 调用 `enable_charset_display` 恢复显示；主循环不再装载源字库。

Hide display -> compose the scene charset -> clear/draw the scene -> enable
visibility. This happens on scene transitions, not every frame.

游戏每次开局都从图形源库重建，不依赖上一局的动态字形。源库永不被滚动
覆盖，动态更新只写输出字库的相关槽。只有图形源库是“纯自定义图形”；
游戏显示用的输出字库包含从第二源库合成进来的 HUD 文字。

Every new game rebuilds from immutable graphics. Only output glyphs are animated.
The first source is graphics-only; the game output intentionally combines graphics
and imported HUD text.

新增 HUD 符号时，先从当前空闲的 22–39 槽分配编号，再增加
`COPY_CHARSET_RANGE text_charset, 源编号, 目标编号, 数量`。不要把位图写回
图形源库。宏会检查源范围和输出 68 槽限制；新增完整游戏中字母表仍需重新
预算空间，不能因为源库有 256 字形就直接使用 256 种。

For another HUD symbol, allocate a free game slot and add a range import from
source 2. Source and destination bounds are asserted. A full in-game alphabet
still requires a new glyph-slot budget or an explicit output/music memory relocation.

## 6. 构建和回归 / Build and regression

```powershell
.\build.ps1
python -m pip install -r tests/requirements-charset.txt
python tests/test_charset.py
python tests/test_ceiling.py
python tests/test_brick_platforms.py
python tests/test_conveyor_material.py
```

完整旧版对照另需 `567c2e3` 构建出的 PRG 和标签文件放在独立目录：
Full baseline comparisons require the PRG and label file built from `567c2e3`:

```powershell
python tests/test_charset.py --baseline-dir D:\path\to\baseline
```

测试验证两源/输出边界、跨页和空范围复制、A-Z 与符号、菜单文字调用、三次
菜单/游戏往返、源库不变、滚动/碰撞/消失平台，以及原有角色帧未变。
旧 HUD 的字形在对照时先归一化为老师的字体，避免把预期字体差异当成游戏
回归。旧版像素对照只比较第 3 行以下，屏蔽边框格，并把普通砖台阶归一化为
同轮廓白色台阶；`test_brick_platforms.py` 不做归一化，单独检查完整砖纹、
所有滚动相位、顶部裁剪、消失台阶分级脱落不串色和出生台阶。边框砖纹、倒尖刺的固定行、
像素边界、扣血脱离/受击帧、零 HP 死亡和出生计分索引由 `test_ceiling.py` 单独验证。
可安装 Pillow 并加 `--render-directory` 输出字符画面。

`test_conveyor_material.py` 验证白蓝纹理、每三帧左右循环移位、垂直滚动时
保留水平相位，以及动画与推人计时独立、碰撞支撑边界不变。

Tests cover source/output boundaries, copies across pages, alphabet/symbols,
menu calls, repeated scene changes, immutable sources, scrolling/collision/fade,
and unchanged original character frames. Old HUD glyphs are normalized to the teacher's font
for pixel comparisons. Legacy comparisons exclude the new ceiling row and
mask changed frame cells and normalize normal-platform texture to its old solid
silhouette. Dedicated brick tests compare raw pixels through all phases, top
clipping, fade-stage isolation and spawning. Dedicated ceiling tests verify exact frame
pixels, geometry, persistence, HP recovery/hurt frames, zero-HP death, and seed/score boundaries.
Optional Pillow plus `--render-directory` saves renders.

Dedicated conveyor tests cover exact white/blue pixels, three-frame circular
shifts, horizontal-phase preservation while scrolling vertically, independent
animation/push timers, and unchanged collision/support bounds.

可用改材质前独立保存的 PRG/标签执行周期对照（只统计 6502 指令周期，不等于
VICE 帧率测量）：`python tests/test_brick_platforms.py --baseline-dir D:\path\to\pre-brick-build`。
An optional pre-brick build enables the eight-pixel CPU-cycle regression; this
does not model VIC bus stealing, interrupts or frame rate.

这些是编译后 6502 例程测试，不模拟 VIC 光栅时序、MMU 或 SID。VICE 和真机
验收请重新装载新版 PRG，不要继续使用包含旧布局的快照。教师原文件未修改。

These compiled-routine tests do not emulate VIC timing, MMU or SID. VICE/hardware
acceptance should load the new PRG, not resume an old-layout snapshot. The
teacher's original files remain unchanged.
