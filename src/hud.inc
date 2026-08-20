; ---------------------------------------------------------------------------
; 固定状态栏 HUD
; ---------------------------------------------------------------------------
; POTX 和白色半圆表盘都由固定字符组成，只有红色表针使用 sprite 4。
; 这样 sprite 1..3、5..7 全部保持空闲；自然 D011 相位也不再需要底端
; 补齐 sprite。
;
; 公共入口：
;   initialize_hud、update_potx_display、initialize_dashboard、
;   update_dashboard_pointer。

.segment "CODE"

; POTX 数值显示 -------------------------------------------------------------

initialize_hud:
    ; 状态栏内部恰好有八列，直接写入 "POTX:000"。两个屏幕缓冲区都
    ; 必须更新，否则平台过渡时切换 D018 会显示旧数字。
    ldx #0
@label:
    lda potx_label, x
    sta SCREEN_A+POTX_DISPLAY_OFFSET, x
    sta SCREEN_B+POTX_DISPLAY_OFFSET, x
    inx
    cpx #5
    bne @label

    ldx #0
    lda #COLOR_WHITE
@color:
    sta COLOR_RAM+POTX_DISPLAY_OFFSET, x
    inx
    cpx #8
    bne @color
    rts

update_potx_display:
    lda potx_raw
    sta decimal_work
    ldx #0

@hundreds:
    lda decimal_work
    cmp #100
    bcc @hundreds_done
    sec
    sbc #100
    sta decimal_work
    inx
    bne @hundreds

@hundreds_done:
    txa
    sta potx_hundreds

    ldx #0
@tens:
    lda decimal_work
    cmp #10
    bcc @tens_done
    sec
    sbc #10
    sta decimal_work
    inx
    bne @tens

@tens_done:
    txa
    sta potx_tens

    lda decimal_work
    sta potx_ones

    ; 字符码 4..13 是 0..9。UI 不使用平台碎片更新区域，因此这些字符
    ; 始终固定在右上角。
    lda potx_hundreds
    clc
    adc #CHAR_DIGIT_0
    sta SCREEN_A+POTX_DISPLAY_OFFSET+5
    sta SCREEN_B+POTX_DISPLAY_OFFSET+5

    lda potx_tens
    clc
    adc #CHAR_DIGIT_0
    sta SCREEN_A+POTX_DISPLAY_OFFSET+6
    sta SCREEN_B+POTX_DISPLAY_OFFSET+6

    lda potx_ones
    clc
    adc #CHAR_DIGIT_0
    sta SCREEN_A+POTX_DISPLAY_OFFSET+7
    sta SCREEN_B+POTX_DISPLAY_OFFSET+7
    rts

; 固定 paddle 仪表盘 --------------------------------------------------------

initialize_dashboard:
    lda #PADDLE_POINTER_BLOCK
    sta SPRITE_POINTER_A+4
    sta SPRITE_POINTER_B+4

    ; 只有红色表针使用 sprite；白色半圆表盘由状态栏中的固定自定义字符组成。
    lda #PADDLE_POINTER_X_LOW
    sta VIC_SPRITE_4_X
    lda #PADDLE_POINTER_Y
    sta VIC_SPRITE_4_Y

    lda #COLOR_RED
    sta VIC_SPRITE_4_COLOR

    lda VIC_SPRITE_X_HIGH
    ; 保留玩家 sprite 0 的第九位，清除所有未使用 sprite 的高位。
    and #%00000001
    ora #%00010000
    sta VIC_SPRITE_X_HIGH

    lda #%00010000
    sta VIC_SPRITE_X_EXPAND

    lda #$ff
    sta dashboard_pointer_state
    jsr update_dashboard_pointer

    lda VIC_SPRITE_ENABLE
    ora #%00010000
    sta VIC_SPRITE_ENABLE
    rts

; 使用与玩家控制完全相同的死区选择向左、居中或向右的表针。仅切换指针
; 位图，表盘本身始终不动。
update_dashboard_pointer:
    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcs @right
    lda #1
    bne @select
@left:
    lda #0
    beq @select
@right:
    lda #2
@select:
    cmp dashboard_pointer_state
    beq @done
    sta dashboard_pointer_state
    tax
    lda pointer_shape_low, x
    sta ROW_PTR
    lda pointer_shape_high, x
    sta ROW_PTR+1

    ldy #62
@copy:
    lda (ROW_PTR), y
    sta PADDLE_POINTER_ADDRESS, y
    dey
    bpl @copy
@done:
    rts
