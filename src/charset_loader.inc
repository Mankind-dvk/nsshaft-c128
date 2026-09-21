; 两个输入字库 -> 一个输出字库 / Two inputs -> one live output
; ---------------------------------------------------------------------------
; 对应老师的 copyFromCharset / copyMemoryChunks：源地址加字形编号*8，
; 再按完整的 8 字节块复制到输出。源库始终不变，只有 $2800 输出区可修改。
; 课堂交互输入改为编译期选字表，输入文件随 PRG 内嵌，保持单文件启动。
; 不是每个字符各自切 D018，也没有新增光栅 IRQ。
; EN: Scene-entry composition only; no work added to the frame loop.
; Callers stop music/IRQs before changing scenes. Clobbers A/X/Y, flags and
; ROW_PTR/DEST_PTR. Loading leaves display off until drawing ends.
.segment "CHARSETCODE"

; 编译期选字范围，结束编号不包含在复制范围内，与老师的 until-from 相同。
; EN: [source_id, source_id+count) -> [destination_id, destination_id+count).
.macro COPY_CHARSET_RANGE source_charset, source_id, destination_id, glyph_count
    .assert (glyph_count > 0) .and (glyph_count < 256), error, "Invalid glyph count"
    .assert source_id+glyph_count <= SOURCE_CHARSET_GLYPH_COUNT, error, "Source range overflow"
    .assert destination_id+glyph_count <= CHARSET_GLYPH_COUNT, error, "Live charset overflow"
    SET_WORD ROW_PTR, source_charset + source_id*CHARSET_GLYPH_BYTES
    SET_WORD DEST_PTR, CHARSET_ADDRESS + destination_id*CHARSET_GLYPH_BYTES
    ldx #glyph_count
    jsr copy_charset_glyphs
.endmacro

; 游戏：图形库前 66 槽（含补零）打底，然后加入 HUD 使用的文字。
; EN: Import graphics, then digits and H/O/P/T/X/colon/S/C/R/E from source 2.
load_game_charset:
    jsr blank_charset_display
    COPY_CHARSET_RANGE graphics_charset, 0, 0, CHARSET_GLYPH_COUNT
    COPY_CHARSET_RANGE text_charset, TEXT_DIGIT_0, GLYPH_DIGIT_0, 10
    COPY_CHARSET_RANGE text_charset, TEXT_H, GLYPH_H, 1
    COPY_CHARSET_RANGE text_charset, TEXT_O, GLYPH_O, 2
    COPY_CHARSET_RANGE text_charset, TEXT_T, GLYPH_T, 1
    COPY_CHARSET_RANGE text_charset, TEXT_X, GLYPH_X, 1
    COPY_CHARSET_RANGE text_charset, TEXT_COLON, GLYPH_COLON, 1
    COPY_CHARSET_RANGE text_charset, TEXT_S, GLYPH_S, 1
    COPY_CHARSET_RANGE text_charset, TEXT_C, GLYPH_C, 1
    COPY_CHARSET_RANGE text_charset, TEXT_R, GLYPH_R, 1
    COPY_CHARSET_RANGE text_charset, TEXT_E, GLYPH_E, 1
    rts

; 菜单：基础文字页，26 字母与常用符号同时可用，无需借用碎片槽。
; EN: Whole 64-glyph uppercase text page, with standard C64 screen codes.
load_text_charset:
    jsr blank_charset_display
    COPY_CHARSET_RANGE text_charset, 0, 0, TEXT_QUESTION+1
    rts
.delmacro COPY_CHARSET_RANGE

; 关显示、关 sprite、装载文字页并清理两张矩阵。
; EN: Clear using text space (32), not game blank (0, which is @ in this font).
prepare_modal_screen:
    jsr load_text_charset
    lda #0
    sta VIC_SPRITE_ENABLE
    SHOW_SCREEN SCREEN_A_D018
    ldy #TEXT_SPACE
    jmp clear_screens_with_character

; 保留自然相位 3；游戏/菜单共用显示模式和 D018，文字格用 Color RAM 0..7。
; EN: Hide/show without selecting either source charset in D018.
blank_charset_display:
    lda #(VIC_CONTROL_1_TEXT & $ef)
    sta VIC_CONTROL_1
    lda #$18
    sta VIC_CONTROL_2
    lda #COLOR_BLACK
    sta VIC_BACKGROUND_COLOR
    lda #COLOR_WHITE
    sta VIC_MULTICOLOR_1
    lda #COLOR_GRAY
    sta VIC_MULTICOLOR_2
    rts
enable_charset_display:
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    rts

; 输入：ROW_PTR=源，DEST_PTR=输出，X=字形数；X=0 时不复制。
; 输出：两个指针都推进 X_initial*8，X=0；只写指定区间，支持跨页。
; EN: Copy X eight-byte glyphs with full pointer carries; 0 is a no-op.
copy_charset_glyphs:
    cpx #0
    beq @done
@glyph:
    ldy #0
@byte:
    lda (ROW_PTR), y
    sta (DEST_PTR), y
    iny
    cpy #CHARSET_GLYPH_BYTES
    bne @byte
    clc
    lda ROW_PTR
    adc #CHARSET_GLYPH_BYTES
    sta ROW_PTR
    bcc @source_ready
    inc ROW_PTR+1
@source_ready:
    clc
    lda DEST_PTR
    adc #CHARSET_GLYPH_BYTES
    sta DEST_PTR
    bcc @destination_ready
    inc DEST_PTR+1
@destination_ready:
    dex
    bne @glyph
@done:
    rts
charset_code_end:
.assert charset_code_end <= $5400, error, "Charset loader exceeds reserved memory"
