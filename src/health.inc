; ---------------------------------------------------------------------------
; HP 奖励、显示与伤害恢复
; ---------------------------------------------------------------------------
; 每获得三个“首次到达新平台”的分数增加 1 HP，分数不会被兑换或扣除。尖刺
; 接触也先计分，因此第三分若来自尖刺，新增加的 HP 可以立即吸收这次伤害。HP
; 表示可吸收的普通尖刺平台或顶部倒尖刺伤害次数；HP 为 0 时才请求 game over。
; 掉到底部不经过本模块，无论 HP 多少都直接结束游戏。顶部接触不计分。
;
; 伤害成功吸收后必须立刻离开危险接触面，否则下一帧会再次扣血。尖刺给予
; 一个向上反弹速度，顶部倒尖刺则将人物下移脱离；两者都清除旧的平台状态。
;
; English summary:
;   Every three newly claimed platforms award one persistent HP without reducing
;   score. A spike scores before damage, so a newly earned HP may absorb that
;   same contact. Both spike types consume HP: platform spikes bounce upward,
;   ceiling spikes detach and drop the player. Only falling bypasses HP.

.segment "EXTCODE"

; EN: Reset HP/progress and draw the centered HP:000 line in both screen buffers.
initialize_health:
    lda #0
    sta health_points
    sta health_score_progress
    sta health_decimal_work
    sta health_hundreds
    sta health_tens
    sta health_ones

    lda #CHAR_H
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET
    lda #CHAR_P
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+1
    lda #CHAR_COLON
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+2

    lda #COLOR_WHITE
    ldx #0
@color:
    sta COLOR_RAM+HP_DISPLAY_OFFSET, x
    inx
    cpx #3+HP_DIGIT_COUNT
    bne @color
    jmp update_health_display

; EN: Convert each third platform point into one HP without changing score.
award_health_for_score:
    inc health_score_progress
    lda health_score_progress
    cmp #SCORES_PER_HP
    bcc @done
    lda #0
    sta health_score_progress

    lda health_points
    cmp #$ff
    beq @done
    inc health_points
    jmp update_health_display
@done:
    rts

; EN: Consume one HP and return C=1, or request game over and return C=0.
consume_health_or_game_over:
    lda health_points
    beq @fatal
    dec health_points
    lda #PLAYER_HURT_FRAMES
    sta player_hurt_timer
    jsr update_health_display
    sec
    rts
@fatal:
    jsr request_game_over
    clc
    rts

; EN: Spend HP for spike contact and bounce upward to prevent repeat damage.
handle_spike_damage:
    jsr consume_health_or_game_over
    bcc @done
    jsr cancel_spring_platform
    lda #0
    sta player_on_platform
    sta player_on_fade_platform
    sta player_on_spring
    sta player_fall_fraction
    sta player_fall_velocity
    sta conveyor_move_counter
    lda #SPIKE_DAMAGE_BOUNCE_VELOCITY
    sta player_rise_velocity
@done:
    rts

; EN: Convert binary HP 0..255 to three decimal character cells in both buffers.
update_health_display:
    lda health_points
    sta health_decimal_work
    ldx #0
@hundreds:
    lda health_decimal_work
    cmp #100
    bcc @hundreds_done
    sec
    sbc #100
    sta health_decimal_work
    inx
    bne @hundreds
@hundreds_done:
    stx health_hundreds

    ldx #0
@tens:
    lda health_decimal_work
    cmp #10
    bcc @tens_done
    sec
    sbc #10
    sta health_decimal_work
    inx
    bne @tens
@tens_done:
    stx health_tens
    lda health_decimal_work
    sta health_ones

    lda health_hundreds
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+3
    lda health_tens
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+4
    lda health_ones
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+5
    rts

.assert * <= PLAYER_HURT_ADDRESS, error, "HP code overlaps hurt sprite block"

; EXTCODE 紧邻受伤 sprite，顶部恢复放在尚有空间的 GAMEPLAY 段；调用方式不变。
; EN: Keep ceiling recovery in GAMEPLAY to avoid overflowing EXTCODE into sprites.
.segment "GAMEPLAY"

; 头顶碰到尖端后只扣一次血：取消压缩弹簧、解除支撑、终止上升，再下移 8 像素。
; 不能只改 Y 而保留支撑/弹簧状态，否则下帧又会被托举或发射回尖刺。
; EN: Absorb one ceiling hit, cancel spring/support/momentum, and drop below the tips.
handle_ceiling_damage:
    jsr consume_health_or_game_over
    bcc @done
    jsr cancel_spring_platform
    lda #0
    sta player_on_platform
    sta player_on_fade_platform
    sta player_on_spring
    sta player_fall_fraction
    sta player_fall_velocity
    sta player_rise_velocity
    sta conveyor_move_counter
    lda #(PLAYER_CEILING_CONTACT_Y+CEILING_DAMAGE_DROP_PIXELS)
    sta player_y
    sta VIC_SPRITE_0_Y
@done:
    rts
