; ---------------------------------------------------------------------------
; VIC-IIe 屏幕系统
; ---------------------------------------------------------------------------
; 本模块负责平台逐像素平滑过渡、双缓冲屏幕复制、开始/结束画面、固定列
; 分隔条，以及字符表盘的放置。硬件 D011 固定在 25 行模式的自然相位 3；
; 平滑运动改由实时改写四个过渡字形实现。
;
; 公共入口：
;   scroll_one_pixel_up、prepare_screen_b_from_a、show_start_screen、
;   show_game_over_screen、wait_for_action_button、clear_screens_and_colors、
;   initialize_ui。

.segment "CODE"

scroll_one_pixel_up:
    ; 玩家站在平台上时，会随滚动平台一起向上移动。
    lda player_on_platform
    beq @scroll_playfield
    dec player_y
    lda player_y
    sta VIC_SPRITE_0_Y

@scroll_playfield:
    lda fine_scroll
    cmp #7
    bne @fragments_exist

    dec fine_scroll
    jsr update_fragment_glyphs
    jsr start_fragment_transition
    jmp flip_hidden_screen

@fragments_exist:
    lda fine_scroll
    beq @coarse_step
    dec fine_scroll
    jmp update_fragment_glyphs

@coarse_step:
    lda #7
    sta fine_scroll
    jsr complete_fragment_transition
    jsr advance_fade_platform_scroll_position
    jmp flip_hidden_screen

; VIC 垂直滚动寄存器始终保持自然相位 3，使 25 行字符完整覆盖可视区。
; 普通/未触发灰色平台共用一对矩形碎片；已触发平台使用另一对矩形碎片，
; 尖刺平台使用自己的三角形碎片。每帧改写这些字形即可推进像素相位。
update_fragment_glyphs:
    lda fine_scroll
    clc
    adc #1
    sta fragment_split
    lda #7
    sec
    sbc fine_scroll
    sta fragment_count

    ldy #0
@normal_masks:
    cpy fragment_split
    bcc @normal_top
    lda #$00
    sta normal_upper_glyph, y
    sta fade_active_upper_glyph, y
    lda #$ff
    sta normal_lower_glyph, y
    sta fade_active_lower_glyph, y
    jmp @normal_next
@normal_top:
    lda #$ff
    sta normal_upper_glyph, y
    sta fade_active_upper_glyph, y
    lda #$00
    sta normal_lower_glyph, y
    sta fade_active_lower_glyph, y
@normal_next:
    iny
    cpy #8
    bne @normal_masks

    ldy #0
    lda #$ff
@clear_spikes:
    sta spike_upper_glyph, y
    sta spike_lower_glyph, y
    iny
    cpy #8
    bne @clear_spikes

    ldx #0
    ldy fragment_split
@spike_upper:
    lda spike_source_glyph, x
    sta spike_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @spike_upper

    ldx fragment_count
    ldy #0
@spike_lower:
    lda spike_source_glyph, x
    sta spike_lower_glyph, y
    inx
    iny
    cpx #8
    bne @spike_lower
    rts

prepare_screen_b_from_a:
copy_active_to_hidden:
    jsr set_screen_copy_pointers
    ldx #SCREEN_ROWS
@row:
    ldy #SCREEN_COLUMNS-1
@column:
    lda (ROW_PTR), y
    sta (DEST_PTR), y
    dey
    bpl @column
    jsr advance_copy_pointers
    dex
    bne @row
    rts

set_screen_copy_pointers:
    lda active_screen
    beq @a_to_b
    lda #<SCREEN_B
    sta ROW_PTR
    lda #>SCREEN_B
    sta ROW_PTR+1
    lda #<SCREEN_A
    sta DEST_PTR
    lda #>SCREEN_A
    sta DEST_PTR+1
    rts
@a_to_b:
    lda #<SCREEN_A
    sta ROW_PTR
    lda #>SCREEN_A
    sta ROW_PTR+1
    lda #<SCREEN_B
    sta DEST_PTR
    lda #>SCREEN_B
    sta DEST_PTR+1
    rts

advance_copy_pointers:
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta ROW_PTR
    bcc @source_ready
    inc ROW_PTR+1
