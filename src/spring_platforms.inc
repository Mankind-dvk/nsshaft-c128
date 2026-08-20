; ---------------------------------------------------------------------------
; 弹簧平台压缩、恢复与发射
; ---------------------------------------------------------------------------
; 弹簧完整时使用“工”字形。人物落地后，把同一平台在两个屏幕缓冲区中的
; 完整/碎片屏幕码全部换成压扁版本；短暂停留后恢复图形并给予人物向上速度。
; 若人物在发射前走出平台，图形会恢复，但不会产生弹跳。
;
; 公共入口：initialize_spring_platforms、compress_spring_platform、
;   cancel_spring_platform、update_spring_platform、
;   advance_spring_platform_scroll_position、update_spring_fragment_glyphs。
;
; 与消失平台一样，弹簧效果必须同时识别 FULL/UPPER/LOWER 三种表示。区别是
; 弹簧不删除平台，只在 normal 与 compressed 两套屏幕码之间来回转换。

.segment "GAMEPLAY"

; 根据 video.inc 已算好的 fragment_split/fragment_count，同时重建普通和
; 压扁弹簧的上下滚动碎片。0 是黑色背景，复制进来的 1 位显示紫色弹簧。
update_spring_fragment_glyphs:
    ; 输入：fragment_split、fragment_count。输出：四个可写动态字形。
    ; 先清零保证未复制部分透明，再把源字形的头部放入 UPPER、尾部放入
    ; LOWER。普通和压扁两套形状在同一个循环中生成，节省循环开销。
    ldy #0
    lda #$00
@clear:
    sta spring_upper_glyph, y
    sta spring_lower_glyph, y
    sta spring_compressed_upper_glyph, y
    sta spring_compressed_lower_glyph, y
    iny
    cpy #8
    bne @clear

    ldx #0
    ldy fragment_split
@upper:
    lda spring_source_glyph, x
    sta spring_upper_glyph, y
    lda spring_compressed_source_glyph, x
    sta spring_compressed_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @upper

    ldx fragment_count
    ldy #0
@lower:
    lda spring_source_glyph, x
    sta spring_lower_glyph, y
    lda spring_compressed_source_glyph, x
    sta spring_compressed_lower_glyph, y
    inx
    iny
    cpx #8
    bne @lower
    rts

initialize_spring_platforms:
    ; 新游戏清除压缩倒计时和上升速度，避免继承上一局的弹跳状态。
    lda #0
    sta player_on_spring
    sta player_rise_velocity
    sta spring_platform_row
    sta spring_compression_timer
    rts

; collision_row 始终指向平台下部碎片/完整字符所在的逻辑锚点行。
compress_spring_platform:
    ; 输入：collision_row。若当前尚未压缩，记录锚点、装载 8 帧计时，并把
    ; 两张屏幕中该平台的三种 normal 屏幕码替换为 compressed 屏幕码。
    lda player_on_spring
    bne @done
    lda collision_row
    sta spring_platform_row
    lda #SPRING_COMPRESSION_FRAMES
    sta spring_compression_timer
    lda #1
    sta player_on_spring
    lda #SPRING_EFFECT_COMPRESS
    sta spring_effect_mode
    jmp apply_spring_effect_to_buffers
@done:
    rts

; 走出弹簧时只恢复外形，不发射人物。
cancel_spring_platform:
    ; 玩家在发射前横向走出平台时调用：恢复图形、清计时，但不赋予上升速度。
    lda player_on_spring
    beq @done
    lda #SPRING_EFFECT_RESTORE
    sta spring_effect_mode
    jsr apply_spring_effect_to_buffers
    lda #0
    sta player_on_spring
    sta spring_compression_timer
@done:
    rts

