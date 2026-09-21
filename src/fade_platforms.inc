; ---------------------------------------------------------------------------
; 消失平台生命周期
; ---------------------------------------------------------------------------
; 完整砖纹用 56/57/58；踩下后切为部分脱落的 7/16/17，25 帧后切为只剩
; 砖块的 59/60/61，50 帧后消失。Color RAM 始终为 10；10=灰、11=红、00=空。
; 删除只匹配已激活字形，不影响普通砖台阶或其他未触发平台。
;
; 公共入口：initialize_fade_platforms、activate_fade_platform、
;   update_fade_platform、advance_fade_platform_scroll_position。
;
; 设计限制：同一时间只维护一个已触发平台。第二个平台被触发时，旧平台先删除。
;
; English summary:
;   Tracks one activated disappearing platform. Landing converts its FULL/UPPER/
;   LOWER cells in both buffers to cracked tiles, then broken tiles at half-life.
;   The 50-frame lifetime removes only those private glyphs and
;   releases the player if that platform still supplies support.

.segment "EFFECTS"

; EN: Reset the single active fade lifecycle and player support marker.
initialize_fade_platforms:
    ; 新游戏重置计时、破碎阶段和支撑标志；未触发平台无需登记。
    lda #0
    sta fade_platform_active
    sta fade_platform_row
    sta fade_platform_timer
    sta player_on_fade_platform
    lda #FADE_STAGE_CRACKED
    sta fade_stage
    rts

; collision_row 是平滑滚动中平台下部碎片所在的逻辑锚点行。平台完整时也
; 使用同一行，因此该值可以跨过后续像素相位；每次粗滚动再减一。
; EN: Register collision_row, start timers, and convert both buffers to private glyphs.
activate_fade_platform:
    ; 输入：collision_row 指向刚踩到的平台锚点。
    ; 输出：记录行与寿命，把两屏中的完整砖纹转换成第二级破碎字形。
    lda fade_platform_active
    beq @start
    ; 同一时间只维护一个独立倒计时。若玩家快速落到另一个消失平台，先让
    ; 旧平台消失，再为新平台建立完整的新倒计时。
    jsr remove_active_fade_platform
@start:
    lda collision_row
    sta fade_platform_row
    lda #FADE_PLATFORM_LIFETIME
    sta fade_platform_timer
    lda #FADE_STAGE_CRACKED
    sta fade_stage
    lda #1
    sta fade_platform_active

    lda #FADE_EFFECT_ACTIVATE
    sta fade_effect_mode
    jmp apply_fade_effect_to_buffers

; 每帧推进倒计时；剩余半段寿命时进入第三级，归零时删除。
; EN: Advance once from cracked to broken, then remove the expired platform.
update_fade_platform:
    ; 两组碎片保持同一滚动相位；破碎仅改变外观，删除前仍提供支撑。
    lda fade_platform_active
    beq @done

    dec fade_platform_timer
    beq remove_active_fade_platform

    lda fade_platform_timer
    cmp #FADE_PLATFORM_BROKEN_TIME
    bne @done
    lda #FADE_STAGE_BROKEN
    sta fade_stage
    lda #FADE_EFFECT_ADVANCE
    sta fade_effect_mode
    jmp apply_fade_effect_to_buffers
@done:
    rts

; 粗滚动把完整平台移到上一字符行，因此效果扫描的锚点也必须同步上移。
; 平台从第 2 行离开游戏区时，转场代码已经清除了它，不再继续倒计时。
; EN: Move the saved anchor up after each completed eight-pixel coarse scroll.
advance_fade_platform_scroll_position:
    ; 每完成一次八像素粗滚动调用。屏幕内容已上移一行，所以保存的逻辑
    ; 锚点也必须减一，使阶段推进和删除继续匹配同一平台。
    lda fade_platform_active
    beq @done
    lda fade_platform_row
    cmp #(PLAYFIELD_TOP_ROW+1)
    bcs @move_up

    lda #0
    sta fade_platform_active
    jsr release_player_from_fade_platform
    rts
@move_up:
    dec fade_platform_row
@done:
    rts

