; ---------------------------------------------------------------------------
; 平台生成与伪随机序列
; ---------------------------------------------------------------------------
; 在当前游戏区域宽度内生成普通、尖刺、消失、弹簧和左右传送平台。平台
; 类型、宽度和水平位置全部由同一个 16 位状态驱动，因此只要启动种子相同，
; 就能完整复现整个平台序列。
;
; 公共入口：
;   seed_platforms、seed_random、randomize_platform、draw_platform。
;
; 调用约定：randomize_platform 把结果写入 platform_start/width/type；
; draw_platform 从这些状态和 ROW_PTR 取输入。除 random_byte 在 A 中返回值外，
; 调用者不应依赖例程返回后的 A/X/Y。
;
; English summary:
;   Generates width, legal X position, and a score-gated weighted platform type
;   from one seeded 16-bit LFSR. Normal platforms may repeat; every other type is
;   capped at two consecutive results. X is preserved for seed_platforms.

.segment "CODE"

; EN: Draw five initial platforms into SCREEN_A; the lowest is forced safe.
seed_platforms:
    ; 输入：SCREEN_A 已清空，random_state 已由 seed_random 初始化。
    ; 输出：在五个固定逻辑行画出初始平台；最后一行强制为安全出生平台。
    ; X 同时是地址表索引。被调用例程必须保持 X，当前实现满足这一约束。
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
    lda #PLATFORM_TYPE_FADE
.elseif .defined(SPRING_PLATFORM_TEST)
    ; 自动化版本把出生平台设为弹簧，便于直接观察压缩和弹跳流程。
    lda #PLATFORM_TYPE_SPRING
.else
    lda #PLATFORM_TYPE_NORMAL
.endif
    sta platform_type
@draw:
    ; randomize_platform 只提出候选；出生平台可能在上方被测试配置覆盖。
    ; 因此必须到最终 platform_type 已确定后，才更新连续类型记录。
    jsr commit_generated_platform_type
    jsr draw_platform

    ; 平台计分状态按游戏区相对行保存。出生平台直接标成已领取，避免人物离开
    ; 后重新落回出生台阶时获得第一分；其余初始平台等待首次落地。
    ldy seed_platform_score_index, x
    lda #PLATFORM_SCORE_UNCLAIMED
    cpx #SEED_PLATFORM_COUNT-1
    bne @store_score_state
    lda #PLATFORM_SCORE_CLAIMED
@store_score_state:
    sta platform_score_rows, y

    inx
    cpx #SEED_PLATFORM_COUNT
    bne @loop
    rts

; 使用机器启动后持续运行的时钟，以及已加载程序之外的 RAM 内容生成
; PRNG 种子。如果混合后的所有位恰好全部抵消，则使用非零后备种子。
; EN: Mix clocks, timers, raster, and RAM into a nonzero 16-bit PRNG state.
seed_random:
    ; 输出：random_state_low/high 得到非零的 16 位种子。
    ; EOR 混合多个会随开机/装载时刻变化的字节；它不是密码学随机数，只用于
    ; 让每局的平台布局不同。最后的 OR 只检测两个字节是否同时为 0。
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

; 16 位 Galois LFSR，多项式为 $b400；每次推进八位后，在 A 中返回混合字节。
; EN: Advance the 16-bit Galois LFSR by eight bits; return high XOR low in A.
random_byte:
    ; 旧实现每次调用只推进 1 bit，却把结果当作一个全新的随机字节使用。
    ; 相邻返回值因此共享七位，platform_type 再取低三位时会出现明显的方向
    ; 连续性。这里连续执行八次反馈，使相邻返回字节不再是简单的一位平移。
    ;
    ; X 必须保持不变，因为 seed_platforms 用 X 遍历初始平台地址表；Y 是本
    ; 例程允许破坏的临时八次计数器。最终把 high/low 异或，避免调用者总是
    ; 只观察 LFSR 状态的一半。状态推进仍是同一条 65535 周期序列。
    ldy #8
@step:
    lsr random_state_high
    ror random_state_low
    bcc @next_step
    lda random_state_high
    eor #$b4
    sta random_state_high
@next_step:
    dey
    bne @step

    lda random_state_low
    eor random_state_high
    rts

.segment "EXTCODE"

; EN: Reset score-gated type selection and consecutive-platform history.
initialize_platform_generation:
    lda #0
    sta platform_generation_level
    sta generated_type_run_length
    lda #$ff
    sta last_generated_platform_type
    rts

; 根据首次落地分数选择五档平台池：0..4、5..9、10..19、20..39、40+。
; 高四位十进制数字非零时必然已经超过 40，直接使用最高档。
; EN: Map the decimal score to generation levels at 5, 10, 20, and 40 points.
update_platform_generation_level:
    lda score_digits
    ora score_digits+1
    ora score_digits+2
    ora score_digits+3
    bne @level_4

    lda score_digits+4
    beq @under_ten
    cmp #1
    beq @level_2
    cmp #4
    bcs @level_4
    lda #3
    bne @store

@under_ten:
    lda score_digits+5
    cmp #5
    bcc @level_0
    lda #1
    bne @store
@level_0:
    lda #0
    beq @store
@level_2:
    lda #2
    bne @store
@level_4:
    lda #PLATFORM_GENERATION_LEVEL_MAX
@store:
    sta platform_generation_level
    rts

