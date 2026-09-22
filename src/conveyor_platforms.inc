; ---------------------------------------------------------------------------
; 左/右传送平台的动态字形
; ---------------------------------------------------------------------------
; 平台边界不水平位移，白蓝表面每三帧向左/右循环移动两显示像素。
; 人物站在上面时由 player.inc 每三帧推动两像素；平台整体仍向上平滑滚动。
;
; Color RAM=14（bit 3=1），多色位对 01=白、11=蓝、00=黑色裁切区。
; 水平动画只改写独立 RAM 图案，再按当前垂直相位生成显示字形。
;
; 每个方向独立保存 FULL、UPPER、LOWER，避免翻屏前改坏可见完整平台。
; FULL 使用 5/6，UPPER 使用 18/19，LOWER 使用 66/67；保留水平动画相位。
; 菜单不借用任何平台槽；charset_loader.inc 在场景入口装载完整文字页。
;
; 公共入口：
;   initialize_conveyor_platforms、update_conveyor_fragment_glyphs、
;   restore_conveyor_full_glyphs、update_conveyor_animation。
;
; English summary:
;   White/blue belts rotate one multicolor pixel left/right every three frames,
;   independently of player support. FULL and LOWER have separate slots so
;   fragment rebuilding cannot reshape FULL cells on the still-visible screen.

.segment "GAMEPLAY"

; EN: Reset horizontal phase, install FULL glyphs and clear all fragment slots.
initialize_conveyor_platforms:
    ; 新游戏已经清空两张屏幕，可安全重置上局留下的传送平台碎片。
    ; 完整平台使用 LEFT/RIGHT_FULL；UPPER/LOWER 等第一次滚动时再生成。
    lda #CONVEYOR_ANIMATION_FRAMES
    sta conveyor_animation_counter
    ldy #7
@reset_patterns:
    lda conveyor_left_source_glyph, y
    sta conveyor_left_pattern, y
    lda conveyor_right_source_glyph, y
    sta conveyor_right_pattern, y
    dey
    bpl @reset_patterns
    jsr restore_conveyor_full_glyphs
    ldy #7
    lda #$00
@clear_upper:
    sta conveyor_left_upper_glyph, y
    sta conveyor_right_upper_glyph, y
    sta conveyor_left_lower_glyph, y
    sta conveyor_right_lower_glyph, y
    dey
    bpl @clear_upper
    rts

; 根据 fragment_split/fragment_count，把当前水平相位的纹理切成跨越
; 相邻字符行的上下两片。只写 UPPER/LOWER，保留仍可能被可见屏引用的 FULL。
; EN: Split the horizontally animated RAM patterns into UPPER and LOWER rows.
update_conveyor_fragment_glyphs:
    ; 第一步把四个目标字形清成黑色。未被下面复制覆盖的像素行就是裁切区。
    ldy #7
    lda #$00
@clear:
    sta conveyor_left_lower_glyph, y
    sta conveyor_left_upper_glyph, y
    sta conveyor_right_lower_glyph, y
    sta conveyor_right_upper_glyph, y
    dey
    bpl @clear

    ; 源字形顶部 fragment_count 行进入上一字符格，从 fragment_split 行开始。
    ldx #0
    ldy fragment_split
@upper:
    lda conveyor_left_pattern, x
    sta conveyor_left_upper_glyph, y
    lda conveyor_right_pattern, x
    sta conveyor_right_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @upper

    ; 源字形剩余的底部行移到当前字符格顶部，使用独立 LOWER 槽。
    ldx fragment_count
    ldy #0
@lower:
    lda conveyor_left_pattern, x
    sta conveyor_left_lower_glyph, y
    lda conveyor_right_pattern, x
    sta conveyor_right_lower_glyph, y
    inx
    iny
    cpx #8
    bne @lower
    rts

; EN: Refresh independent FULL slots without altering visible LOWER fragments.
restore_conveyor_full_glyphs:
    ; 完成八像素粗滚动、生成底部新平台之前调用。只恢复两个 FULL 槽；
    ; 不改写旧屏上的 UPPER/LOWER，完整字形由下一张屏幕使用。
    ldy #7
@copy:
    lda conveyor_left_pattern, y
    sta conveyor_left_full_glyph, y
    lda conveyor_right_pattern, y
    sta conveyor_right_full_glyph, y
    dey
    bpl @copy
    rts

.assert * <= $3800, error, "conveyor-platform code exceeds reserved gameplay memory"

.segment "CHARSETCODE"

; EN: One call per game frame; rotate paired bits without moving platform bounds.
update_conveyor_animation:
    dec conveyor_animation_counter
    bne @done
    lda #CONVEYOR_ANIMATION_FRAMES
    sta conveyor_animation_counter

    ; 先把移出的边缘位放入 C，再旋转 RAM 字节，实现首尾相接的循环移位。
    ; 每行连续两次，保证白/蓝多色位对不被拆开。
    ldx #7
@rotate:
    lda conveyor_left_pattern, x
    asl
    rol conveyor_left_pattern, x
    lda conveyor_left_pattern, x
    asl
    rol conveyor_left_pattern, x
    lda conveyor_right_pattern, x
    lsr
    ror conveyor_right_pattern, x
    lda conveyor_right_pattern, x
    lsr
    ror conveyor_right_pattern, x
    dex
    bpl @rotate

    lda fine_scroll
    cmp #7
    beq @full
    clc
    adc #1
    sta fragment_split
    lda #7
    sec
    sbc fine_scroll
    sta fragment_count
    jmp update_conveyor_fragment_glyphs
@full:
    jmp restore_conveyor_full_glyphs
@done:
    rts

.assert * <= $5400, error, "conveyor animation exceeds charset-code memory"
