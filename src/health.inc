; ---------------------------------------------------------------------------
; HP 奖励、显示与伤害恢复
; ---------------------------------------------------------------------------
; 每获得三个“首次到达新平台”的分数增加 1 HP，分数不会被兑换或扣除。尖刺
; 接触也先计分，因此第三分若来自尖刺，新增加的 HP 可以立即吸收这次伤害。HP
; 表示可吸收的伤害次数：尖刺和顶框先尝试消耗 1 HP；HP 为 0 时才请求
; game over。掉到底部不经过本模块，仍由 player.inc 直接结束游戏。
;
; 伤害成功吸收后必须立刻离开危险接触面，否则下一帧会再次扣血。尖刺给予
; 一个向上反弹速度；顶框把 sprite 放回框内八像素并清除平台支撑，使人物
; 向下坠落。两种处理都清除旧的垂直/平台效果状态。
;
; English summary:
;   Every three newly claimed platforms award one persistent HP without reducing
;   score. A spike scores before damage, so a newly earned HP may absorb that
;   same contact. Spikes bounce the player; the top frame drops the player back
;   into the shaft after consuming HP. Zero HP turns either hazard into game over;
;   falling through the bottom remains immediately fatal.

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
    sta SCREEN_A+HP_DISPLAY_OFFSET
    sta SCREEN_B+HP_DISPLAY_OFFSET
    lda #CHAR_P
    sta SCREEN_A+HP_DISPLAY_OFFSET+1
    sta SCREEN_B+HP_DISPLAY_OFFSET+1
    lda #CHAR_COLON
    sta SCREEN_A+HP_DISPLAY_OFFSET+2
    sta SCREEN_B+HP_DISPLAY_OFFSET+2

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

; EN: Spend HP for top contact, detach from support, and drop inside the shaft.
handle_top_damage:
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
    lda #PLAYER_TOP_GAME_OVER_Y+TOP_DAMAGE_DROP_PIXELS
    sta player_y
    sta VIC_SPRITE_0_Y
@done:
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
    sta SCREEN_A+HP_DISPLAY_OFFSET+3
    sta SCREEN_B+HP_DISPLAY_OFFSET+3
    lda health_tens
    clc
    adc #CHAR_DIGIT_0
    sta SCREEN_A+HP_DISPLAY_OFFSET+4
    sta SCREEN_B+HP_DISPLAY_OFFSET+4
    lda health_ones
    clc
    adc #CHAR_DIGIT_0
    sta SCREEN_A+HP_DISPLAY_OFFSET+5
    sta SCREEN_B+HP_DISPLAY_OFFSET+5
    rts

.assert * <= PLAYER_HURT_ADDRESS, error, "HP code overlaps hurt sprite block"
