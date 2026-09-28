; ---------------------------------------------------------------------------
; 固定状态栏 HUD
; ---------------------------------------------------------------------------
; POTX 数字使用固定字符。右下角由 sprite 4 复用当前角色的五档方向帧，
; 根据 paddle 所在方向显示；sprite 1..3、5..7 保持空闲。
;
; 公共入口：
;   initialize_hud、update_potx_display、initialize_dashboard、
;   update_dashboard_direction。
;
; HUD 同时写 SCREEN_A 和 SCREEN_B，因为 $d018 随滚动翻页。Color RAM 只有
; 一份，不需要双写。sprite 4 直接复用角色现有位图，只需同步两张指针表。
;
; English summary:
;   Renders POTX:xxx into both screen buffers. Sprite 4 reuses the selected
;   character's existing directional frames as a five-level speed indicator.

.segment "CODE"

; POTX 数值显示 -------------------------------------------------------------

; EN: Write the fixed POTX:000 label to both screen matrices.
initialize_hud:
    ; 输出：两张屏幕右上角都有 POTX:000，八个字符的颜色统一为白色。
    ; 状态栏内部恰好有八列，直接写入 "POTX:000"。两个屏幕缓冲区都
    ; 必须更新，否则平台过渡时切换 D018 会显示旧数字。
    ldx #0
@label:
    lda potx_label, x
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET, x
    inx
    cpx #5
    bne @label

    ldx #0
    lda #COLOR_WHITE
@color:
    sta COLOR_RAM+POTX_DISPLAY_OFFSET, x
    inx
    cpx #8
    bne @color
    rts

; EN: Convert potx_raw to three decimal digits and update both buffers.
update_potx_display:
    ; 输入：potx_raw（0..255）。输出：右上角三个十进制数字。
    ; 6502 没有除法指令，因此先循环减 100 得百位，再循环减 10 得十位；
    ; 剩余值就是个位。对最多 255 的输入，这比通用除法例程更小也足够快。
    lda potx_raw
    sta decimal_work
    ldx #0

@hundreds:
    lda decimal_work
    cmp #100
    bcc @hundreds_done
    sec
    sbc #100
    sta decimal_work
    inx
    bne @hundreds

@hundreds_done:
    txa
    sta potx_hundreds

    ldx #0
@tens:
    lda decimal_work
    cmp #10
    bcc @tens_done
    sec
    sbc #10
    sta decimal_work
    inx
    bne @tens

@tens_done:
    txa
    sta potx_tens

    lda decimal_work
    sta potx_ones

    ; 游戏数字字形从第二源库抽取；不属于平台碎片区，因此这些字符
    ; 始终固定在右上角。
    lda potx_hundreds
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET+5

    lda potx_tens
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET+6

    lda potx_ones
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET+7
    rts

; 角色方向提示 --------------------------------------------------------------

; EN: Show the selected character's direction frame in the fixed status panel.
initialize_dashboard:
    ; sprite 4 使用现有角色帧，固定在右下角。X 第九位只改 bit 4，保留玩家 bit 0。
    lda #HUD_CHARACTER_X_LOW
    sta VIC_SPRITE_4_X
    lda #HUD_CHARACTER_Y
    sta VIC_SPRITE_4_Y

    ldy selected_character
    lda character_color_table, y
    sta VIC_SPRITE_4_COLOR

    SET_REGISTER_BITS VIC_SPRITE_X_HIGH, %00010000
    CLEAR_REGISTER_BITS VIC_SPRITE_X_EXPAND, %00010000
    CLEAR_REGISTER_BITS VIC_SPRITE_Y_EXPAND, %00010000
    jsr update_dashboard_direction

    SET_REGISTER_BITS VIC_SPRITE_ENABLE, %00010000
    rts

; 与人物控制共用五档方向选帧；只切换 sprite 块号。
; 与 sprite 0 同步写两张屏幕的指针，避免 D018 翻页时方向闪回旧帧。
update_dashboard_direction:
    jsr select_direction_sprite_block
    STORE_SPRITE_POINTER 4
    rts

; 输入 potx_raw，返回所选角色对应的 sprite 块号 A；Y 会改为角色索引。
; 边界与 player_movement_ready 的最快档一致，HUD 和角色不会显示不同档。
select_direction_sprite_block:
    ldy selected_character
    lda potx_raw
    cmp #POTX_FAR_LEFT_END
    bcc @fastleft
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcc @normal
    cmp #POTX_FAR_RIGHT_START
    bcc @right
    lda character_fastright_blocks, y
    rts
@fastleft:
    lda character_fastleft_blocks, y
    rts
@left:
    lda character_left_blocks, y
    rts
@normal:
    lda character_normal_blocks, y
    rts
@right:
    lda character_right_blocks, y
    rts
