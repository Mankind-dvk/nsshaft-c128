; ---------------------------------------------------------------------------
; 平台生成与伪随机序列
; ---------------------------------------------------------------------------
; 在当前游戏区域宽度内生成普通、尖刺和消失平台。平台类型、宽度和水平
; 位置全部由同一个 16 位状态驱动，因此只要启动种子相同，就能完整复现
; 整个平台序列。
;
; 公共入口：
;   seed_platforms、seed_random、randomize_platform、draw_platform。

.segment "CODE"

seed_platforms:
    ldx #0
@loop:
    lda seed_platform_address_low, x
    sta ROW_PTR
    lda seed_platform_address_high, x
    sta ROW_PTR+1
    jsr randomize_platform

    ; 最低的初始平台始终为安全平台，用于放置玩家。
    cpx #SEED_PLATFORM_COUNT-1
    bne @draw
.ifdef FADE_PLATFORM_TEST
    ; 自动化版本让出生平台直接覆盖消失平台的激活、闪烁和删除流程。
    lda #2
.else
    lda #0
.endif
    sta platform_type
@draw:
    jsr draw_platform
    inx
    cpx #SEED_PLATFORM_COUNT
    bne @loop
    rts

; 使用机器启动后持续运行的时钟，以及已加载程序之外的 RAM 内容生成
; PRNG 种子。如果混合后的所有位恰好全部抵消，则使用非零后备种子。
seed_random:
    lda KERNAL_JIFFY_LOW
    eor CIA1_TIMER_A_LOW
    eor CIA1_TOD_TENTHS
    eor VIC_RASTER
    eor $3a23
    sta random_state_low

    lda KERNAL_JIFFY_MIDDLE
    eor CIA1_TIMER_A_HIGH
    eor CIA1_TIMER_B_LOW
    eor CIA1_TOD_SECONDS
    eor $3f57
    sta random_state_high

    lda KERNAL_JIFFY_HIGH
    eor CIA1_TOD_MINUTES
    eor random_state_low
    sta random_state_low

    lda random_state_low
    ora random_state_high
    bne @done
    lda #$a7
    sta random_state_low
    lda #$5c
    sta random_state_high
@done:
    rts

; 16 位 Galois LFSR，多项式为 $b400；在 A 中返回状态低字节。
random_byte:
    lsr random_state_high
    ror random_state_low
    bcc @done
    lda random_state_high
    eor #$b4
    sta random_state_high
@done:
    lda random_state_low
    rts

; 平台宽度为 7..12 个字符。平台始终位于青色外框与分隔条之间，绝不会
; 覆盖 UI 字符。
randomize_platform:
@choose_width:
    jsr random_byte
    and #%00000111
    cmp #PLATFORM_WIDTH_COUNT
    bcs @choose_width
    clc
    adc #PLATFORM_MIN_WIDTH
    sta platform_width

    ; 可用列为 PLAYFIELD_FIRST_COLUMN .. UI_DIVIDER_COLUMN-1。
    ; limit 是给定宽度下合法起点的数量，随机值再加左侧内边距。
    lda #UI_DIVIDER_COLUMN-PLAYFIELD_FIRST_COLUMN+1
    sec
    sbc platform_width
    sta platform_start_limit

@choose_start:
    jsr random_byte
    and #%00011111
    cmp platform_start_limit
    bcs @choose_start
    clc
    adc #PLAYFIELD_FIRST_COLUMN
    sta platform_start

    ; 平台类型也使用同一条 PRNG 序列：0=普通、1=尖刺、2=消失平台。
    ; 四种低两位组合中的 3 映射回普通平台，概率为 50%/25%/25%。
    jsr random_byte
    and #%00000011
    cmp #3
    bne @store_type
    lda #0
@store_type:
    sta platform_type
    rts

draw_platform:
    ldy platform_type
    lda platform_chars, y
    sta selected_platform_char

    ldy platform_start
    lda selected_platform_char
    sta (ROW_PTR), y
    iny

    lda platform_width
    sec
    sbc #2
    sta platform_middle_count
@middle:
    lda selected_platform_char
    sta (ROW_PTR), y
    iny
    dec platform_middle_count
    bne @middle

    lda selected_platform_char
    sta (ROW_PTR), y
    rts
