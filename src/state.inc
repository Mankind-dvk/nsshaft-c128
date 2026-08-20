; ---------------------------------------------------------------------------
; 可变游戏状态
; ---------------------------------------------------------------------------
; PRG 装载时这些字节均为零；开始新游戏时，再由各自所属的子系统显式
; 初始化。集中存放这些变量，可以在一个简短文件中直接看清 RAM 占用和
; 子系统之间的耦合关系。
;
; `.byte value` 会在 PRG 中保留一个可写字节并给它贴上标签。6502 没有局部
; 栈变量或结构体语法，所以这里按子系统分组，相当于游戏的全局状态结构。
; 注释中的“布尔值”约定 0=false、非 0=true。
;
; English summary:
;   This file is the program's writable global-state structure. `.byte` reserves
;   one initialized RAM byte. State is grouped by subsystem; Boolean values use
;   zero for false and nonzero for true. Every subsystem must reset its bytes
;   from new_game instead of relying on values left by the previous round.

.segment "CODE"

; --- 帧调度与双缓冲 -------------------------------------------------------
; 0 表示 SCREEN_A 可见，1 表示 SCREEN_B 可见。
active_screen:
    .byte 0
; 任一物理分支置 1；主循环统一切换到 game-over 画面。
game_over_flag:
    .byte 0
; 软件滚动相位 7..0。它描述动态字形的切割位置，不直接写入 $d011。
fine_scroll:
    .byte 7
; update_fragment_glyphs 计算出的上下碎片分界和需要复制的像素行数。
fragment_split:
    .byte 7
fragment_count:
    .byte 1
; 每次减到 0 才调用 scroll_one_pixel_up。
speed_counter:
    .byte SCROLL_DELAY
; 底部还要生成多少个空逻辑行；0 表示本次应生成平台。
rows_until_platform:
    .byte 2

; --- 正在生成的平台 -------------------------------------------------------
; start/width 使用字符列单位；type 为 0普通、1尖刺、2消失、3弹簧、
; 4向左传送、5向右传送。
platform_start:
    .byte 0
platform_width:
    .byte 0
platform_type:
    .byte 0
platform_start_limit:
    .byte 0
platform_middle_count:
    .byte 0
selected_platform_char:
    .byte 0
selected_platform_color:
    .byte COLOR_BLACK

; --- Paddle、HUD 与音乐 ---------------------------------------------------
; SID 返回的原始 0..255 POTX 值；128 附近是中立位置。
potx_raw:
    .byte 128
decimal_work:
    .byte 0
potx_hundreds:
    .byte 1
potx_tens:
    .byte 2
potx_ones:
    .byte 8
dashboard_pointer_state:
    .byte $ff
; music_step 是 0..127 的八分音符索引；accumulator 溢出时才前进一步。
music_step:
    .byte 0
music_tempo_accumulator:
    .byte 0
music_gate_timer:
    .byte 0
music_voice3_delay:
    .byte 0

; --- 玩家位置与垂直物理 ---------------------------------------------------
; VIC sprite X 是 9 位：低八位写 $d000，第九位写 $d010 bit 0。
player_x_low:
    .byte 0
player_x_high:
    .byte 0
player_move_counter:
    .byte 0
; 与 paddle 的 player_move_counter 分开；值 2 表示本次推动后跳过两帧。
conveyor_move_counter:
    .byte 0
player_y:
    .byte 0
; 三个布尔值区分一般支撑、消失平台支撑和正在压缩的弹簧支撑。
player_on_platform:
    .byte 0
player_on_fade_platform:
    .byte 0
player_on_spring:
    .byte 0
; 垂直速度采用 1/8 像素定点数。fraction 保存尚未累积成整像素的余数；
; fall/rise_velocity 分开，使无符号 6502 不必频繁处理有符号数。
player_fall_fraction:
    .byte 0
player_fall_velocity:
    .byte 0
player_rise_velocity:
    .byte 0
spawn_row:
    .byte 0

; --- 碰撞查询临时值 -------------------------------------------------------
; find_platform_below_player 把 sprite 脚底的屏幕 Y 转换为字符行，并在左右
; 两个脚点采样。type 返回 0空/1普通/2尖刺/3未触发消失/4已触发/5弹簧、
; 6向左传送/7向右传送。
collision_probe_y:
    .byte 0
collision_screen_top:
    .byte 0
collision_relative_y:
    .byte 0
collision_row:
    .byte 0
collision_type:
    .byte 0
collision_x_low:
    .byte 0
collision_x_high:
    .byte 0

; --- 消失平台生命周期 -----------------------------------------------------
; 同时只跟踪一个已经触发的平台。row 是下部碎片/完整字符所在的逻辑锚点。
fade_platform_active:
    .byte 0
fade_platform_row:
    .byte 0
fade_platform_timer:
    .byte 0
fade_flash_counter:
    .byte 0
fade_color_prefix:
    .byte FADE_COLOR_RED_PREFIX
fade_effect_mode:
    .byte FADE_EFFECT_ACTIVATE
fade_rows_remaining:
    .byte 0
fade_char_work:
    .byte 0

; --- 弹簧生命周期 ---------------------------------------------------------
; 玩家站上时记录锚点并倒计时；离开或发射时在两个屏幕缓冲区恢复原字形。
spring_platform_row:
    .byte 0
spring_compression_timer:
    .byte 0
spring_effect_mode:
    .byte SPRING_EFFECT_COMPRESS
spring_rows_remaining:
    .byte 0
spring_char_work:
    .byte 0

; --- 16 位伪随机数状态 ----------------------------------------------------
random_state_low:
    .byte 0
random_state_high:
    .byte 0
