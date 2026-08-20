.setcpu "6502"

.include "constants.inc"

; ---------------------------------------------------------------------------
; NS-SHAFT C128 程序入口与逐帧调度
; ---------------------------------------------------------------------------
;
; main.s 有意只保留机器初始化和高层游戏生命周期。各子系统分别放在
; video、platforms、input、HUD、player、assets 和 state 模块中。
;
; 每帧的执行顺序不可随意调整：
;   1. 与 VIC-IIe 帧边界同步；
;   2. 读取 POTX 并更新固定 UI；
;   3. 更新玩家的水平与垂直运动；
;   4. 按设定节奏推进滚动游戏区域。
;
; 所有模块最终组成一个 ca65 翻译单元。这是常见的 6502 工程组织方式：
; 既保留低成本局部标签和精确的固定地址布局，又让各子系统源码保持精简、
; 易于审查。

.segment "LOADADDR"
    .word $1c01

.segment "BASIC"
    .word basic_end
    .word 10
    .byte $9e
    .byte "7424"
    .byte 0
basic_end:
    .word 0

.segment "CODE"

start:
    sei

    ; C128 原生 VIC-IIe 版本：可见显示期间让 8502 保持在 1 MHz。
    ; BASIC 可能将 CPU 留在其他 RAM bank，因此在改写 VIC 可见的屏幕、
    ; 字符集和 sprite 之前，必须明确映射 RAM bank 0。
    lda #MMU_CONFIG_BANK0_IO
    sta MMU_CONFIG
    lda VIC_CPU_SPEED
    and #%11111110
    sta VIC_CPU_SPEED
    lda MMU_RAM_CONFIG
    and #%10111111
    sta MMU_RAM_CONFIG

    ; 选择 RAM bank 0 中的第一个 16 KB VIC bank（$0000-$3fff）。
    lda CIA2_DDR_A
    ora #%00000011
    sta CIA2_DDR_A
    lda CIA2_PORT_A
    ora #%00000011
    sta CIA2_PORT_A

    lda #0
    sta VIC_SPRITE_ENABLE

    ; 高分辨率字符模式。D011 启用扩展背景色模式，使每个屏幕码都能选择
    ; 黑色、白色或红色，而无需逐帧移动 Color RAM。低三位固定为自然相位
    ; 3，让 25 行字符矩阵完整覆盖可视区。
    lda #$08
    sta VIC_CONTROL_2
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    lda #SCREEN_A_D018
    sta VIC_MEMORY_POINTERS

    lda #COLOR_BLACK
    sta VIC_BACKGROUND_COLOR
    lda #COLOR_WHITE
    sta VIC_MULTICOLOR_1
    lda #COLOR_RED
    sta VIC_MULTICOLOR_2
    lda #COLOR_GRAY
    sta VIC_BACKGROUND_COLOR_3
    ; 参考旧版 UI：硬件边框保持黑色，青色框完全由字符绘制。
    lda #COLOR_BLACK
    sta VIC_BORDER_COLOR

    jsr initialize_paddles
    jsr stop_music
.ifdef AUTO_START_TEST
    ; 画面/物理自动化测试只绕过标题按钮，仍执行正常游戏主循环。
    jmp new_game
.endif
.ifdef MUSIC_REGRESSION_TEST
    ; 自动化回归版本绕过标题按钮，只用于 VICE 的限时启动和 SID 写入检查。
    jmp new_game
.endif
    jsr show_start_screen
    jsr wait_for_action_button
    jmp new_game

new_game:
    lda #0
    sta VIC_SPRITE_ENABLE
    sta active_screen
    sta game_over_flag
    lda #7
    sta fine_scroll
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    lda #SCREEN_A_D018
    sta VIC_MEMORY_POINTERS

    jsr clear_screens_and_colors
    jsr initialize_fade_platforms
    jsr seed_random
    jsr seed_platforms
    jsr initialize_ui
    jsr initialize_hud
    jsr read_paddle_x
    jsr update_potx_display

    lda #SCROLL_DELAY
    sta speed_counter
    lda #2
    sta rows_until_platform

    jsr initialize_player
    jsr initialize_dashboard
    jsr initialize_music

    ; 屏幕 B 初始为当前可见游戏画面与 UI 的完全一致的隐藏副本。
    jsr prepare_screen_b_from_a

.ifdef MUSIC_REGRESSION_TEST
    ; 测试版本冻结游戏逻辑，让 VICE 能完整记录 16 小节和循环衔接。
music_regression_loop:
    jsr wait_for_frame
    jmp music_regression_loop
.endif

main_loop:
    jsr wait_for_frame
    jsr read_paddle_x
    jsr update_dashboard_pointer
    jsr update_potx_display
    jsr update_player_horizontal
    jsr update_player_vertical
    jsr update_fade_platform
    jsr update_player_sprite_frame
    lda game_over_flag
    beq @continue_game
    jmp game_over_screen
@continue_game:

    dec speed_counter
    bne main_loop
    lda #SCROLL_DELAY
    sta speed_counter

    jsr scroll_one_pixel_up
    jmp main_loop

game_over_screen:
    jsr stop_music
    jsr show_game_over_screen
    jsr wait_for_action_button
    jmp new_game

wait_for_frame:
@wait_until_away:
    lda VIC_RASTER
    cmp #FRAME_SYNC_RASTER
    beq @wait_until_away
@wait_until_line:
    lda VIC_RASTER
    cmp #FRAME_SYNC_RASTER
    bne @wait_until_line
    rts

; 子系统在文件中的排列顺序不会改变运行时调用顺序。ca65 会解析上方调度
; 代码产生的所有前向引用。
.include "video.inc"
.include "platforms.inc"
.include "input.inc"
.include "hud.inc"
.include "player.inc"
.include "fade_platforms.inc"
.include "music.inc"
.include "assets.inc"
.include "state.inc"
