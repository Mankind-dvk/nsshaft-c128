; Row-indexed platform descriptions and hidden-screen reconstruction.
; Gameplay permits one platform per row, with a blank row above each platform.
; Collision still samples the visible matrix; effects update both that matrix
; and the canonical FULL code before a later reconstruction can use it.
.segment "CODE"

reset_platform_rows:
    lda #0
    ldx #PLAYFIELD_ROW_COUNT-1
@clear:
    sta platform_row_start, x
    sta platform_row_width, x
    sta platform_row_full, x
    dex
    bpl @clear
    rts

; ROW_PTR names an aligned row in either screen. Preserve the seed loop's X.
record_platform_row:
    txa
    pha
    ldx #PLAYFIELD_ROW_COUNT-1
@find:
    lda playfield_row_low, x
    cmp ROW_PTR
    bne @next
    lda ROW_PTR+1
    and #3
    cmp playfield_row_high, x
    beq @store
@next:
    dex
    bpl @find
    bmi @done
@store:
    lda platform_start
    sta platform_row_start, x
    lda platform_width
    sta platform_row_width, x
    ldy platform_type
    lda platform_chars, y
    sta platform_row_full, x
@done:
    pla
    tax
    rts

advance_platform_rows:
    ldx #0
@shift:
    lda platform_row_start+1, x
    sta platform_row_start, x
    lda platform_row_width+1, x
    sta platform_row_width, x
    lda platform_row_full+1, x
    sta platform_row_full, x
    inx
    cpx #PLAYFIELD_ROW_COUNT-1
    bne @shift
    lda #0
    sta platform_row_start+PLAYFIELD_ROW_COUNT-1
    sta platform_row_width+PLAYFIELD_ROW_COUNT-1
    sta platform_row_full+PLAYFIELD_ROW_COUNT-1
    rts

.segment "CHARSETCODE"

update_fade_row_description:
    lda fade_platform_row
    sec
    sbc #PLAYFIELD_TOP_ROW
    bcc @done
    cmp #PLAYFIELD_ROW_COUNT
    bcs @done
    tax
    pha
    lda platform_row_full, x
    jsr transform_fade_character
    tay
    pla
    tax
    tya
    sta platform_row_full, x
    bne @done
    sta platform_row_width, x
@done:
    rts

update_spring_row_description:
    lda spring_platform_row
    sec
    sbc #PLAYFIELD_TOP_ROW
    bcc @done
    cmp #PLAYFIELD_ROW_COUNT
    bcs @done
    tax
    pha
    lda platform_row_full, x
    jsr transform_spring_character
    tay
    pla
    tax
    tya
    sta platform_row_full, x
@done:
    rts

.segment "CODE"

; Clear only hidden playfield cells, never HUD, frame, or sprite pointers.
; Unroll rows so each column needs one loop branch rather than twenty.
clear_hidden_playfield:
    lda active_screen
    beq @screen_b
    lda #>SCREEN_A
    sta platform_render_bank
    lda #CHAR_BLANK
    ldx #PLAYFIELD_LAST_COLUMN-PLAYFIELD_FIRST_COLUMN
@clear_a:
    .repeat PLAYFIELD_ROW_COUNT, row
        sta SCREEN_A+(PLAYFIELD_TOP_ROW+row)*SCREEN_COLUMNS+PLAYFIELD_FIRST_COLUMN, x
    .endrepeat
    dex
    bpl @clear_a
    rts
@screen_b:
    lda #>SCREEN_B
    sta platform_render_bank
    lda #CHAR_BLANK
    ldx #PLAYFIELD_LAST_COLUMN-PLAYFIELD_FIRST_COLUMN
@clear_b:
    .repeat PLAYFIELD_ROW_COUNT, row
        sta SCREEN_B+(PLAYFIELD_TOP_ROW+row)*SCREEN_COLUMNS+PLAYFIELD_FIRST_COLUMN, x
    .endrepeat
    dex
    bpl @clear_b
    rts

render_hidden_platforms:
    jsr clear_hidden_playfield
    ldx #PLAYFIELD_ROW_COUNT-1
@row:
    lda platform_row_width, x
    beq @next
    clc
    adc platform_row_start, x
    sta platform_render_end
    jsr set_platform_render_pointers
    ldy platform_row_full, x
    lda game_character_colors, y
    sta selected_platform_color
    lda fine_scroll
    cmp #7
    beq @full
    lda fragment_lower_codes, y
    jsr draw_platform_span
    cpx #0
    beq @next
    ; UPPER is clipped at the playfield top; the fixed ceiling is never touched.
    dex
    jsr set_platform_render_pointers
    inx
    ldy platform_row_full, x
    lda fragment_upper_codes, y
    jsr draw_platform_span
    jmp @next
@full:
    tya
    jsr draw_platform_span
@next:
    dex
    bpl @row
    rts

; X remains the descriptor index; Y walks the platform's contiguous columns.
draw_platform_span:
    sta selected_platform_char
    ldy platform_row_start, x
@column:
    lda selected_platform_char
    sta (DEST_PTR), y
    lda selected_platform_color
    sta (COLOR_PTR), y
    iny
    cpy platform_render_end
    bne @column
    rts

set_platform_render_pointers:
    lda playfield_row_low, x
    sta DEST_PTR
    sta COLOR_PTR
    lda playfield_row_high, x
    ora platform_render_bank
    sta DEST_PTR+1
    lda playfield_row_high, x
    ora #>COLOR_RAM
    sta COLOR_PTR+1
    rts

playfield_row_low:
    .repeat PLAYFIELD_ROW_COUNT, row
        .byte <((PLAYFIELD_TOP_ROW+row)*SCREEN_COLUMNS)
    .endrepeat
playfield_row_high:
    .repeat PLAYFIELD_ROW_COUNT, row
        .byte >((PLAYFIELD_TOP_ROW+row)*SCREEN_COLUMNS)
    .endrepeat
