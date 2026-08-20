; ---------------------------------------------------------------------------
; 可变游戏状态
; ---------------------------------------------------------------------------
; PRG 装载时这些字节均为零；开始新游戏时，再由各自所属的子系统显式
; 初始化。集中存放这些变量，可以在一个简短文件中直接看清 RAM 占用和
; 子系统之间的耦合关系。

.segment "CODE"

active_screen:
    .byte 0
game_over_flag:
    .byte 0
fine_scroll:
    .byte 7
fragment_split:
    .byte 7
fragment_count:
    .byte 1
speed_counter:
    .byte SCROLL_DELAY
rows_until_platform:
    .byte 2
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
music_step:
    .byte 0
music_tempo_accumulator:
    .byte 0
music_gate_timer:
    .byte 0
music_voice3_delay:
    .byte 0
player_x_low:
    .byte 0
player_x_high:
    .byte 0
player_move_counter:
    .byte 0
player_y:
    .byte 0
player_on_platform:
    .byte 0
player_on_fade_platform:
    .byte 0
player_fall_fraction:
    .byte 0
player_fall_velocity:
    .byte 0
spawn_row:
    .byte 0
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
random_state_low:
    .byte 0
random_state_high:
    .byte 0
