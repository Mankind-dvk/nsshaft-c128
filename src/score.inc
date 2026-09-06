; ---------------------------------------------------------------------------
; 分数与滚动难度
; ---------------------------------------------------------------------------
; 分数表示首次到达的新平台数量，包括尖刺接触。每个游戏区逻辑行都有一个
; 计分状态，状态与粗滚动同步上移；第一次接触将 claimed 位设为 1，之后
; 无论弹簧重落、走开再回来或平台继续上移，都不会再次得分。
;
; 右栏只有八个字符宽，因此显示为 P:000000。六位数字分别保存为 0..9 的
; 独立字节；加分、显示和比较都不需要 6502 不擅长的除法，也不需要 SED，
; 从而不会让异步 SID IRQ 意外继承十进制标志。
;
; 滚动采用 8 位相位累加器。每帧加一次 scroll_rate，只有产生进位时才滚动
; 一像素。速度只在首次落地分数达到 20/40/60/80 时提升，不再由滚动距离
; 自己推动自己，因此没有“速度越快 -> 得分越快 -> 再加速”的正反馈。
; 同一档位还会更新 SID 的模 512 音乐相位增量，让节奏与平台速度一起变快。
;
; English summary:
;   Score counts first contacts with distinct platform rows, including spikes,
;   and is displayed as P:000000. Per-row claimed flags survive scrolling and
;   prevent duplicate awards. Scroll speed and SID tempo rise together at
;   20/40/60/80 points.

.segment "CODE"

; EN: Clear score, draw its fixed label, and restore base scroll/music rates.
initialize_score:
    ; 输出：score=000000，清空平台领取状态，右栏显示 P:000000，并让滚动与
    ; SID 音乐都恢复基础速度。
    lda #0
    sta scroll_accumulator
    ldx #SCORE_DIGIT_COUNT-1
@clear_digits:
    sta score_digits, x
    dex
    bpl @clear_digits

    ldx #PLAYFIELD_ROW_COUNT-1
@clear_platform_rows:
    sta platform_score_rows, x
    dex
    bpl @clear_platform_rows

    lda #SCROLL_RATE_LEVEL_0
    sta scroll_rate
    lda #MUSIC_TEMPO_LEVEL_0
    sta music_tempo_increment

    lda #CHAR_P
    STORE_SCREEN_PAIR SCORE_DISPLAY_OFFSET
    lda #CHAR_COLON
    STORE_SCREEN_PAIR SCORE_DISPLAY_OFFSET+1

    lda #COLOR_WHITE
    ldx #0
@color:
    sta COLOR_RAM+SCORE_DISPLAY_OFFSET, x
    inx
    cpx #2+SCORE_DIGIT_COUNT
    bne @color
    jmp render_score_digits

; EN: Return C=1 when the phase accumulator schedules a one-pixel world step.
scroll_step_ready:
    ; 输入：scroll_accumulator、scroll_rate。输出：更新 accumulator；C=1 表示
    ; 本帧应调用 scroll_one_pixel_up，C=0 表示保持位置。最高档用 rate=0
    ; 作为 256/256 的标记，直接每帧返回 C=1；其他档继续使用相位累加器。
    lda scroll_rate
    beq @every_frame
    clc
    adc scroll_accumulator
    sta scroll_accumulator
    rts
@every_frame:
    sec
    rts

; EN: Award a point only if collision_row names an unclaimed platform row.
award_platform_landing_score:
    ; collision_row 是绝对屏幕行；减去 PLAYFIELD_TOP_ROW 得到 0..20 的游戏区
    ; 索引。空行、越界行和已经领取的平台都直接返回。
    lda collision_row
    sec
    sbc #PLAYFIELD_TOP_ROW
    bcc @done
    cmp #PLAYFIELD_ROW_COUNT
    bcs @done
    tax
    lda platform_score_rows, x
    beq @done
    and #PLATFORM_SCORE_CLAIMED_MASK
    bne @done
    lda #PLATFORM_SCORE_CLAIMED
    sta platform_score_rows, x
    jmp add_platform_score
@done:
    rts