; EN: Delete private fade glyphs from both buffers and release supported player.
remove_active_fade_platform:
    ; 把两个缓冲区中属于当前激活平台的三种字形全部变为空白，并在玩家仍
    ; 受它支撑时解除支撑，使下一帧进入重力下落。
    lda fade_platform_active
    beq @done
    lda #FADE_EFFECT_REMOVE
    sta fade_effect_mode
    jsr apply_fade_effect_to_buffers

    lda #0
    sta fade_platform_active
    sta fade_platform_timer
    jsr release_player_from_fade_platform
@done:
    rts

; 只有人物当前仍由正在消失的平台支撑时，删除平台才会主动开始下落。
; 若人物已经走开并落到普通平台，旧平台的倒计时不会干扰新的支撑状态。
; EN: Clear support only when the player still stands on the disappearing platform.
release_player_from_fade_platform:
    lda player_on_fade_platform
    beq @done
    lda #0
    sta player_on_fade_platform
    sta player_on_platform
    sta player_fall_fraction
    sta player_fall_velocity
@done:
    rts

; 两个缓冲区可能分别保留完整平台和上下碎片，所以围绕锚点扫描三行。
; 平台间距至少四行，三行窗口不会碰到另一个未触发的消失平台。
; EN: Scan three rows around the anchor and transform matching cells in both buffers.
apply_fade_effect_to_buffers:
    ; 输入：fade_platform_row、fade_effect_mode、fade_stage。
    ; 输出：同时修改 SCREEN_A/B。扫描 row-1,row,row+1 是因为转场期间一个
    ; 逻辑平台可能分成相邻两行，而两张屏幕还可能处于不同表示阶段。
    jsr update_fade_row_description
    SET_WORD ROW_PTR, SCREEN_A
    SET_WORD DEST_PTR, SCREEN_B

    ldx fade_platform_row
    dex
@seek_first_row:
    cpx #0
    beq @pointers_ready
    jsr advance_copy_pointers
    dex
    bne @seek_first_row
@pointers_ready:
    lda #3
    sta fade_rows_remaining

@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    jsr transform_fade_character
    sta (ROW_PTR), y

    lda (DEST_PTR), y
    jsr transform_fade_character
    sta (DEST_PTR), y

    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @column

    jsr advance_copy_pointers
    dec fade_rows_remaining
    bne @row
    rts

; 输入和输出均为 A 中的屏幕码。激活只转换完整平台；阶段推进和删除只认
; 两组已激活字形，不对编号进行位掩码或范围猜测。
; EN: Pure A-in/A-out mapping for activate, advance-stage, or remove mode.
transform_fade_character:
    ; 输入：A=一个屏幕码。输出：A=转换后的屏幕码。
    ; 这是纯“单字符映射”例程；外层负责地址和循环，因此激活、换阶段、删除
    ; 可以共用同一遍历框架。
    sta fade_char_work
    ldx fade_effect_mode
    beq @activate

    ; EN: X identifies FULL/UPPER/LOWER across both exact active-code sets.
    ldx #2
@find_active:
    cmp fade_cracked_chars, x
    beq @active
    cmp fade_broken_chars, x
    beq @active
    dex
    bpl @find_active
    jmp @unchanged
@active:
    lda fade_effect_mode
    cmp #FADE_EFFECT_ADVANCE
    beq @select_stage
    lda #CHAR_BLANK
    rts

@select_stage:
    lda fade_stage
    beq @cracked
    lda fade_broken_chars, x
    rts
@cracked:
    lda fade_cracked_chars, x
    rts

@activate:
    lda fade_char_work
    ldx #2
@find_inactive:
    cmp fade_inactive_chars, x
    beq @select_stage
    dex
    bpl @find_inactive

@unchanged:
    lda fade_char_work
    rts

fade_inactive_chars:
    .byte CHAR_FADE_FULL, CHAR_FADE_UPPER, CHAR_FADE_LOWER
fade_cracked_chars:
    .byte CHAR_FADE_CRACKED_FULL, CHAR_FADE_CRACKED_UPPER, CHAR_FADE_CRACKED_LOWER
fade_broken_chars:
    .byte CHAR_FADE_BROKEN_FULL, CHAR_FADE_BROKEN_UPPER, CHAR_FADE_BROKEN_LOWER

.assert * <= $3000, error, "fade-platform effects overlap sprite memory"
