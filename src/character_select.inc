; ---------------------------------------------------------------------------
; 标题画面角色选择与 paddle 回中确认
; ---------------------------------------------------------------------------
; 选人阶段同时启用硬件 sprite 0..2，分别显示 Elien、Ember、Wasser 的 normal
; 帧。POTX 被平均分成左/中/右三段，当前段对应的角色保留本色，其他角色显示
; 灰色。第一次 Fire 只锁定角色，不会立即开始游戏。
;
; 锁定后关闭 sprite 1/2，把所选角色移到中央，并显示英文提示
; "PRESS FIRE WHEN CHARACTER FACES YOU"。此时 POTX<108 显示 left，
; POTX>148 显示 right，108..148 显示 normal；只有 normal 状态下再次按下并
; 释放 Fire 才返回 new_game。若朝向左/右时误按 Fire，角色至少显示 12 帧
; hurt 作为明确反馈，松开后再恢复当前方向。
;
; English summary:
;   The title screen shows all three normal frames. POTX selects left/middle/
;   right, Fire locks the highlighted character, and the chosen sprite then
;   faces toward the neutral zone under an explicit English prompt. A second
;   Fire is accepted only while POTX is inside 108..148. Off-center Fire shows
;   the selected hurt frame; every press is release-debounced.

.segment "SELECTCODE"

; EN: Run select -> lock -> center -> confirm, returning only when ready to play.
run_character_selection:
    jsr initialize_character_choices

    ; 等待进入界面时可能仍按住的按钮释放；等待期间选人预览仍持续响应 POTX。
@initial_release:
    jsr update_character_choices_frame
    jsr action_button_pressed
    bcs @initial_release

@wait_for_lock:
    jsr update_character_choices_frame
    jsr action_button_pressed
    bcc @wait_for_lock

    ; 第一次 Fire 锁定 selected_character。随后必须先松开，避免同一次长按
    ; 直接穿过第二阶段。
    jsr initialize_centering_preview
@wait_lock_release:
    jsr update_centering_preview_frame
    jsr action_button_pressed
    bcs @wait_lock_release

@wait_start_press:
    jsr update_centering_preview_frame
    jsr action_button_pressed
    bcc @wait_start_press
    jsr paddle_is_centered
    bcc @invalid_press

    ; 有效的第二次 Fire 也等待释放。若释放前旋钮离开死区，则取消本次确认，
    ; 回到等待下一次按键，而不是带着移动输入进入游戏。
@wait_final_release:
    jsr update_centering_preview_frame
    jsr action_button_pressed
    bcs @wait_final_release
    jsr paddle_is_centered
    bcc @wait_start_press
    rts

@invalid_press:
    ; 在 left/right 朝向时按 Fire 不开始游戏，而是显示 hurt 作为错误反馈。
    ; 固定计时保证即使很快松开，也能看清至少约 0.24 秒的受击帧。
    jsr render_centering_hurt
    lda #CHARACTER_SELECT_HURT_FRAMES
    sta character_select_hurt_timer
@show_hurt:
    jsr wait_for_frame
    jsr read_paddle_x
    dec character_select_hurt_timer
    bne @show_hurt

    ; 若按钮仍按住就继续保持 hurt；松开后才恢复与最新 POTX 对应的朝向。
@wait_invalid_release:
    jsr wait_for_frame
    jsr read_paddle_x
    jsr action_button_pressed
    bcs @wait_invalid_release
    jsr render_centering_direction
    jmp @wait_start_press