; EN: Move platform claim flags upward with each completed character-row scroll.
advance_platform_score_rows:
    ; 元素 1..20 复制到 0..19，最后一行清零。generate_bottom_row 若实际生成
    ; 平台，会随后把最后一行改成 PLATFORM_SCORE_UNCLAIMED。
    ldx #0
@shift:
    lda platform_score_rows+1, x
    sta platform_score_rows, x
    inx
    cpx #PLAYFIELD_ROW_COUNT-1
    bne @shift
    lda #0
    sta platform_score_rows+PLAYFIELD_ROW_COUNT-1
    rts

; EN: Add one newly landed platform, update difficulty, and redraw six digits.
add_platform_score:
    ; 从个位向左执行十进制进位。每个数字独立保存，所以 CMP #10 就足够。
    ldx #SCORE_DIGIT_COUNT-1
@increment:
    inc score_digits, x
    lda score_digits, x
    cmp #10
    bcc @updated
    lda #0
    sta score_digits, x
    dex
    bpl @increment

    ; 极端情况下 999999 再加一会饱和，而不是回到 000000 并降低速度。
    lda #9
    ldx #SCORE_DIGIT_COUNT-1
@saturate:
    sta score_digits, x
    dex
    bpl @saturate

@updated:
    jsr award_health_for_score
    jsr update_scroll_rate_from_score
    jsr update_platform_generation_level
    jmp render_score_digits

; EN: Map scores 0/20/40/60/80 to matching scroll-rate and SID-tempo entries.
update_scroll_rate_from_score:
    ; 100 分以上直接保持最高档。0..99 时只需比较十位与 2/4/6/8；X 最终
    ; 等于已经跨过的门槛数量，即速度表索引 0..4。
    lda score_digits
    ora score_digits+1
    ora score_digits+2
    ora score_digits+3
    bne @maximum
    lda score_digits+4
    ldx #0
@threshold:
    cmp score_level_tens, x
    bcc @load
    inx
    cpx #SCORE_LEVEL_THRESHOLD_COUNT
    bne @threshold
@maximum:
    ldx #SCROLL_LEVEL_MAX
@load:
    lda scroll_rate_table, x
    sta scroll_rate
    lda music_tempo_rate_table, x
    sta music_tempo_increment
    rts

; EN: Copy the six decimal digits to both screen buffers as custom glyph codes.
render_score_digits:
    ; 两张屏幕必须同时更新，否则下一次 $d018 翻页会短暂显示旧分数。
    ldx #0
@digit:
    lda score_digits, x
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR SCORE_DISPLAY_OFFSET+2, x
    inx
    cpx #SCORE_DIGIT_COUNT
    bne @digit
    rts

scroll_rate_table:
    .byte SCROLL_RATE_LEVEL_0, SCROLL_RATE_LEVEL_1, SCROLL_RATE_LEVEL_2
    .byte SCROLL_RATE_LEVEL_3, SCROLL_RATE_LEVEL_4
scroll_rate_table_end:
.assert scroll_rate_table_end-scroll_rate_table = SCROLL_LEVEL_MAX+1, error, "scroll rate table size mismatch"

; 与滚动的 1/1.25/1.5/1.75/2 倍使用同一索引。模 512 相位让 55 和 77
; 可以精确表示两个带 0.25 的倍率，不需要在相邻整数速度间抖动。
music_tempo_rate_table:
    .byte MUSIC_TEMPO_LEVEL_0, MUSIC_TEMPO_LEVEL_1, MUSIC_TEMPO_LEVEL_2
    .byte MUSIC_TEMPO_LEVEL_3, MUSIC_TEMPO_LEVEL_4
music_tempo_rate_table_end:
.assert music_tempo_rate_table_end-music_tempo_rate_table = SCROLL_LEVEL_MAX+1, error, "music tempo table size mismatch"

score_level_tens:
    .byte 2, 4, 6, 8
score_level_tens_end:
.assert score_level_tens_end-score_level_tens = SCORE_LEVEL_THRESHOLD_COUNT, error, "score threshold table size mismatch"
