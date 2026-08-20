; ---------------------------------------------------------------------------
; Paddle 与动作按钮输入
; ---------------------------------------------------------------------------
; 选择控制端口 1 供 SID 执行 POTX 转换，并提供玩家控制器和 HUD 共用的
; 原始 8 位读数。按钮采样属于开始/结束画面的模态流程，因此保留在
; video.inc 中。
;
; 公共入口：initialize_paddles、read_paddle_x。

.segment "CODE"

; Paddle 输入 ---------------------------------------------------------------

initialize_paddles:
    ; PA6/PA7 选择连接到 SID POT 输入的控制端口。
    lda CIA1_DDR_A
    ora #%11000000
    sta CIA1_DDR_A
    lda CIA1_PORT_A
    and #%00111111
    ora #PADDLE_PORT_1_SELECT
    sta CIA1_PORT_A

    ; Paddle 按钮和摇杆 FIRE 都是 PB2..PB4 上的低电平有效输入。
    lda CIA1_DDR_B
    and #%11100011
    sta CIA1_DDR_B
    rts

read_paddle_x:
.ifdef FADE_PLATFORM_TEST
    ; 自动化效果测试保持 paddle 居中，让人物持续站在出生平台上。
    lda #128
    sta potx_raw
    rts
.endif
    lda CIA1_PORT_A
    and #%00111111
    ora #PADDLE_PORT_1_SELECT
    sta CIA1_PORT_A

    ; 选择端口后等待 SID 的 paddle ADC 转换结果稳定。
    ldx #$a0
@settle:
    dex
    bne @settle

    lda SID_POTX
    sta potx_raw
    rts