; EN: Configure three side-by-side normal sprites for the selection phase.
initialize_character_choices:
    lda #0
    sta VIC_SPRITE_MULTICOLOR
    sta VIC_SPRITE_X_EXPAND
    sta VIC_SPRITE_Y_EXPAND
    sta VIC_SPRITE_PRIORITY

    ; 三个 X 坐标均小于 256，因此清 sprite 0..2 的三个高位；其他位不改。
    lda VIC_SPRITE_X_HIGH
    and #%11111000
    sta VIC_SPRITE_X_HIGH

    lda #CHARACTER_SELECT_LEFT_X
    sta VIC_SPRITE_0_X
    lda #CHARACTER_SELECT_CENTER_X
    sta VIC_SPRITE_1_X
    lda #CHARACTER_SELECT_RIGHT_X
    sta VIC_SPRITE_2_X
    lda #CHARACTER_SELECT_Y
    sta VIC_SPRITE_0_Y
    sta VIC_SPRITE_1_Y
    sta VIC_SPRITE_2_Y

    ldx #0
@pointers:
    lda character_normal_blocks, x
    sta SPRITE_POINTER_A, x
    sta SPRITE_POINTER_B, x
    inx
    cpx #PLAYER_CHARACTER_COUNT
    bne @pointers

    lda #%00000111
    sta VIC_SPRITE_ENABLE
    jmp render_character_choices

; EN: Sample POTX, select one of three equal ranges, and update highlight colors.
update_character_choices_frame:
    jsr wait_for_frame
    jsr read_paddle_x

    lda potx_raw
    cmp #CHARACTER_SELECT_MIDDLE_START
    bcc @elien
    cmp #CHARACTER_SELECT_RIGHT_START
    bcc @ember
    lda #CHARACTER_WASSER
    bne @store
@elien:
    lda #CHARACTER_ELIEN
    beq @store
@ember:
    lda #CHARACTER_EMBER
@store:
    sta selected_character
    jmp render_character_choices

; EN: Gray all previews, then restore the selected character's native color.
render_character_choices:
    lda #COLOR_GRAY
    sta VIC_SPRITE_0_COLOR
    sta VIC_SPRITE_1_COLOR
    sta VIC_SPRITE_2_COLOR
    ldy selected_character
    lda character_color_table, y
    sta VIC_SPRITE_0_COLOR, y
    rts

; EN: Keep only sprite 0, center it, and begin direction-based neutral guidance.
initialize_centering_preview:
    jsr draw_centering_prompt
    lda #%00000001
    sta VIC_SPRITE_ENABLE
    lda #CHARACTER_SELECT_CENTER_X
    sta VIC_SPRITE_0_X
    lda #CHARACTER_SELECT_Y
    sta VIC_SPRITE_0_Y
    lda VIC_SPRITE_X_HIGH
    and #%11111110
    sta VIC_SPRITE_X_HIGH
    ldy selected_character
    lda character_color_table, y
    sta VIC_SPRITE_0_COLOR
    jmp render_centering_direction

; EN: Refresh the locked sprite once per PAL frame while waiting for neutral.
update_centering_preview_frame:
    jsr wait_for_frame
    jsr read_paddle_x
    jmp render_centering_direction

; EN: Point the locked character left/right toward neutral, or normal when ready.
render_centering_direction:
    ldy selected_character
    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcs @right
    lda character_normal_blocks, y
    bne @store
@left:
    lda character_left_blocks, y
    bne @store
@right:
    lda character_right_blocks, y
@store:
    sta SPRITE_POINTER_A
    sta SPRITE_POINTER_B
    rts

; EN: Replace the direction frame with the selected hurt frame after bad Fire.
render_centering_hurt:
    ldy selected_character
    lda character_hurt_blocks, y
    sta SPRITE_POINTER_A
    sta SPRITE_POINTER_B
    rts

; EN: Install four modal-only letters and draw the 35-character neutral prompt.
draw_centering_prompt:
    ; W/C/Y/U 暂用连续的 31..34 号过渡字形。游戏尚未开始，覆盖它们不会
    ; 影响平台；后续滚动转场会按正常流程重新生成相同槽位。
    ldx #CENTERING_PROMPT_GLYPH_BYTES-1
@install_glyphs:
    lda centering_prompt_glyphs, x
    sta normal_upper_glyph, x
    dex
    bpl @install_glyphs

    ; 先清除旧的十字符 PRESS FIRE，再写居中的 35 字符完整说明。
    ldx #SCREEN_COLUMNS-1
