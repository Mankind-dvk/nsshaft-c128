; 平台材质与颜色 / Platform materials and Color RAM
; ---------------------------------------------------------------------------
; 多色砖块使用红、白、黑三色，黑色保留滚动碎片外的空白区域。
; 文字、表盘和弹簧使用高分辨率字符；传送平台使用白蓝多色纹理。
; EN: Rebuild multicolor rows, preserving black outside the moving platform.
; Keep this renderer beside the charset loader, away from tightly packed sprites.
.segment "CHARSETCODE"

update_brick_and_fade_fragments:
    ldy #7
    lda #0
@clear:
    sta normal_upper_glyph, y
    sta normal_lower_glyph, y
    sta fade_upper_glyph, y
    sta fade_lower_glyph, y
    sta fade_cracked_upper_glyph, y
    sta fade_cracked_lower_glyph, y
    sta fade_broken_upper_glyph, y
    sta fade_broken_lower_glyph, y
    dey
    bpl @clear

    ; 源图案按行分成两段：前段放上一格末尾，后段放当前格开头。
    ; EN: Byte-row copies keep both the mortar pattern and 1-pixel vertical motion.
    ldx #0
    ldy fragment_split
@upper:
    lda brick_source_glyph, x
    sta normal_upper_glyph, y
    lda fade_source_glyph, x
    sta fade_upper_glyph, y
    lda fade_cracked_source_glyph, x
    sta fade_cracked_upper_glyph, y
    lda fade_broken_source_glyph, x
    sta fade_broken_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @upper

    ldx fragment_count
    ldy #0
@lower:
    lda brick_source_glyph, x
    sta normal_lower_glyph, y
    lda fade_source_glyph, x
    sta fade_lower_glyph, y
    lda fade_cracked_source_glyph, x
    sta fade_cracked_lower_glyph, y
    lda fade_broken_source_glyph, x
    sta fade_broken_lower_glyph, y
    inx
    iny
    cpx #8
    bne @lower
    rts

; Color RAM bit 3=1 为多色；0 为单色。低三位是该格前景色。
; EN: Shared multicolor slots: 00 black, 01 white, 10 gray, 11 local foreground.
game_character_colors:
    .byte COLOR_BLACK, BRICK_CELL_COLOR, SPIKE_CELL_COLOR ; 0..2
    .byte COLOR_PURPLE, COLOR_PURPLE, CONVEYOR_CELL_COLOR, CONVEYOR_CELL_COLOR, FADE_CELL_COLOR ; 3..7
    .byte BRICK_CELL_COLOR, BRICK_CELL_COLOR, SPIKE_CELL_COLOR, SPIKE_CELL_COLOR ; 8..11
    .res 4, COLOR_PURPLE        ; 12..15 spring fragments
    .byte FADE_CELL_COLOR, FADE_CELL_COLOR, CONVEYOR_CELL_COLOR, CONVEYOR_CELL_COLOR ; 16..19
    .byte UI_FRAME_COLOR, SPIKE_CELL_COLOR ; 20..21
    .res 34, COLOR_WHITE        ; 22..55 dial and text
    .res 6, FADE_CELL_COLOR     ; 56..61 intact/broken fade variants
    .res 4, COLOR_WHITE         ; 62..65 SCORE letters
game_character_colors_end:
.assert game_character_colors_end-game_character_colors = CHARSET_GLYPH_COUNT, error, "Color table size mismatch"

