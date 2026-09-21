; C128 原生版本共用的编译期常量。
;
; 本文件只保存地址和不可变的布局参数。将这些定义集中在一处，能够避免
; 显示、输入、UI 和游戏逻辑模块各自维护不一致的内存映射。
;
; English summary:
;   Compile-time addresses and immutable layout/tuning values live here. `$`
;   introduces hexadecimal, `%` binary, `<`/`>` select address bytes, and these
;   definitions consume no runtime RAM. Keep hardware registers named and keep
;   playfield calculations inside the brick frame and below the ceiling spikes.
;
; 初学者语法提示：
;   $1234       十六进制常量；%10101010 是二进制常量。
;   NAME = ...  汇编期常量，不会在运行时占用 RAM。
;   <label      取地址低字节；>label 取地址高字节。
;   |           汇编期按位 OR，例如 MULTICOLOR_CELL | COLOR_WHITE。
;   +、-、*     这里都由 ca65 在汇编期计算，不消耗 8502 运行时间。
;
; 屏幕矩阵与双栏布局。砖纹框内的第 29 列是同纹理 UI 分隔条，因此平台
; 生成、软件滚动和碰撞采样都必须避开框线与状态栏。
; EN: The playfield excludes the brick frame, ceiling spikes and HUD divider.
SCREEN_A                = $0400
SCREEN_B                = $0c00
COLOR_RAM               = $d800
SCREEN_COLUMNS          = 40
SCREEN_ROWS             = 25
UI_FRAME_TOP_ROW        = 1
UI_FRAME_BOTTOM_ROW     = 23
UI_FRAME_LEFT_COLUMN    = 1
UI_FRAME_RIGHT_COLUMN   = 38
UI_FRAME_WIDTH          = UI_FRAME_RIGHT_COLUMN - UI_FRAME_LEFT_COLUMN + 1
UI_FRAME_HEIGHT         = UI_FRAME_BOTTOM_ROW - UI_FRAME_TOP_ROW + 1
CEILING_SPIKE_ROW       = UI_FRAME_TOP_ROW + 1
PLAYFIELD_TOP_ROW       = CEILING_SPIKE_ROW + 1
PLAYFIELD_BOTTOM_ROW    = UI_FRAME_BOTTOM_ROW - 1
PLAYFIELD_ROW_COUNT     = PLAYFIELD_BOTTOM_ROW - PLAYFIELD_TOP_ROW + 1
PLAYFIELD_FIRST_COLUMN  = UI_FRAME_LEFT_COLUMN + 1
UI_DIVIDER_COLUMN       = 29
STATUS_FIRST_COLUMN     = UI_DIVIDER_COLUMN + 1
PLAYFIELD_LAST_COLUMN   = UI_DIVIDER_COLUMN - 1
DIAL_FIRST_COLUMN       = STATUS_FIRST_COLUMN
DIAL_TOP_ROW            = 20
POTX_ROW                = UI_FRAME_TOP_ROW + 1
SCORE_ROW               = POTX_ROW + 2
HP_ROW                  = SCORE_ROW + 2
HP_FIRST_COLUMN         = STATUS_FIRST_COLUMN + 1
PLAYFIELD_BOTTOM_OFFSET = PLAYFIELD_BOTTOM_ROW * SCREEN_COLUMNS
POTX_DISPLAY_OFFSET     = POTX_ROW * SCREEN_COLUMNS + STATUS_FIRST_COLUMN
SCORE_DISPLAY_OFFSET    = SCORE_ROW * SCREEN_COLUMNS + STATUS_FIRST_COLUMN
SCORE_DIGITS_OFFSET     = SCORE_DISPLAY_OFFSET + SCREEN_COLUMNS
HP_DISPLAY_OFFSET       = HP_ROW * SCREEN_COLUMNS + HP_FIRST_COLUMN
SCORE_DIGIT_COUNT       = 6
HP_DIGIT_COUNT          = 3