@source_ready:
    clc
    lda DEST_PTR
    adc #SCREEN_COLUMNS
    sta DEST_PTR
    bcc @done
    inc DEST_PTR+1
@done:
    rts

advance_transition_pointers:
    jsr advance_copy_pointers
    clc
    lda NEXT_ROW_PTR
    adc #SCREEN_COLUMNS
    sta NEXT_ROW_PTR
    bcc @done
    inc NEXT_ROW_PTR+1
@done:
    rts

; 相位 1 将每个完整平台替换为下部碎片，并在上一字符行插入对应的上部碎片。
start_fragment_transition:
    jsr set_screen_copy_pointers
    ; 青色框占据第 1/23 行，平台只允许在第 2..22 行滚动。跳过前两行也
    ; 会让最上方平台自然消失在框线后面，而不是覆盖顶框。
    jsr advance_copy_pointers
    jsr advance_copy_pointers
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta NEXT_ROW_PTR
    lda ROW_PTR+1
    adc #0
    sta NEXT_ROW_PTR+1

    ldx #PLAYFIELD_ROW_COUNT
@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    beq @check_next_row
    cmp #CHAR_PLATFORM_FULL
    beq @normal_lower
    cmp #CHAR_SPIKE_TRIANGLE
    beq @spike_lower
    cmp #CHAR_FADE_FULL
    beq @fade_lower
    cmp #CHAR_FADE_ACTIVE_RED_FULL
    beq @fade_active_red_lower
    cmp #CHAR_FADE_ACTIVE_GRAY_FULL
    beq @fade_active_gray_lower
    ; 游戏区中若以后加入其他固定字符，原样复制到隐藏屏。
    jmp @store

@check_next_row:
    cpx #1
    beq @store
    lda (NEXT_ROW_PTR), y
    beq @blank
    cmp #CHAR_PLATFORM_FULL
    beq @normal_upper
    cmp #CHAR_SPIKE_TRIANGLE
    beq @spike_upper
    cmp #CHAR_FADE_FULL
    beq @fade_upper
    cmp #CHAR_FADE_ACTIVE_RED_FULL
    beq @fade_active_red_upper
    cmp #CHAR_FADE_ACTIVE_GRAY_FULL
    beq @fade_active_gray_upper
@blank:
    lda #CHAR_BLANK
    beq @store
@normal_lower:
    lda #CHAR_NORMAL_LOWER
    bne @store
@spike_lower:
    lda #CHAR_SPIKE_LOWER
    bne @store
@fade_lower:
    lda #CHAR_FADE_LOWER
    bne @store
@fade_active_red_lower:
    lda #CHAR_FADE_ACTIVE_RED_LOWER
    bne @store
@fade_active_gray_lower:
    lda #CHAR_FADE_ACTIVE_GRAY_LOWER
    bne @store
@normal_upper:
    lda #CHAR_NORMAL_UPPER
    bne @store
@spike_upper:
    lda #CHAR_SPIKE_UPPER
    bne @store
@fade_upper:
    lda #CHAR_FADE_UPPER
    bne @store
@fade_active_red_upper:
    lda #CHAR_FADE_ACTIVE_RED_UPPER
    bne @store
@fade_active_gray_upper:
    lda #CHAR_FADE_ACTIVE_GRAY_UPPER
@store:
    sta (DEST_PTR), y
@next_column:
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @column
    jsr advance_transition_pointers
    dex
    bne @row
    rts

; 相位 7 完成后，上部碎片在上移一行的位置变为完整字符，下部碎片消失；
; 此时可以从画面底部加入新平台。
complete_fragment_transition:
    jsr set_screen_copy_pointers
    jsr advance_copy_pointers
    jsr advance_copy_pointers
    ldx #PLAYFIELD_ROW_COUNT
