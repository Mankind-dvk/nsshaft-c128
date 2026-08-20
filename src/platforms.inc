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

.segment "CODE"

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
    jsr draw_platform
    inx
    cpx #SEED_PLATFORM_COUNT
    bne @loop
    rts

; 使用机器启动后持续运行的时钟，以及已加载程序之外的 RAM 内容生成
; PRNG 种子。如果混合后的所有位恰好全部抵消，则使用非零后备种子。
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

; 平台宽度为 7..12 个字符。平台始终位于青色外框与分隔条之间，绝不会
; 覆盖 UI 字符。
randomize_platform:
    ; 输出：
    ;   platform_width = 7..12 字符；
    ;   platform_start = 当前宽度下不会越过游戏区的合法起始列；
    ;   platform_type  = 0..5。
    ; “随机后拒绝”的循环避免简单取模在范围不是 2 的幂时产生偏差。
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

    ; 平台类型也使用同一条 PRNG 序列：0=普通、1=尖刺、2=消失、3=弹簧、
    ; 4=向左传送、5=向右传送。屏蔽后得到 0..7，再拒绝 6/7，避免用简单
    ; 取模令某些类型概率更高；六种平台因此各占约六分之一。
@choose_type:
    jsr random_byte
    and #%00000111
    cmp #PLATFORM_TYPE_COUNT
    bcs @choose_type
    sta platform_type
    rts

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