; 渲染模块使用的 VIC-IIe、MMU 和 sprite 寄存器。$d000-$d02e 属于
; 内存映射 I/O：对这些“地址”执行 STA 实际是在配置视频芯片，而不是写 RAM。
VIC_SPRITE_0_X          = $d000
VIC_SPRITE_0_Y          = $d001
VIC_SPRITE_1_X          = $d002
VIC_SPRITE_1_Y          = $d003
VIC_SPRITE_2_X          = $d004
VIC_SPRITE_2_Y          = $d005
VIC_SPRITE_3_X          = $d006
VIC_SPRITE_3_Y          = $d007
VIC_SPRITE_4_X          = $d008
VIC_SPRITE_4_Y          = $d009
VIC_SPRITE_5_X          = $d00a
VIC_SPRITE_5_Y          = $d00b
VIC_SPRITE_6_X          = $d00c
VIC_SPRITE_6_Y          = $d00d
VIC_SPRITE_7_X          = $d00e
VIC_SPRITE_7_Y          = $d00f
VIC_SPRITE_X_HIGH       = $d010
VIC_CONTROL_1           = $d011
VIC_CONTROL_2           = $d016
VIC_RASTER              = $d012
VIC_SPRITE_ENABLE       = $d015
VIC_SPRITE_Y_EXPAND     = $d017
VIC_MEMORY_POINTERS     = $d018
VIC_SPRITE_PRIORITY     = $d01b
VIC_SPRITE_MULTICOLOR   = $d01c
VIC_SPRITE_X_EXPAND     = $d01d
VIC_IRQ_STATUS          = $d019
VIC_IRQ_ENABLE          = $d01a
VIC_BORDER_COLOR        = $d020
VIC_BACKGROUND_COLOR    = $d021
VIC_MULTICOLOR_1        = $d022
VIC_MULTICOLOR_2        = $d023
VIC_BACKGROUND_COLOR_3  = $d024
VIC_SPRITE_0_COLOR      = $d027
VIC_SPRITE_1_COLOR      = $d028
VIC_SPRITE_2_COLOR      = $d029
VIC_SPRITE_3_COLOR      = $d02a
VIC_SPRITE_4_COLOR      = $d02b
VIC_SPRITE_5_COLOR      = $d02c
VIC_SPRITE_6_COLOR      = $d02d
VIC_SPRITE_7_COLOR      = $d02e
VIC_CPU_SPEED           = $d030
MMU_RAM_CONFIG          = $d506
MMU_CONFIG              = $ff00
MMU_CONFIG_BANK0_IO     = $0e
; 25 行模式的自然垂直相位是 3：字符矩阵从光栅线 $33 开始，完整覆盖
; 200 条可视线，不需要 sprite 填补顶部或底部的相位空隙。
VIC_CONTROL_1_TEXT      = $1b

; CIA 端口与定时器。DDR（Data Direction Register）中的 1 表示输出、0 表示
; 输入。CIA2 端口 A 的位 0..1 用于选择 VIC bank 0。
CIA1_PORT_A             = $dc00
CIA1_PORT_B             = $dc01
CIA1_DDR_A              = $dc02
CIA1_DDR_B              = $dc03
CIA2_PORT_A             = $dd00
CIA2_DDR_A              = $dd02

CIA1_TIMER_A_LOW        = $dc04
CIA1_TIMER_A_HIGH       = $dc05
CIA1_TIMER_B_LOW        = $dc06
CIA1_TOD_TENTHS         = $dc08
CIA1_TOD_SECONDS        = $dc09
CIA1_TOD_MINUTES        = $dc0a
CIA1_INTERRUPT_CONTROL  = $dc0d
CIA2_INTERRUPT_CONTROL  = $dd0d

; 将 KERNAL 维护的时钟字节混入游戏伪随机数发生器的种子。
KERNAL_JIFFY_HIGH       = $a0
KERNAL_JIFFY_MIDDLE     = $a1
KERNAL_JIFFY_LOW        = $a2
KERNAL_IRQ_VECTOR       = $0314
KERNAL_IRQ_RETURN       = $ff33