; 压缩倒计时结束后恢复“工”字形，并切换到独立的向上运动状态。
update_spring_platform:
    ; 每帧调用。倒计时归零时先恢复“工”字形，再清除支撑并设置
    ; player_rise_velocity；实际向上位移由下一帧 update_player_vertical 完成。
    lda player_on_spring
    beq @done
    dec spring_compression_timer
    bne @done

    lda #SPRING_EFFECT_RESTORE
    sta spring_effect_mode
    jsr apply_spring_effect_to_buffers

    lda #0
    sta player_on_spring
    sta player_on_platform
    sta player_on_fade_platform
    sta player_fall_fraction
    sta player_fall_velocity
    lda #SPRING_LAUNCH_VELOCITY
    sta player_rise_velocity
@done:
    rts

; 每次完成八像素粗滚动后，平台锚点向上一行。离开游戏区时无需再恢复，
; 因为 video.inc 已经把对应碎片从两个屏幕缓冲区中清除。
advance_spring_platform_scroll_position:
    ; 与消失平台相同，每次粗滚动把保存的锚点减一。若平台已越过顶框，屏幕
    ; 转场已经清除图形，这里只需丢弃生命周期状态。
    lda player_on_spring
    beq @done
    lda spring_platform_row
    cmp #(PLAYFIELD_TOP_ROW+1)
    bcs @move_up
    lda #0
    sta player_on_spring
    sta spring_compression_timer
    rts
@move_up:
    dec spring_platform_row
@done:
    rts

; 两个缓冲区可能一个保存完整字符，另一个保存上下碎片。因此围绕锚点
; 扫描三行，并只转换弹簧自己的六种屏幕码，不会影响附近其他平台。
apply_spring_effect_to_buffers:
    ; 输入：spring_platform_row、spring_effect_mode。输出：同步修改 A/B 两屏
    ; 锚点附近三行。紫色 Color RAM 不需要改，因为压缩前后都使用同一颜色。
    lda #<SCREEN_A
    sta ROW_PTR
    lda #>SCREEN_A
    sta ROW_PTR+1
    lda #<SCREEN_B
    sta DEST_PTR
    lda #>SCREEN_B
    sta DEST_PTR+1

    ldx spring_platform_row
    dex
@seek_first_row:
    cpx #0
    beq @pointers_ready
    jsr advance_copy_pointers
    dex
    bne @seek_first_row
@pointers_ready:
    lda #3
    sta spring_rows_remaining

@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    jsr transform_spring_character
    sta (ROW_PTR), y
    lda (DEST_PTR), y
    jsr transform_spring_character
    sta (DEST_PTR), y
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @column

    jsr advance_copy_pointers
    dec spring_rows_remaining
    bne @row
    rts

transform_spring_character:
    ; 输入/输出：A=屏幕码。mode=0 将 normal 映射为 compressed；mode=1
    ; 执行反向映射。非弹簧字符原样返回。
    sta spring_char_work
    lda spring_effect_mode
    bne @restore

@compress:
    lda spring_char_work
    cmp #CHAR_SPRING_FULL
    beq @compressed_full
    cmp #CHAR_SPRING_UPPER
    beq @compressed_upper
    cmp #CHAR_SPRING_LOWER
    bne @unchanged
    lda #CHAR_SPRING_COMPRESSED_LOWER
    rts
@compressed_full:
    lda #CHAR_SPRING_COMPRESSED_FULL
    rts
@compressed_upper:
    lda #CHAR_SPRING_COMPRESSED_UPPER
    rts

@restore:
    lda spring_char_work
    cmp #CHAR_SPRING_COMPRESSED_FULL
    beq @full
    cmp #CHAR_SPRING_COMPRESSED_UPPER
    beq @upper
    cmp #CHAR_SPRING_COMPRESSED_LOWER
    bne @unchanged
    lda #CHAR_SPRING_LOWER
    rts
@full:
    lda #CHAR_SPRING_FULL
    rts
@upper:
    lda #CHAR_SPRING_UPPER
    rts

@unchanged:
    lda spring_char_work
    rts

.assert * <= $3800, error, "gameplay code exceeds reserved memory"
