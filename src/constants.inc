; C128 原生版本共用的编译期常量。
;
; 本文件只保存地址和不可变的布局参数。将这些定义集中在一处，能够避免
; 显示、输入、UI 和游戏逻辑模块各自维护不一致的内存映射。
;
; English summary:
;   Compile-time addresses and immutable layout/tuning values live here. `$`
;   introduces hexadecimal, `%` binary, `<`/`>` select address bytes, and these
;   definitions consume no runtime RAM. Keep hardware registers named and keep
;   playfield calculations inside the cyan frame and left of the HUD divider.
;
; 初学者语法提示：
;   $1234       十六进制常量；%10101010 是二进制常量。
;   NAME = ...  汇编期常量，不会在运行时占用 RAM。
;   <label      取地址低字节；>label 取地址高字节。
;   |           汇编期按位 OR，例如 $40 | 31 组合 ECM 颜色位和字形编号。
;   +、-、*     这里都由 ca65 在汇编期计算，不消耗 8502 运行时间。
;
; 屏幕矩阵与双栏布局。青色框内的第 29 列是实心 UI 分隔条，因此平台
; 生成、软件滚动和碰撞采样都必须避开框线与状态栏。
; EN: The playfield excludes the cyan frame and column-29 HUD divider.
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
PLAYFIELD_TOP_ROW       = UI_FRAME_TOP_ROW + 1
PLAYFIELD_BOTTOM_ROW    = UI_FRAME_BOTTOM_ROW - 1
PLAYFIELD_ROW_COUNT     = PLAYFIELD_BOTTOM_ROW - PLAYFIELD_TOP_ROW + 1
PLAYFIELD_FIRST_COLUMN  = UI_FRAME_LEFT_COLUMN + 1
UI_DIVIDER_COLUMN       = 29
STATUS_FIRST_COLUMN     = UI_DIVIDER_COLUMN + 1
PLAYFIELD_LAST_COLUMN   = UI_DIVIDER_COLUMN - 1
DIAL_FIRST_COLUMN       = STATUS_FIRST_COLUMN
DIAL_TOP_ROW            = 20
POTX_ROW                = PLAYFIELD_TOP_ROW
PLAYFIELD_BOTTOM_OFFSET = PLAYFIELD_BOTTOM_ROW * SCREEN_COLUMNS
POTX_DISPLAY_OFFSET     = POTX_ROW * SCREEN_COLUMNS + STATUS_FIRST_COLUMN

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
VIC_CONTROL_1_TEXT      = $5b

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
; ADSR 包络寄存器。音乐代码位于 $2a00，不占用字符集和 sprite 数据区；
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

MUSIC_TEMPO_INCREMENT   = 22
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
PLAYER_NORMAL_BLOCK    = PLAYER_NORMAL_ADDRESS / 64
PLAYER_FALL_BLOCK      = PLAYER_FALL_ADDRESS / 64
PLAYER_RIGHT_BLOCK     = PLAYER_RIGHT_ADDRESS / 64
PLAYER_LEFT_BLOCK      = PLAYER_LEFT_ADDRESS / 64
PADDLE_POINTER_ADDRESS  = $3100
PADDLE_POINTER_BLOCK    = PADDLE_POINTER_ADDRESS / 64
PADDLE_POINTER_X_LOW    = 18
PADDLE_POINTER_Y        = 211

; 自定义屏幕码。扩展背景色模式（ECM）把屏幕码高两位解释为背景色选择器，
; 低六位才是 0..63 的字形编号。因此本项目的自定义字符集固定为 64 个字形。
CHAR_BLANK              = 0
CHAR_PLATFORM_FULL      = $41
CHAR_SPIKE_TRIANGLE     = $82
CHAR_WALL               = 3
CHAR_DIGIT_0            = 4
CHAR_P                  = 14
CHAR_O                  = 15
CHAR_T                  = 16
CHAR_X                  = 17
CHAR_COLON              = 18
CHAR_N                  = 19
CHAR_S                  = 20
CHAR_DASH               = 21
CHAR_H                  = 22
CHAR_A                  = 23
CHAR_F                  = 24
CHAR_R                  = 25
CHAR_E                  = 26
CHAR_I                  = 27
CHAR_G                  = 28
CHAR_M                  = 29
CHAR_V                  = 30