SID_POTX                = $d419
PADDLE_PORT_1_SELECT    = $40

; SID 三声道音乐播放器使用的寄存器。每个硬件声道包含频率、波形/门控和
; ADSR 包络寄存器。音乐代码位于 $2a10，不占用字符集和 sprite 数据区；
; 播放器由光栅 IRQ 每个 PAL 帧调用一次。
SID_VOICE_1_FREQ_LOW    = $d400
SID_VOICE_1_FREQ_HIGH   = $d401
SID_VOICE_1_PULSE_LOW   = $d402
SID_VOICE_1_PULSE_HIGH  = $d403
SID_VOICE_1_CONTROL     = $d404
SID_VOICE_1_ATTACK      = $d405
SID_VOICE_1_SUSTAIN     = $d406
SID_VOICE_2_FREQ_LOW    = $d407
SID_VOICE_2_FREQ_HIGH   = $d408
SID_VOICE_2_CONTROL     = $d40b
SID_VOICE_2_ATTACK      = $d40c
SID_VOICE_2_SUSTAIN     = $d40d
SID_VOICE_3_FREQ_LOW    = $d40e
SID_VOICE_3_FREQ_HIGH   = $d40f
SID_VOICE_3_CONTROL     = $d412
SID_VOICE_3_ATTACK      = $d413
SID_VOICE_3_SUSTAIN     = $d414
SID_FILTER_CUTOFF_LOW   = $d415
SID_FILTER_CUTOFF_HIGH  = $d416
SID_FILTER_CONTROL      = $d417
SID_VOLUME              = $d418

; 音乐速度使用 9 位（模 512）相位累加器。基础增量 44/512 与旧版
; 22/256 完全相同；55/66/77/88 则精确对应 1.25/1.5/1.75/2 倍。
; EN: A modulo-512 phase makes all five scroll-matched tempo ratios exact.
MUSIC_TEMPO_LEVEL_0     = 44
MUSIC_TEMPO_LEVEL_1     = 55
MUSIC_TEMPO_LEVEL_2     = 66
MUSIC_TEMPO_LEVEL_3     = 77
MUSIC_TEMPO_LEVEL_4     = 88
MUSIC_TEMPO_WRAP_HIGH   = 2
MUSIC_PATTERN_MASK      = $7f
MUSIC_LEAD_WAVE         = $40
MUSIC_LEAD_GATE         = $41
MUSIC_BASS_WAVE         = $10
MUSIC_BASS_GATE         = $11
MUSIC_ARP_WAVE          = $20
MUSIC_ARP_GATE          = $21
MUSIC_NOISE_WAVE        = $80
MUSIC_NOISE_GATE        = $81

; 音乐 IRQ 必须早于主循环的帧同步线。若两者都设为 250，IRQ 会在
; wait_for_frame 读取 $D012 前抢占 CPU；返回时光栅已经越过 250，主循环
; 便会偶发多等待一整帧，表现为明显顿挫。
MUSIC_IRQ_RASTER        = 240
FRAME_SYNC_RASTER       = 250