; Screen-code -> logical support type. UPPER fragments stay non-solid so the
; collision anchor remains on FULL/LOWER instead of moving up one character row.
collision_type_by_char:
.repeat CHARSET_GLYPH_COUNT, code
    .if code = CHAR_PLATFORM_FULL
        .byte COLLISION_NORMAL
    .elseif code = CHAR_NORMAL_LOWER
        .byte COLLISION_NORMAL
    .elseif code = CHAR_SPIKE_TRIANGLE
        .byte COLLISION_SPIKES
    .elseif code = CHAR_SPIKE_LOWER
        .byte COLLISION_SPIKES
    .elseif code = CHAR_FADE_FULL
        .byte COLLISION_FADE_INACTIVE
    .elseif code = CHAR_FADE_LOWER
        .byte COLLISION_FADE_INACTIVE
    .elseif code = CHAR_FADE_CRACKED_FULL
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_FADE_CRACKED_LOWER
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_FADE_BROKEN_FULL
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_FADE_BROKEN_LOWER
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_SPRING_FULL
        .byte COLLISION_SPRING
    .elseif code = CHAR_SPRING_LOWER
        .byte COLLISION_SPRING
    .elseif code = CHAR_SPRING_COMPRESSED_FULL
        .byte COLLISION_SPRING
    .elseif code = CHAR_SPRING_COMPRESSED_LOWER
        .byte COLLISION_SPRING
    .elseif code = CHAR_CONVEYOR_LEFT_FULL
        .byte COLLISION_CONVEYOR_LEFT
    .elseif code = CHAR_CONVEYOR_RIGHT_FULL
        .byte COLLISION_CONVEYOR_RIGHT
    .else
        .byte COLLISION_NONE
    .endif
.endrepeat
collision_type_by_char_end:
.assert collision_type_by_char_end-collision_type_by_char = CHARSET_GLYPH_COUNT, error, "Collision table size mismatch"

platform_colors:
    .byte BRICK_CELL_COLOR, SPIKE_CELL_COLOR, FADE_CELL_COLOR
    .byte COLOR_PURPLE, CONVEYOR_CELL_COLOR, CONVEYOR_CELL_COLOR

; 编译期生成两个 66 项转换表，每个平台查一次 FULL -> LOWER/UPPER。
; EN: FULL -> LOWER; unrelated occupied cells are retained.
fragment_lower_codes:
.repeat CHARSET_GLYPH_COUNT, code
    .if code = CHAR_PLATFORM_FULL
        .byte CHAR_NORMAL_LOWER
    .elseif code = CHAR_SPIKE_TRIANGLE
        .byte CHAR_SPIKE_LOWER
    .elseif code = CHAR_SPRING_FULL
        .byte CHAR_SPRING_LOWER
    .elseif code = CHAR_SPRING_COMPRESSED_FULL
        .byte CHAR_SPRING_COMPRESSED_LOWER
    .elseif code = CHAR_FADE_FULL
        .byte CHAR_FADE_LOWER
    .elseif code = CHAR_FADE_CRACKED_FULL
        .byte CHAR_FADE_CRACKED_LOWER
    .elseif code = CHAR_FADE_BROKEN_FULL
        .byte CHAR_FADE_BROKEN_LOWER
    .else
        .byte code
    .endif
.endrepeat

; EN: The next row's FULL creates UPPER in the empty cell above it.
fragment_upper_codes:
.repeat CHARSET_GLYPH_COUNT, code
    .if code = CHAR_PLATFORM_FULL
        .byte CHAR_NORMAL_UPPER
    .elseif code = CHAR_SPIKE_TRIANGLE
        .byte CHAR_SPIKE_UPPER
    .elseif code = CHAR_SPRING_FULL
        .byte CHAR_SPRING_UPPER
    .elseif code = CHAR_SPRING_COMPRESSED_FULL
        .byte CHAR_SPRING_COMPRESSED_UPPER
    .elseif code = CHAR_CONVEYOR_LEFT_FULL
        .byte CHAR_CONVEYOR_LEFT_UPPER
    .elseif code = CHAR_CONVEYOR_RIGHT_FULL
        .byte CHAR_CONVEYOR_RIGHT_UPPER
    .elseif code = CHAR_FADE_FULL
        .byte CHAR_FADE_UPPER
    .elseif code = CHAR_FADE_CRACKED_FULL
        .byte CHAR_FADE_CRACKED_UPPER
    .elseif code = CHAR_FADE_BROKEN_FULL
        .byte CHAR_FADE_BROKEN_UPPER
    .else
        .byte CHAR_BLANK
    .endif
.endrepeat

.assert * <= $5400, error, "Material renderer exceeds charset-code memory"