; 游戏过程中，标题画面暂时不用的 N/S/-/H 四个字形槽会改写为传送平台。
; LEFT/RIGHT_FULL 同时承担“完整字符”和滚动中的下部碎片；粗滚动完成后再
; 恢复完整箭头。进入标题或结束画面前必须调用 restore_modal_font_glyphs。
CHAR_CONVEYOR_LEFT_FULL  = CHAR_N
CHAR_CONVEYOR_LEFT_UPPER = CHAR_S
CHAR_CONVEYOR_RIGHT_FULL = CHAR_DASH
CHAR_CONVEYOR_RIGHT_UPPER = CHAR_H
CHAR_NORMAL_UPPER       = $40 | 31
CHAR_NORMAL_LOWER       = $40 | 32
CHAR_SPIKE_UPPER        = $80 | 33
CHAR_SPIKE_LOWER        = $80 | 34
CHAR_UI_FRAME           = 35
CHAR_DIAL_ROW_1         = 36
CHAR_DIAL_ROW_2         = 44
CHAR_DIAL_ROW_3         = 52

; 表盘中原本完全空白的六个字形槽改作弹簧平台。表盘数据会在这些位置直接
; 使用 CHAR_BLANK，因此不会改变表盘外观。弹簧使用黑色背景和紫色 Color RAM
; 前景，不占用已经分配给白色、红色和灰色平台的 ECM 背景色槽。
CHAR_SPRING_FULL              = 36
CHAR_SPRING_UPPER             = 43
CHAR_SPRING_LOWER             = 46
CHAR_SPRING_COMPRESSED_FULL   = 47
CHAR_SPRING_COMPRESSED_UPPER  = 48
CHAR_SPRING_COMPRESSED_LOWER  = 49

; 消失平台使用 ECM 的第四背景色作为灰色。激活后改用独立的 60..62
; 号字形，这样红/灰闪烁不会误改尚未踩过的灰色平台。
CHAR_FADE_FULL          = $c0 | 1
CHAR_FADE_UPPER         = $c0 | 31
CHAR_FADE_LOWER         = $c0 | 32
CHAR_FADE_ACTIVE_RED_FULL  = $80 | 60
CHAR_FADE_ACTIVE_RED_UPPER = $80 | 61
CHAR_FADE_ACTIVE_RED_LOWER = $80 | 62
CHAR_FADE_ACTIVE_GRAY_FULL  = $c0 | 60
CHAR_FADE_ACTIVE_GRAY_UPPER = $c0 | 61
CHAR_FADE_ACTIVE_GRAY_LOWER = $c0 | 62
FADE_ACTIVE_GLYPH_FIRST = 60
FADE_ACTIVE_GLYPH_LAST  = 62
FADE_COLOR_RED_PREFIX   = $80
FADE_COLOR_GRAY_PREFIX  = $c0

; 开始/结束画面的文字位置，使用相对于任一屏幕缓冲区的偏移量表示。
TITLE_OFFSET            = 8 * SCREEN_COLUMNS + 16
PROMPT_OFFSET           = 14 * SCREEN_COLUMNS + 15
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

; 游戏参数与碰撞空间边界。所有 DELAY 值都表示“本次动作之后还要跳过的
; 帧数”：值 2 等于每三帧执行一次，值 1 等于每两帧执行一次。
SCROLL_DELAY            = 2
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
POTX_DEADZONE_LOW       = 108
POTX_DEADZONE_HIGH      = 148
POTX_FAR_LEFT_END       = 64
POTX_FAR_RIGHT_START    = 192
PLAYER_MOVE_DELAY       = 2
PLAYER_FAST_MOVE_DELAY  = 1
PLAYER_MOVE_STEP        = 2
; 玩家被限制在青色字符框内部：列 2 从 sprite X=40 开始，右侧分隔条
; 位于 X=256，因此 24 像素宽的玩家最右只能到 X=232。
PLAYER_MIN_X            = 40
PLAYER_MAX_X_LOW        = $e8
PLAYER_MAX_X_HIGH       = 0
PLAYER_HEIGHT           = 21
SCREEN_ROW0_BASE_Y      = 43
GRAVITY_MAX             = 8

; 顶框第 1 行的上沿位于 Y=51；底框第 23 行的下沿位于 Y=234。player_y
; 表示 21 像素高 sprite 的顶端，所以底端接触 Y=234 时 player_y=214。
; 两个阈值都包含接触位置：刚碰到框线外沿就触发 game over。
PLAYER_TOP_GAME_OVER_Y  = SCREEN_ROW0_BASE_Y + UI_FRAME_TOP_ROW * 8
PLAYER_BOTTOM_GAME_OVER_Y = SCREEN_ROW0_BASE_Y + (UI_FRAME_BOTTOM_ROW + 1) * 8 - PLAYER_HEIGHT

; PAL 下约两秒后消失，每 8 帧切换一次红/灰显示。
FADE_PLATFORM_LIFETIME  = 100
FADE_PLATFORM_FLASH_PERIOD = 8
FADE_EFFECT_ACTIVATE    = 0
FADE_EFFECT_RECOLOR     = 1
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
