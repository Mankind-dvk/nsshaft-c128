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
;
; 本工程不定义跨例程自动保存寄存器的 ABI。除非例程注释另有说明，JSR
; 调用者应假定 A、X、Y 和状态标志都会被修改；需要长期保存的数据一律放在
; state.inc 的命名字节中。@name 是 ca65 的 cheap-local label，只在它前面
; 最近的非 @ 标签范围内有效，因此不同例程可以重复使用 @done、@loop。
;
; English summary:
;   This is the only assembler entry point. It owns machine initialization,
;   round initialization, frame scheduling, and modal transitions. All included
;   modules form one ca65 translation unit. There is no register-preserving ABI:
;   unless a routine says otherwise, callers must treat A, X, Y, and flags as
;   clobbered. Persistent values belong in state.inc.
;   Frame order is input -> HUD -> horizontal physics -> vertical physics ->
;   platform effects -> animation -> game-over check -> scheduled world scroll.

.segment "LOADADDR"
    ; PRG 文件开头的两个字节不是 8502 指令，而是 LOAD 使用的装载地址。
    .word $1c01

.segment "BASIC"
    ; BASIC 7 的链表行：下一行地址、行号 10、SYS token、十进制地址 7424、
    ; 行尾 0，最后再用空指针结束程序。RUN 最终跳到机器码 $1d00。
    .word basic_end
    .word 10
    .byte $9e
    .byte "7424"
    .byte 0
basic_end:
    .word 0

.segment "CODE"

; EN: One-time native-C128/MMU/VIC/CIA initialization; enters the title flow.
start:
    ; 输入：由 BASIC 的 SYS 7424 进入；不依赖 A/X/Y 初值。
    ; 输出：完成 C128/VIC-IIe/CIA 基础配置，然后进入标题或自动测试流程。
    ; SEI 先阻止异步 IRQ 在内存映射尚未稳定时执行；音乐初始化完成后再 CLI。
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

    ; 初始化期间先关闭全部 sprite，避免 BASIC 遗留寄存器显示随机图块。
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
start_screen:
    jsr show_start_screen
    jsr run_character_selection
    jmp new_game

; EN: Reset every subsystem, generate a new world, and start music from bar one.
new_game:
    ; 新游戏必须显式重置每个子系统。不能依赖“上一次 game over 后碰巧留下
    ; 什么值”，否则第二局会继承消失平台计时器、滚动相位或音乐步骤。
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
    jsr initialize_spring_platforms
    jsr initialize_conveyor_platforms
    jsr seed_random
    jsr initialize_score
    jsr initialize_health
    jsr initialize_platform_generation
    jsr seed_platforms
    jsr initialize_ui
    jsr initialize_hud
    jsr read_paddle_x
    jsr update_potx_display
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

; EN: Deterministic PAL-frame scheduler; game_over_flag is handled after updates.
main_loop:
    ; 这一段是游戏的固定逐帧调度表。前八个 JSR 每帧执行一次；平台世界
    ; 的一像素上移由定点相位累加器控制；首次落到新平台时才增加分数。
    jsr wait_for_frame
    jsr read_paddle_x
    jsr update_dashboard_pointer
    jsr update_potx_display
    jsr update_player_horizontal
    jsr update_player_vertical
    jsr update_fade_platform
    jsr update_spring_platform
    jsr update_player_sprite_frame
    lda game_over_flag
    beq @continue_game
    jmp game_over_screen
@continue_game:

    jsr scroll_step_ready
    bcc main_loop
    jsr scroll_one_pixel_up
    jmp main_loop

; EN: Stop gameplay/music, show the modal screen, then rebuild a fresh round.
game_over_screen:
    ; 结束画面确认后回到选人界面；下一局仍由 new_game 完整重建随机世界。
    jsr stop_music
    jsr show_game_over_screen
    jsr wait_for_action_button
    jmp start_screen

; EN: Wait until raster 250 is left and reached again, preventing double updates.
wait_for_frame:
    ; 两段等待很重要：如果进入例程时光栅已经等于 250，先等待它离开，
    ; 再等待下一次到达 250。否则主循环可能在同一帧内执行两次。
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
.include "score.inc"
.include "health.inc"
.include "character_select.inc"
.include "player.inc"
.include "fade_platforms.inc"
.include "spring_platforms.inc"
.include "conveyor_platforms.inc"
.include "music.inc"
.include "assets.inc"
.include "state.inc"