; VIC 内存指针和固定的 64 字节 sprite 槽。硬件 sprite 位图实际使用前
; 63 字节（24x21 位），第 64 字节只是对齐填充。屏幕矩阵末尾 $3f8..$3ff
; 保存八个 sprite 块号。滚动期间 D018 会切换屏幕，因此 A/B 两张指针表
; 必须始终同步。
SCREEN_A_D018           = $1a
SCREEN_B_D018           = $3a
SPRITE_POINTER_A        = SCREEN_A + $03f8
SPRITE_POINTER_B        = SCREEN_B + $03f8
PLAYER_NORMAL_ADDRESS  = $3000
PLAYER_FALL_ADDRESS    = $3040
PLAYER_RIGHT_ADDRESS   = $3080
PLAYER_LEFT_ADDRESS    = $30c0
PLAYER_HURT_ADDRESS    = $3a00
EMBER_NORMAL_ADDRESS   = $3a40
EMBER_FALL_ADDRESS     = $3a80
EMBER_RIGHT_ADDRESS    = $3ac0
EMBER_LEFT_ADDRESS     = $3b00
EMBER_HURT_ADDRESS     = $3b40
WASSER_NORMAL_ADDRESS  = $3b80
WASSER_FALL_ADDRESS    = $3bc0
WASSER_RIGHT_ADDRESS   = $3c00
WASSER_LEFT_ADDRESS    = $3c40
WASSER_HURT_ADDRESS    = $3c80
PLAYER_SETS_END_ADDRESS = $3cc0
PLAYER_NORMAL_BLOCK    = PLAYER_NORMAL_ADDRESS / 64
PLAYER_FALL_BLOCK      = PLAYER_FALL_ADDRESS / 64
PLAYER_RIGHT_BLOCK     = PLAYER_RIGHT_ADDRESS / 64
PLAYER_LEFT_BLOCK      = PLAYER_LEFT_ADDRESS / 64
PLAYER_HURT_BLOCK      = PLAYER_HURT_ADDRESS / 64
EMBER_NORMAL_BLOCK     = EMBER_NORMAL_ADDRESS / 64
EMBER_FALL_BLOCK       = EMBER_FALL_ADDRESS / 64
EMBER_RIGHT_BLOCK      = EMBER_RIGHT_ADDRESS / 64
EMBER_LEFT_BLOCK       = EMBER_LEFT_ADDRESS / 64
EMBER_HURT_BLOCK       = EMBER_HURT_ADDRESS / 64
WASSER_NORMAL_BLOCK    = WASSER_NORMAL_ADDRESS / 64
WASSER_FALL_BLOCK      = WASSER_FALL_ADDRESS / 64
WASSER_RIGHT_BLOCK     = WASSER_RIGHT_ADDRESS / 64
WASSER_LEFT_BLOCK      = WASSER_LEFT_ADDRESS / 64
WASSER_HURT_BLOCK      = WASSER_HURT_ADDRESS / 64
PADDLE_POINTER_ADDRESS  = $3100
PADDLE_POINTER_BLOCK    = PADDLE_POINTER_ADDRESS / 64
PADDLE_POINTER_X_LOW    = 18
PADDLE_POINTER_Y        = 211

; 结束画面把受伤帧作为 sprite 0 居中放在 GAME OVER 与 PRESS FIRE 之间。
GAME_OVER_PLAYER_X      = 172
GAME_OVER_PLAYER_Y      = 124

; 标题画面同时显示三个角色。POTX 左/中/右三段直接映射到角色 0/1/2；
; 锁定后只保留中间的 sprite 0，并用 left/normal/right 朝向提示旋钮回中。
PLAYER_CHARACTER_COUNT        = 3
CHARACTER_ELIEN               = 0
CHARACTER_EMBER               = 1
CHARACTER_WASSER              = 2
CHARACTER_SELECT_MIDDLE_START = 85
CHARACTER_SELECT_RIGHT_START  = 171
CHARACTER_SELECT_LEFT_X       = 96
CHARACTER_SELECT_CENTER_X     = 172
CHARACTER_SELECT_RIGHT_X      = 248
CHARACTER_SELECT_Y            = 124
CHARACTER_SELECT_HURT_FRAMES  = 12

; 游戏字形/屏幕码和独立菜单文字屏幕码；无碎片/字母复用别名。
; EN: Game glyph IDs/screen codes and separate modal text codes.
.include "charset_ids.inc"

