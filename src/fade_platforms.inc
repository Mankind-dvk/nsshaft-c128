; ---------------------------------------------------------------------------
; 消失平台生命周期
; ---------------------------------------------------------------------------
; 未触发平台使用灰色 ECM 屏幕码。玩家第一次落上后，将该平台在两个屏幕
; 缓冲区中的完整/碎片屏幕码替换为独立的 60..62 号字形，并在红色和灰色
; 背景色之间切换。倒计时结束后只删除这些已激活字形，不会影响其他平台。
;
; 公共入口：initialize_fade_platforms、activate_fade_platform、
;   update_fade_platform、advance_fade_platform_scroll_position。

.segment "EFFECTS"

initialize_fade_platforms:
    lda #0
    sta fade_platform_active
    sta fade_platform_row
    sta fade_platform_timer
    sta fade_flash_counter
    sta player_on_fade_platform
    lda #FADE_COLOR_RED_PREFIX
    sta fade_color_prefix
    rts

; collision_row 是平滑滚动中平台下部碎片所在的逻辑锚点行。平台完整时也
; 使用同一行，因此该值可以跨过后续像素相位；每次粗滚动再减一。
activate_fade_platform:
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
    lda #FADE_PLATFORM_FLASH_PERIOD
    sta fade_flash_counter
    lda #FADE_COLOR_RED_PREFIX
    sta fade_color_prefix
    lda #1
    sta fade_platform_active

    lda #FADE_EFFECT_ACTIVATE
    sta fade_effect_mode
    jmp apply_fade_effect_to_buffers

; 每帧推进倒计时。激活时立即变红，随后每隔固定帧数在红/灰之间切换。
update_fade_platform:
    lda fade_platform_active
    beq @done

    dec fade_platform_timer
    beq remove_active_fade_platform

    dec fade_flash_counter
    bne @done
    lda #FADE_PLATFORM_FLASH_PERIOD
    sta fade_flash_counter

    lda fade_color_prefix
    eor #%01000000
    sta fade_color_prefix
    lda #FADE_EFFECT_RECOLOR
    sta fade_effect_mode
    jmp apply_fade_effect_to_buffers
@done:
    rts

; 粗滚动把完整平台移到上一字符行，因此效果扫描的锚点也必须同步上移。
; 平台从第 2 行离开游戏区时，转场代码已经清除了它，不再继续倒计时。
advance_fade_platform_scroll_position:
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

remove_active_fade_platform:
    lda fade_platform_active
    beq @done
    lda #FADE_EFFECT_REMOVE
    sta fade_effect_mode
    jsr apply_fade_effect_to_buffers

    lda #0
    sta fade_platform_active
    sta fade_platform_timer
    sta fade_flash_counter
    jsr release_player_from_fade_platform
@done:
    rts

; 只有人物当前仍由正在消失的平台支撑时，删除平台才会主动开始下落。
; 若人物已经走开并落到普通平台，旧平台的倒计时不会干扰新的支撑状态。
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
apply_fade_effect_to_buffers:
    lda #<SCREEN_A
    sta ROW_PTR
    lda #>SCREEN_A
    sta ROW_PTR+1
    lda #<SCREEN_B
    sta DEST_PTR
    lda #>SCREEN_B
    sta DEST_PTR+1

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

; 输入和输出均为 A 中的屏幕码。激活只转换未触发灰色平台；重新着色和
; 删除只认低六位为 60..62 的已触发平台字形。
transform_fade_character:
    sta fade_char_work
    ldx fade_effect_mode
    beq @activate
    dex
    beq @recolor

@remove:
    and #%00111111
    cmp #FADE_ACTIVE_GLYPH_FIRST
    bcc @unchanged
    cmp #(FADE_ACTIVE_GLYPH_LAST+1)
    bcs @unchanged
    lda #CHAR_BLANK
    rts

@recolor:
    and #%00111111
    cmp #FADE_ACTIVE_GLYPH_FIRST
    bcc @unchanged
    cmp #(FADE_ACTIVE_GLYPH_LAST+1)
    bcs @unchanged
    ora fade_color_prefix
    rts

@activate:
    lda fade_char_work
    cmp #CHAR_FADE_FULL
    beq @activate_full
    cmp #CHAR_FADE_UPPER
    beq @activate_upper
    cmp #CHAR_FADE_LOWER
    bne @unchanged
    lda #FADE_ACTIVE_GLYPH_LAST
    bne @apply_active_color
@activate_full:
    lda #FADE_ACTIVE_GLYPH_FIRST
    jmp @apply_active_color
@activate_upper:
    lda #FADE_ACTIVE_GLYPH_FIRST+1
@apply_active_color:
    ora fade_color_prefix
    rts

@unchanged:
    lda fade_char_work
    rts

.assert * <= $3000, error, "fade-platform effects overlap sprite memory"