@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    beq @store
    cmp #CHAR_NORMAL_UPPER
    beq @normal_full
    cmp #CHAR_SPIKE_UPPER
    beq @spike_full
    cmp #CHAR_FADE_UPPER
    beq @fade_full
    cmp #CHAR_FADE_ACTIVE_RED_UPPER
    beq @fade_active_red_full
    cmp #CHAR_FADE_ACTIVE_GRAY_UPPER
    beq @fade_active_gray_full
    cmp #CHAR_NORMAL_LOWER
    beq @blank
    cmp #CHAR_SPIKE_LOWER
    beq @blank
    cmp #CHAR_FADE_LOWER
    beq @blank
    cmp #CHAR_FADE_ACTIVE_RED_LOWER
    beq @blank
    cmp #CHAR_FADE_ACTIVE_GRAY_LOWER
    bne @store
@blank:
    lda #CHAR_BLANK
    beq @store
@normal_full:
    lda #CHAR_PLATFORM_FULL
    bne @store
@spike_full:
    lda #CHAR_SPIKE_TRIANGLE
    bne @store
@fade_full:
    lda #CHAR_FADE_FULL
    bne @store
@fade_active_red_full:
    lda #CHAR_FADE_ACTIVE_RED_FULL
    bne @store
@fade_active_gray_full:
    lda #CHAR_FADE_ACTIVE_GRAY_FULL
@store:
    sta (DEST_PTR), y
@next_column:
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @column
    jsr advance_copy_pointers
    dex
    bne @row

    lda active_screen
    beq @bottom_b
    lda #<(SCREEN_A + PLAYFIELD_BOTTOM_OFFSET)
    sta ROW_PTR
    lda #>(SCREEN_A + PLAYFIELD_BOTTOM_OFFSET)
    sta ROW_PTR+1
    jmp generate_bottom_row
@bottom_b:
    lda #<(SCREEN_B + PLAYFIELD_BOTTOM_OFFSET)
    sta ROW_PTR
    lda #>(SCREEN_B + PLAYFIELD_BOTTOM_OFFSET)
    sta ROW_PTR+1
    jmp generate_bottom_row

flip_hidden_screen:
    lda active_screen
    eor #1
    sta active_screen
    beq @show_screen_a

    lda #SCREEN_B_D018
    sta VIC_MEMORY_POINTERS
    rts

@show_screen_a:
    lda #SCREEN_A_D018
    sta VIC_MEMORY_POINTERS
    rts

generate_bottom_row:
    lda #CHAR_BLANK
    ldy #PLAYFIELD_LAST_COLUMN
@clear:
    sta (ROW_PTR), y
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @clear

    lda rows_until_platform
    beq @add_platform
    dec rows_until_platform
    rts

@add_platform:
    jsr randomize_platform
    jsr draw_platform

    lda #PLATFORM_GAP_ROWS
    sta rows_until_platform
    rts

; 不滚动的开始与游戏结束画面 ---------------------------------------------

show_start_screen:
    lda #0
    sta VIC_SPRITE_ENABLE
    lda #SCREEN_A_D018
    sta VIC_MEMORY_POINTERS
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    jsr clear_screens_and_colors

    ldx #0
@title:
    lda start_title, x
    sta SCREEN_A+TITLE_OFFSET, x
    sta SCREEN_B+TITLE_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+TITLE_OFFSET, x
    inx
    cpx #8
    bne @title
    jmp draw_action_prompt

show_game_over_screen:
    lda #0
    sta VIC_SPRITE_ENABLE
    lda #SCREEN_A_D018
    sta VIC_MEMORY_POINTERS
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    jsr clear_screens_and_colors

    ldx #0
@title:
    lda game_over_title, x
    sta SCREEN_A+GAME_OVER_OFFSET, x
    sta SCREEN_B+GAME_OVER_OFFSET, x
    lda #COLOR_RED
    sta COLOR_RAM+GAME_OVER_OFFSET, x
    inx
    cpx #9
    bne @title

draw_action_prompt:
    ldx #0
@prompt:
    lda action_prompt, x
    sta SCREEN_A+PROMPT_OFFSET, x
    sta SCREEN_B+PROMPT_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+PROMPT_OFFSET, x
    inx
    cpx #10
    bne @prompt
    rts

; 接受控制端口 1 上的 paddle 按钮 1/2 和摇杆 FIRE。按下前后都等待按钮
; 释放，可防止一次按键连续跳过两个画面。
wait_for_action_button:
@wait_release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @wait_release
@wait_press:
    jsr wait_for_frame
    jsr action_button_pressed
    bcc @wait_press