; 开始/结束画面的文字位置，使用相对于任一屏幕缓冲区的偏移量表示。
TITLE_OFFSET            = 8 * SCREEN_COLUMNS + 16
PROMPT_OFFSET           = 14 * SCREEN_COLUMNS + 15
CENTERING_PROMPT_ROW    = 14
CENTERING_PROMPT_COLUMN = 2
CENTERING_PROMPT_LENGTH = 35
CENTERING_PROMPT_OFFSET = CENTERING_PROMPT_ROW * SCREEN_COLUMNS + CENTERING_PROMPT_COLUMN
GAME_OVER_OFFSET        = 8 * SCREEN_COLUMNS + 15

COLOR_BLACK             = 0
COLOR_WHITE             = 1
COLOR_RED               = 2
COLOR_CYAN              = 3
COLOR_PURPLE            = 4
COLOR_GREEN             = 5
COLOR_BLUE              = 6
COLOR_YELLOW            = 7
COLOR_GRAY              = 12
COLOR_LIGHT_BLUE        = 14

; 共享白/灰色，局部色用于红砖、白绿尖刺和虚化平台的红色砖块。
; EN: Shared white/gray; local red or green keeps materials independent.
BRICK_CELL_COLOR        = MULTICOLOR_CELL | COLOR_RED
SPIKE_CELL_COLOR        = MULTICOLOR_CELL | COLOR_GREEN
FADE_CELL_COLOR         = MULTICOLOR_CELL | COLOR_RED
CONVEYOR_CELL_COLOR     = MULTICOLOR_CELL | COLOR_BLUE
UI_FRAME_COLOR          = BRICK_CELL_COLOR

; 世界滚动使用 8 位相位累加器。每帧把 rate 加入 accumulator，只有无符号
; 加法产生进位时才上移一像素。128/256=0.5 像素/帧，保持原版实际速度；
; 160/192/224 分别是基础速度的 1.25/1.5/1.75 倍。最高档需要精确达到
; 1 像素/帧（2 倍），无法用 8 位数表示 256，因此用 rate=0 作为特殊标记。
; EN: Rates 128..224 use an 8-bit phase accumulator; zero means every frame.
SCROLL_RATE_LEVEL_0     = 128
SCROLL_RATE_LEVEL_1     = 160
SCROLL_RATE_LEVEL_2     = 192
SCROLL_RATE_LEVEL_3     = 224
SCROLL_RATE_LEVEL_4     = 0
SCROLL_LEVEL_MAX        = 4
; 分数每 20 个首次落地平台提升一档：20、40、60、80。门槛表保存十位数字。
SCORE_LEVEL_THRESHOLD_COUNT = SCROLL_LEVEL_MAX
SCORES_PER_HP           = 3
PLATFORM_SCORE_UNCLAIMED = $01
PLATFORM_SCORE_CLAIMED   = $81
PLATFORM_SCORE_CLAIMED_MASK = $80
PLATFORM_GAP_ROWS       = 4
SEED_PLATFORM_COUNT     = 5
PLATFORM_MIN_WIDTH      = 7
PLATFORM_WIDTH_COUNT    = 6

; platform_type 是生成/绘制阶段使用的连续数组索引。碰撞返回值另设一组
; 常量，是因为已激活消失平台需要一个额外状态。
PLATFORM_TYPE_NORMAL         = 0
PLATFORM_TYPE_SPIKES         = 1
PLATFORM_TYPE_FADE           = 2
PLATFORM_TYPE_SPRING         = 3
PLATFORM_TYPE_CONVEYOR_LEFT  = 4
PLATFORM_TYPE_CONVEYOR_RIGHT = 5
PLATFORM_TYPE_COUNT          = 6
PLATFORM_REPEAT_LIMIT        = 2
PLATFORM_GENERATION_LEVEL_MAX = 4

COLLISION_NONE               = 0
COLLISION_NORMAL             = 1
COLLISION_SPIKES             = 2
COLLISION_FADE_INACTIVE      = 3
COLLISION_FADE_ACTIVE        = 4
COLLISION_SPRING             = 5
COLLISION_CONVEYOR_LEFT      = 6
COLLISION_CONVEYOR_RIGHT     = 7