; 平台宽度为 7..12 个字符。平台始终位于青色外框与分隔条之间，绝不会
; 覆盖 UI 字符。
; EN: Produce unbiased width/start plus a weighted, score-gated platform type.
randomize_platform:
    ; 输出：
    ;   platform_width = 7..12 字符；
    ;   platform_start = 当前宽度下不会越过游戏区的合法起始列；
    ;   platform_type  = 当前分数档允许的加权类型。
    ; 宽度/位置的“随机后拒绝”避免范围不是 2 的幂时产生取模偏差。
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

    ; 类型池长度固定为 4 或 8，所以可直接用 AND 取得无偏索引。每档的偏移
    ; 和掩码把同一随机字节映射到对应权重表。普通平台允许无限连续；其他
    ; 类型若已经连续两个，则消耗新的 PRNG 字节重选，绝不会生成第三个。
@choose_type:
    jsr random_byte
    ldy platform_generation_level
    and platform_type_pool_mask, y
    clc
    adc platform_type_pool_offset, y
    tay
    lda platform_type_pool, y
    sta platform_type
    beq @type_ready
    cmp last_generated_platform_type
    bne @type_ready
    lda generated_type_run_length
    cmp #PLATFORM_REPEAT_LIMIT
    bcs @choose_type
@type_ready:
    rts

; 只有实际要绘制的平台才进入历史。普通平台重复时把长度保持为 1，既表示
; 它是最近类型，又避免长局中计数器溢出；普通平台本身从不检查该上限。
; EN: Commit the final drawn type; normal is exempt and cannot overflow the run.
commit_generated_platform_type:
    lda platform_type
    cmp last_generated_platform_type
    bne @new_type
    cmp #PLATFORM_TYPE_NORMAL
    beq @normal_repeat
    inc generated_type_run_length
    rts
@normal_repeat:
    lda #1
    sta generated_type_run_length
    rts
@new_type:
    sta last_generated_platform_type
    lda #1
    sta generated_type_run_length
    rts

; 五个档位的普通平台概率依次为 75%、50%、37.5%、25%、12.5%。
; 非普通类型逐步解锁：弹簧 -> 消失 -> 双向传送 -> 尖刺。
platform_type_pool_offset:
    .byte 0, 4, 8, 16, 24
platform_type_pool_mask:
    .byte 3, 3, 7, 7, 7
platform_type_pool:
    ; 0..4 分：75% 普通，25% 弹簧。
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_SPRING
    ; 5..9 分：50% 普通，25% 消失，25% 弹簧。
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_FADE, PLATFORM_TYPE_SPRING
    ; 10..19 分：37.5% 普通，并解锁左右传送。
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_FADE, PLATFORM_TYPE_SPRING, PLATFORM_TYPE_SPRING
    .byte PLATFORM_TYPE_CONVEYOR_LEFT, PLATFORM_TYPE_CONVEYOR_RIGHT
    ; 20..39 分：25% 普通，并首次解锁尖刺。
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_SPIKES, PLATFORM_TYPE_FADE, PLATFORM_TYPE_FADE
    .byte PLATFORM_TYPE_SPRING
    .byte PLATFORM_TYPE_CONVEYOR_LEFT, PLATFORM_TYPE_CONVEYOR_RIGHT
    ; 40+ 分：12.5% 普通，提高尖刺和消失平台权重。
    .byte PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_SPIKES, PLATFORM_TYPE_SPIKES
    .byte PLATFORM_TYPE_FADE, PLATFORM_TYPE_FADE, PLATFORM_TYPE_SPRING
    .byte PLATFORM_TYPE_CONVEYOR_LEFT, PLATFORM_TYPE_CONVEYOR_RIGHT
platform_type_pool_end:
.assert platform_type_pool_end-platform_type_pool = 32, error, "platform type pool size mismatch"

.segment "CODE"

; EN: Draw platform_* to ROW_PTR and set matching shared Color RAM cells.
draw_platform:
    ; 输入：ROW_PTR 指向目标字符行，platform_start/width/type 已准备好。
    ; 输出：写目标屏幕缓冲区；同时写共享 Color RAM。弹簧使用紫色前景，
    ; 两种传送平台使用绿色前景，其他平台保持黑色前景作为 ECM 形状遮罩。
    ; COLOR_PTR 的构造利用 SCREEN_A、SCREEN_B 与 Color RAM 低十位布局相同。
    ldy platform_type
    lda platform_chars, y
    sta selected_platform_char

    ; 普通、尖刺和消失平台把 Color RAM 保持为黑色遮罩。弹簧改用紫色；
    ; 左右传送平台的字形位 1 使用绿色，位 0 露出黑色镂空箭头。
    lda #COLOR_BLACK
    cpy #PLATFORM_TYPE_SPRING
    bne @check_conveyor
    lda #COLOR_PURPLE
    bne @color_ready
@check_conveyor:
    cpy #PLATFORM_TYPE_CONVEYOR_LEFT
    bcc @color_ready
    lda #COLOR_GREEN
@color_ready:
    sta selected_platform_color

    lda ROW_PTR
    sta COLOR_PTR
    lda ROW_PTR+1
    and #%00000011
    ora #>COLOR_RAM
    sta COLOR_PTR+1

    ldy platform_start
    lda selected_platform_char
    sta (ROW_PTR), y
    lda selected_platform_color
    sta (COLOR_PTR), y
    iny

    lda platform_width
    sec
    sbc #2
    sta platform_middle_count
@middle:
    lda selected_platform_char
    sta (ROW_PTR), y
    lda selected_platform_color
    sta (COLOR_PTR), y
    iny
    dec platform_middle_count
    bne @middle

    lda selected_platform_char
    sta (ROW_PTR), y
    lda selected_platform_color
    sta (COLOR_PTR), y
    rts