@wait_final_release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @wait_final_release
    rts

action_button_pressed:
    lda CIA1_PORT_B
    and #%00011100
    cmp #%00011100
    beq @not_pressed
    sec
    rts
@not_pressed:
    clc
    rts

clear_screens_and_colors:
    ldx #0
@loop:
    lda #CHAR_BLANK
    sta SCREEN_A+$000, x
    sta SCREEN_A+$100, x
    sta SCREEN_A+$200, x
    sta SCREEN_A+$300, x
    sta SCREEN_B+$000, x
    sta SCREEN_B+$100, x
    sta SCREEN_B+$200, x
    sta SCREEN_B+$300, x

    lda #COLOR_BLACK
    sta COLOR_RAM+$000, x
    sta COLOR_RAM+$100, x
    sta COLOR_RAM+$200, x
    sta COLOR_RAM+$300, x
    inx
    bne @loop
    rts

; 参考旧版 NS-Shaft 的固定字符框：硬件边框为黑色，屏幕第 1/23 行与
; 第 1/38 列组成青色外框，第 29 列分隔游戏区和八列状态栏。第 0/24 行
; 以及第 0/39 列保留黑色，形成参考图中的外侧留白。
initialize_ui:
    ; 先画上下两条水平框线。
    ldx #0
@horizontal:
    lda #CHAR_UI_FRAME
    sta SCREEN_A+UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    sta SCREEN_B+UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    sta SCREEN_A+UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    sta SCREEN_B+UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    lda #COLOR_CYAN
    sta COLOR_RAM+UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    sta COLOR_RAM+UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    inx
    cpx #UI_FRAME_WIDTH
    bne @horizontal

    ; 再逐行画左框、分隔条和右框。三个指针分别指向屏幕 A、屏幕 B 和
    ; Color RAM；先越过第 0 行，到达框线起始行 1。
    lda #<SCREEN_A
    sta ROW_PTR
    lda #>SCREEN_A
    sta ROW_PTR+1
    lda #<SCREEN_B
    sta DEST_PTR
    lda #>SCREEN_B
    sta DEST_PTR+1
    lda #<COLOR_RAM
    sta NEXT_ROW_PTR
    lda #>COLOR_RAM
    sta NEXT_ROW_PTR+1
    jsr advance_transition_pointers

    ldx #UI_FRAME_HEIGHT
@vertical:
    ldy #UI_FRAME_LEFT_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #COLOR_CYAN
    sta (NEXT_ROW_PTR), y

    ldy #UI_DIVIDER_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #COLOR_CYAN
    sta (NEXT_ROW_PTR), y

    ldy #UI_FRAME_RIGHT_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #COLOR_CYAN
    sta (NEXT_ROW_PTR), y

    jsr advance_transition_pointers
    dex
    bne @vertical

    ldx #0
@dial_row_1:
    lda dial_row_1, x
    sta SCREEN_A+DIAL_TOP_ROW*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    sta SCREEN_B+DIAL_TOP_ROW*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    lda #COLOR_WHITE
    sta COLOR_RAM+DIAL_TOP_ROW*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    inx
    cpx #8
    bne @dial_row_1

    ldx #0
@dial_row_2:
    lda dial_row_2, x
    sta SCREEN_A+(DIAL_TOP_ROW+1)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    sta SCREEN_B+(DIAL_TOP_ROW+1)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    lda #COLOR_WHITE
    sta COLOR_RAM+(DIAL_TOP_ROW+1)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    inx
    cpx #8
    bne @dial_row_2

    ldx #0
@dial_row_3:
    lda dial_row_3, x
    sta SCREEN_A+(DIAL_TOP_ROW+2)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    sta SCREEN_B+(DIAL_TOP_ROW+2)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    lda #COLOR_WHITE
    sta COLOR_RAM+(DIAL_TOP_ROW+2)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    inx
    cpx #8
    bne @dial_row_3
    rts

advance_row_pointer:
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta ROW_PTR
    bcc @done
    inc ROW_PTR+1
@done:
    rts
