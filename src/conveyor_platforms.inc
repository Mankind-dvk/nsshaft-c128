; ---------------------------------------------------------------------------
; 左/右传送平台的动态字形
; ---------------------------------------------------------------------------
; 传送平台本身不做水平位移；连续的“<”或“>”表示传送方向，人物站在上面时
; 由 player.inc 每三帧水平推动两像素。平台仍与其他类型一起向上平滑滚动。
;
; ECM 只能提供四个全局背景色，当前已经分配给黑、白、红、灰。为了得到
; “绿色底、黑色镂空箭头”，传送平台改用普通黑色背景和绿色 Color RAM：
; 字形位 1 显示绿色，位 0 露出黑色背景。
;
; 64 字符 ECM 字符集没有六个空闲槽。本模块在游戏画面期间复用 N、S、-、V
; 四个标题字形槽：每个方向只需要 FULL/LOWER 共用槽和 UPPER 槽。粗滚动
; 完成后把共用槽恢复成完整箭头；回到菜单前再恢复四个原始文字字形。
;
; 公共入口：
;   initialize_conveyor_platforms、update_conveyor_fragment_glyphs、
;   restore_conveyor_full_glyphs、restore_modal_font_glyphs。
;
; English summary:
;   Builds green left/right cut-out arrow glyphs and their smooth-scroll
;   fragments. Each FULL slot doubles as LOWER. Gameplay temporarily reuses the
;   N/S/-/V font slots; modal entry restores those four glyphs from the immutable
;   table below. Player movement itself is implemented in player.inc.

.segment "GAMEPLAY"

; EN: Install full arrow glyphs and clear both UPPER runtime slots for gameplay.
initialize_conveyor_platforms:
    ; 新游戏已经清空两张屏幕，所以此时覆盖 N/S/-/V 不会破坏可见文字。
    ; 完整平台使用 LEFT/RIGHT_FULL；两个 UPPER 槽等第一次滚动时再生成。
    jsr restore_conveyor_full_glyphs
    ldy #7
    lda #$00
@clear_upper:
    sta conveyor_left_upper_glyph, y
    sta conveyor_right_upper_glyph, y
    dey
    bpl @clear_upper
    rts

; 根据 video.inc 算出的 fragment_split/fragment_count，把完整箭头切成跨越
; 相邻字符行的上下两片。FULL 槽在过渡期间直接承担 LOWER，因此每个方向
; 只消耗两个运行时字符，而不是完整、上部、下部三个字符。
; EN: Split immutable left/right arrow templates into runtime UPPER and LOWER rows.
update_conveyor_fragment_glyphs:
    ; 第一步把四个目标字形清成黑色。未被下面复制覆盖的像素行就是裁切区。
    ldy #7
    lda #$00
@clear:
    sta conveyor_left_full_glyph, y
    sta conveyor_left_upper_glyph, y
    sta conveyor_right_full_glyph, y
    sta conveyor_right_upper_glyph, y
    dey
    bpl @clear

    ; 源字形顶部 fragment_count 行进入上一字符格，从 fragment_split 行开始。
    ldx #0
    ldy fragment_split
@upper:
    lda conveyor_left_source_glyph, x
    sta conveyor_left_upper_glyph, y
    lda conveyor_right_source_glyph, x
    sta conveyor_right_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @upper

    ; 源字形剩余的底部行移到当前字符格顶部。FULL 槽此时就是 LOWER 碎片。
    ldx fragment_count
    ldy #0
@lower:
    lda conveyor_left_source_glyph, x
    sta conveyor_left_full_glyph, y
    lda conveyor_right_source_glyph, x
    sta conveyor_right_full_glyph, y
    inx
    iny
    cpx #8
    bne @lower
    rts

; EN: Restore FULL/LOWER shared slots before exposing a completed coarse scroll.
restore_conveyor_full_glyphs:
    ; 完成八像素粗滚动、生成底部新平台之前调用。只恢复两个 FULL 槽；
    ; UPPER 槽在当前完整字符画面中无人引用，可以留到下次滚动再清空。
    ldy #7
@copy:
    lda conveyor_left_source_glyph, y
    sta conveyor_left_full_glyph, y
    lda conveyor_right_source_glyph, y
    sta conveyor_right_full_glyph, y
    dey
    bpl @copy
    rts

; EN: Restore original N/S/-/V bytes before drawing title or game-over text.
restore_modal_font_glyphs:
    ; N/S/- 连续占据 19..21；V 位于 30。H 从未覆盖，游戏中可稳定显示 HP。
    ldx #0
@copy_19_21:
    lda modal_font_19_21, x
    sta conveyor_left_full_glyph, x
    inx
    cpx #24
    bne @copy_19_21

    ldy #7
@copy_v:
    lda modal_font_v, y
    sta conveyor_right_upper_glyph, y
    dey
    bpl @copy_v
    rts

; 黑色“<”和“>”是在全绿字符中把对应位清零形成的。模板放在 GAMEPLAY
; 段中，运行时只读；CHARSET 中的四个目标槽则允许逐帧改写。
conveyor_left_source_glyph:
    .byte $f9,$f3,$e7,$cf,$e7,$f3,$f9,$ff
conveyor_right_source_glyph:
    .byte $9f,$cf,$e7,$f3,$e7,$cf,$9f,$ff

; 字符 19..21 与 30 原始的 N、S、连字符、V。进入模态画面前恢复。
modal_font_19_21:
    .byte $66,$76,$7e,$7e,$6e,$66,$66,$00
    .byte $3c,$66,$60,$3c,$06,$66,$3c,$00
    .byte $00,$00,$00,$7e,$00,$00,$00,$00
modal_font_v:
    .byte $66,$66,$66,$66,$66,$3c,$18,$00

.assert * <= $3800, error, "conveyor-platform code exceeds reserved gameplay memory"
