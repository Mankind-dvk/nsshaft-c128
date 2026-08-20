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
;
; HUD 同时写 SCREEN_A 和 SCREEN_B，因为 $d018 随滚动翻页。Color RAM 只有
; 一份，不需要双写。表针的五个方向模板位于 assets.inc，每次换档才复制，
; 档位不变时不会重复搬运 63 字节。
;
; English summary:
;   Renders POTX:xxx into both screen buffers and maintains the fixed character
;   dial. Only its red hand uses hardware sprite 4. Five pointer templates are
;   copied into one writable sprite block only when the paddle range changes.

.segment "CODE"

; POTX 数值显示 -------------------------------------------------------------

; EN: Write the fixed POTX:000 label to both screen matrices.
initialize_hud:
    ; 输出：两张屏幕右上角都有 POTX:000，八个字符的颜色统一为白色。
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

; EN: Convert potx_raw to three decimal digits and update both buffers.
update_potx_display:
    ; 输入：potx_raw（0..255）。输出：右上角三个十进制数字。
    ; 6502 没有除法指令，因此先循环减 100 得百位，再循环减 10 得十位；
    ; 剩余值就是个位。对最多 255 的输入，这比通用除法例程更小也足够快。
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

; EN: Initialize sprite 4 and force the first five-position hand update.
initialize_dashboard:
    ; 输出：sprite 4 指向可写的 PADDLE_POINTER_ADDRESS，启用 X 两倍宽并设置
    ; 固定位置。$d010 的 bit 4 是 sprite 4 的第九个 X 位；bit 0 必须保留给玩家。
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

; 中央继续使用玩家控制的 108..148 死区；两侧再各分出一个最大斜角，得到
; 最左、左、中、右、最右五档。仅切换指针位图，表盘本身始终不动。
; EN: Map POTX to one of five templates; copy only when the range changes.
update_dashboard_pointer:
    ; 输入：potx_raw。输出：dashboard_pointer_state=0..4，并在档位变化时把
    ; 对应的 63 字节单色位图复制到 sprite 4 的可写块。
    ; 这里用连续 CMP 从小到大分区，边界值的归属可直接从 BCC 条件看出。
    lda potx_raw
    cmp #POTX_FAR_LEFT_END
    bcc @far_left
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcc @center
    cmp #POTX_FAR_RIGHT_START
    bcc @right
    lda #4
    bne @select
@far_left:
    lda #0
    beq @select
@left:
    lda #1
    bne @select
@center:
    lda #2
    bne @select
@right:
    lda #3
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