@clear_row:
    lda #CHAR_BLANK
    sta SCREEN_A+CENTERING_PROMPT_ROW*SCREEN_COLUMNS, x
    sta SCREEN_B+CENTERING_PROMPT_ROW*SCREEN_COLUMNS, x
    lda #COLOR_BLACK
    sta COLOR_RAM+CENTERING_PROMPT_ROW*SCREEN_COLUMNS, x
    dex
    bpl @clear_row

    ldx #0
@draw:
    lda centering_prompt, x
    sta SCREEN_A+CENTERING_PROMPT_OFFSET, x
    sta SCREEN_B+CENTERING_PROMPT_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+CENTERING_PROMPT_OFFSET, x
    inx
    cpx #CENTERING_PROMPT_LENGTH
    bne @draw
    rts

; EN: Return C=1 only for the same 108..148 neutral zone used by gameplay.
paddle_is_centered:
    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @no
    cmp #POTX_DEADZONE_HIGH+1
    bcs @no
    sec
    rts
@no:
    clc
    rts

; 每个表的索引都与 selected_character 相同。游戏中只有硬件 sprite 0 读取
; 这些块号；选人阶段则把 normal 表的三个值分配给硬件 sprite 0..2。
character_normal_blocks:
    .byte PLAYER_NORMAL_BLOCK, EMBER_NORMAL_BLOCK, WASSER_NORMAL_BLOCK
character_fall_blocks:
    .byte PLAYER_FALL_BLOCK, EMBER_FALL_BLOCK, WASSER_FALL_BLOCK
character_right_blocks:
    .byte PLAYER_RIGHT_BLOCK, EMBER_RIGHT_BLOCK, WASSER_RIGHT_BLOCK
character_left_blocks:
    .byte PLAYER_LEFT_BLOCK, EMBER_LEFT_BLOCK, WASSER_LEFT_BLOCK
character_hurt_blocks:
    .byte PLAYER_HURT_BLOCK, EMBER_HURT_BLOCK, WASSER_HURT_BLOCK
character_color_table:
    .byte COLOR_YELLOW, COLOR_RED, COLOR_CYAN

; "PRESS FIRE WHEN CHARACTER FACES YOU"，正好 35 个屏幕码。
centering_prompt:
    .byte CHAR_P, CHAR_R, CHAR_E, CHAR_S, CHAR_S, CHAR_BLANK
    .byte CHAR_F, CHAR_I, CHAR_R, CHAR_E, CHAR_BLANK
    .byte CHAR_W, CHAR_H, CHAR_E, CHAR_N, CHAR_BLANK
    .byte CHAR_C, CHAR_H, CHAR_A, CHAR_R, CHAR_A, CHAR_C, CHAR_T, CHAR_E, CHAR_R
    .byte CHAR_BLANK, CHAR_F, CHAR_A, CHAR_C, CHAR_E, CHAR_S, CHAR_BLANK
    .byte CHAR_Y, CHAR_O, CHAR_U
centering_prompt_end:
.assert centering_prompt_end-centering_prompt = CENTERING_PROMPT_LENGTH, error, "centering prompt length mismatch"

; 与项目其余 8x8 字体同风格的 W/C/Y/U，依次复制到字形 31..34。
centering_prompt_glyphs:
    .byte $63,$63,$63,$6b,$7f,$77,$63,$00 ; W
    .byte $3c,$66,$60,$60,$60,$66,$3c,$00 ; C
    .byte $66,$66,$66,$3c,$18,$18,$18,$00 ; Y
    .byte $66,$66,$66,$66,$66,$66,$3c,$00 ; U
centering_prompt_glyphs_end:
CENTERING_PROMPT_GLYPH_BYTES = centering_prompt_glyphs_end-centering_prompt_glyphs
.assert CENTERING_PROMPT_GLYPH_BYTES = 32, error, "centering prompt glyph size mismatch"
character_select_code_end:

.assert character_select_code_end <= $4000, error, "character select code exceeds VIC bank 0"