; 传送平台每三帧推动人物两像素，平均速度与 paddle 普通档相同。独立计数器
; 让传送速度与人物输入真正叠加，而不是共用节流相位。
CONVEYOR_MOVE_DELAY     = 2
CONVEYOR_PUSH_STEP      = 2
; 表面每三帧循环移动一个多色像素（两显示像素），不依赖人物是否站在上面。
CONVEYOR_ANIMATION_FRAMES = 3
POTX_DEADZONE_LOW       = 108
POTX_DEADZONE_HIGH      = 148
POTX_FAR_LEFT_END       = 64
POTX_FAR_RIGHT_START    = 192
PLAYER_MOVE_DELAY       = 2
PLAYER_FAST_MOVE_DELAY  = 1
PLAYER_MOVE_STEP        = 2
; 玩家被限制在砖纹字符框内部：列 2 从 sprite X=40 开始，右侧分隔条
; 位于 X=256，因此 24 像素宽的玩家最右只能到 X=232。
PLAYER_MIN_X            = 40
PLAYER_MAX_X_LOW        = $e8
PLAYER_MAX_X_HIGH       = 0
PLAYER_HEIGHT           = 21
; 固定字符矩阵在 sprite 坐标系的首行 Y=50；43 只是平台软件相位的基值，
; 加 fine_scroll=7 才得到完整字符位置。固定危险区不能直接使用 43。
; EN: Fixed screen origin versus phase-biased moving-platform origin.
SCREEN_CHARACTER_TOP_Y = 50
SCREEN_ROW0_BASE_Y      = SCREEN_CHARACTER_TOP_Y - 7
GRAVITY_MAX             = 8
SPIKE_DAMAGE_BOUNCE_VELOCITY = 16
CEILING_DAMAGE_DROP_PIXELS = 8
PLAYER_HURT_FRAMES      = 16

; 倒尖刺占第 2 行，尖端最后一个像素在 Y=73。sprite 顶端接触该行尖端
; 时扣 1 HP 并下移脱离；HP 已为 0 才死亡。底部接触 Y=234 仍直接死亡（Y=214）。
; EN: Ceiling contact spends HP and drops below the last tip pixel; zero HP is fatal.
PLAYER_CEILING_CONTACT_Y = SCREEN_CHARACTER_TOP_Y + (CEILING_SPIKE_ROW + 1) * 8 - 1
PLAYER_BOTTOM_GAME_OVER_Y = SCREEN_CHARACTER_TOP_Y + UI_FRAME_BOTTOM_ROW * 8 - (PLAYER_HEIGHT-1)

; 踩下后部分脱落，25 帧后只剩砖块，50 帧后消失（PAL 约一秒）。
FADE_PLATFORM_LIFETIME  = 50
FADE_PLATFORM_BROKEN_TIME = FADE_PLATFORM_LIFETIME / 2
FADE_EFFECT_ACTIVATE    = 0
FADE_EFFECT_ADVANCE     = 1
FADE_EFFECT_REMOVE      = 2

; 弹簧落地后先压缩数帧，再以 1/8 像素为单位给人物一个向上的初速度。
; 初速度 24、每帧衰减 1，理论上可上升约 37 像素。
SPRING_COMPRESSION_FRAMES = 8
SPRING_LAUNCH_VELOCITY  = 24
SPRING_EFFECT_COMPRESS  = 0
SPRING_EFFECT_RESTORE   = 1

; 共用的零页临时指针。6502 的 (pointer),Y 间接索引寻址要求 pointer 位于
; 零页：低字节放在第一个地址，高字节放在下一个地址。音乐 IRQ 不使用这些
; 指针，所以主循环中的屏幕复制不会被中断破坏。
ROW_PTR                 = $fb
DEST_PTR                = $fd
NEXT_ROW_PTR            = $f9
COLOR_PTR               = $f7
