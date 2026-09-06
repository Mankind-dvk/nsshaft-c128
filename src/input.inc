; ---------------------------------------------------------------------------
; Paddle 与动作按钮输入
; ---------------------------------------------------------------------------
; 选择控制端口 1 供 SID 执行 POTX 转换，并提供玩家控制器和 HUD 共用的
; 原始 8 位读数。按钮采样属于开始/结束画面的模态流程，因此保留在
; video.inc 中。
;
; 公共入口：initialize_paddles、read_paddle_x。
;
; 这里不做移动判断，只产生一个稳定的 potx_raw。这样玩家和 HUD 必然读取
; 同一帧的值，不会因各自再次访问 SID 而得到不同结果。
;
; English summary:
;   Selects control port 1 for SID POT conversion and publishes one raw POTX byte
;   per frame. This module does not interpret movement. Player physics and the
;   HUD therefore consume the same sampled value instead of reading SID twice.

.segment "CODE"

; Paddle 输入 ---------------------------------------------------------------

; EN: Configure CIA directions/selection for port-1 paddle POT and buttons.
initialize_paddles:
    ; 输出：CIA1 PA6/PA7 配置为端口选择输出，PB2..PB4 配置为按钮输入。
    ; OR 用来只置位目标 bit，AND 用来只清除目标 bit，其他 CIA 引脚保持原值。
    ; PA6/PA7 选择连接到 SID POT 输入的控制端口。
    SET_REGISTER_BITS CIA1_DDR_A, %11000000
    lda CIA1_PORT_A
    and #%00111111
    ora #PADDLE_PORT_1_SELECT
    sta CIA1_PORT_A

    ; Paddle 按钮和摇杆 FIRE 都是 PB2..PB4 上的低电平有效输入。
    CLEAR_REGISTER_BITS CIA1_DDR_B, %00011100
    rts

; EN: Read one SID POTX sample into potx_raw; test builds may override it.
read_paddle_x:
    ; 输出：potx_raw = SID_POTX 的 0..255 转换结果。
    ; SID 的 POT ADC 不是瞬时完成的；切换控制端口后用短循环等待读数稳定。
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
