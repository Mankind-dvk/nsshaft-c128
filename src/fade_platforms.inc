; ---------------------------------------------------------------------------
; 消失平台生命周期
; ---------------------------------------------------------------------------
; 未触发平台使用灰色 ECM 屏幕码。玩家第一次落上后，将该平台在两个屏幕
; 缓冲区中的完整/碎片屏幕码替换为独立的 60..62 号字形，并在红色和灰色
; 背景色之间切换。倒计时结束后只删除这些已激活字形，不会影响其他平台。
;
; 公共入口：initialize_fade_platforms、activate_fade_platform、
;   update_fade_platform、advance_fade_platform_scroll_position。
;
; 设计限制：同一时间只给一个已触发平台分配独立字形 60..62。第二个平台
; 被触发时，旧平台先删除；这样只用三个稀缺 ECM 字形槽就能实现闪烁效果。
;
; English summary:
;   Tracks one activated disappearing platform. Landing converts its FULL/UPPER/
;   LOWER cells in both buffers to private glyphs 60..62. A lifetime counter
;   alternates red/gray ECM prefixes, then removes only those private glyphs and
;   releases the player if that platform still supplies support.

.segment "EFFECTS"

; EN: Reset the single active fade lifecycle and player support marker.
initialize_fade_platforms:
    ; 新游戏重置全部计时和支撑标志；未触发的灰色平台无需登记。
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
; EN: Register collision_row, start timers, and convert both buffers to private glyphs.
activate_fade_platform:
    ; 输入：collision_row 指向刚踩到的平台锚点。
    ; 输出：记录行、装载寿命/闪烁计时，并把两屏中的未触发灰色码转换成
    ; 60..62 号“已激活”码。JMP 尾调用避免多占一个返回地址。
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
; EN: Tick lifetime/flashing; recolor or remove the active platform when due.
update_fade_platform:
    ; 每帧调用。寿命到 0 删除平台；flash_counter 到 0 时仅切换屏幕码的
    ; ECM 高两位，字形低六位保持 60..62，因此形状和滚动相位不变。
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
; EN: Move the saved anchor up after each completed eight-pixel coarse scroll.
advance_fade_platform_scroll_position:
    ; 每完成一次八像素粗滚动调用。屏幕内容已上移一行，所以保存的逻辑
    ; 锚点也必须减一，否则后续闪烁会修改错误的平台。
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
    sta fade_flash_counter
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
    ; 输入：fade_platform_row、fade_effect_mode、fade_color_prefix。
    ; 输出：同时修改 SCREEN_A/B。扫描 row-1,row,row+1 是因为转场期间一个
    ; 逻辑平台可能分成相邻两行，而两张屏幕还可能处于不同表示阶段。
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
; EN: Pure A-in/A-out mapping for activate, recolor, or remove mode.
transform_fade_character:
    ; 输入：A=一个屏幕码。输出：A=转换后的屏幕码。
    ; 这是纯“单字符映射”例程；外层负责地址和循环，因此激活、重着色、删除
    ; 可以共用同一遍历框架。
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
