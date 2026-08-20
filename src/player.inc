; ---------------------------------------------------------------------------
; 玩家控制、重力、着陆与碰撞
; ---------------------------------------------------------------------------
; 本模块负责 sprite 0，以及玩家和碰撞相关的全部状态转换。水平输入使用
; 共用的 POTX 死区。垂直碰撞在 24 像素宽的 sprite 下方采样两个点，并
; 分别处理普通平台与尖刺平台。
;
; 公共入口：initialize_player、update_player_horizontal、
;   update_player_vertical、update_player_sprite_frame。

.segment "CODE"

; 玩家 sprite ---------------------------------------------------------------

initialize_player:
    lda #PLAYER_NORMAL_BLOCK
    sta SPRITE_POINTER_A
    sta SPRITE_POINTER_B

    lda #0
    sta VIC_SPRITE_MULTICOLOR
    sta VIC_SPRITE_X_EXPAND
    sta VIC_SPRITE_Y_EXPAND
    sta VIC_SPRITE_PRIORITY
    sta player_move_counter

    ; Spritemate 工程中的四帧均为单色紫色 sprite。位图只描述透明/实心
    ; 像素，真正的显示颜色仍由 sprite 0 的颜色寄存器决定。
    lda #COLOR_PURPLE
    sta VIC_SPRITE_0_COLOR

    lda #22
    sta spawn_row
    jsr position_player_on_platform

    ; POTX 已改为字符 HUD，只启用玩家 sprite 0。
    lda #%00000001
    sta VIC_SPRITE_ENABLE
    rts

; 根据最终的逐帧物理状态选择 sprite 0 的一帧。下落优先于水平输入；
; 站在平台上时，paddle 死区显示 normal，死区两侧显示 left/right。
; 两张屏幕的指针表必须同步更新，否则 D018 双缓冲翻页时人物会闪回旧帧。
update_player_sprite_frame:
    lda player_on_platform
    beq @fall

    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcs @right
    lda #PLAYER_NORMAL_BLOCK
    bne @store

@fall:
    lda #PLAYER_FALL_BLOCK
    bne @store
@left:
    lda #PLAYER_LEFT_BLOCK
    bne @store
@right:
    lda #PLAYER_RIGHT_BLOCK
@store:
    sta SPRITE_POINTER_A
    sta SPRITE_POINTER_B
    rts

; 将玩家放置到 spawn_row 中由 platform_start/platform_width 指定的平台上。
position_player_on_platform:
    ; 水平居中 24 像素宽的 sprite：
    ; x = 24 + start*8 + (width*8 - 24)/2
    ;   = 12 + start*8 + width*4。
    lda platform_start
    asl
    asl
    asl
    clc
    adc #12
    sta player_x_low
    lda #0
    adc #0
    sta player_x_high

    lda platform_width
    asl
    asl
    clc
    adc player_x_low
    sta player_x_low
    bcc @x_ready
    inc player_x_high
@x_ready:
    jsr write_player_x

    ; 平台顶部 = SCREEN_ROW0_BASE_Y + fine_scroll + row*8。
    lda spawn_row
    asl
    asl
    asl
    clc
    adc fine_scroll
    adc #SCREEN_ROW0_BASE_Y-PLAYER_HEIGHT
    sta player_y
    sta VIC_SPRITE_0_Y

    lda #0
    sta player_fall_fraction
    sta player_fall_velocity
    sta player_on_fade_platform
    lda #1
    sta player_on_platform
    rts

update_player_horizontal:
    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcs @right

    ; 108..148 为不移动的中立死区。
    lda #0
    sta player_move_counter
    rts

@left:
    jsr player_movement_ready
    bcc @done

@move_left:
    ; 在减法前判断边界，避免 X=0/1 等异常状态产生 16 位下溢并把 sprite
    ; 瞬间送到画面右侧。正常移动到最后一步时直接钳制到真实左边缘。
    lda player_x_high
    bne @subtract_left
    lda player_x_low
    cmp #PLAYER_MIN_X+PLAYER_MOVE_STEP
    bcs @subtract_left
@clamp_left:
    lda #PLAYER_MIN_X
    sta player_x_low
    lda #0
    sta player_x_high
    jmp write_player_x

@subtract_left:
    sec
    lda player_x_low
    sbc #PLAYER_MOVE_STEP
    sta player_x_low
    lda player_x_high
    sbc #0
    sta player_x_high
@write_left:
    jmp write_player_x

@right:
    jsr player_movement_ready
    bcc @done

@move_right:
    clc
    lda player_x_low
    adc #PLAYER_MOVE_STEP
    sta player_x_low
    lda player_x_high
    adc #0
    sta player_x_high
    cmp #PLAYER_MAX_X_HIGH
    bcc @write_right
    bne @clamp_right
    lda player_x_low
    cmp #PLAYER_MAX_X_LOW+1
    bcc @write_right
@clamp_right:
    lda #PLAYER_MAX_X_LOW
    sta player_x_low
    lda #PLAYER_MAX_X_HIGH
    sta player_x_high
@write_right:
    jmp write_player_x

@done:
    rts

; NS-Shaft 垂直运动 ---------------------------------------------------------

update_player_vertical:
    lda player_on_platform
    beq @falling

    lda player_y
    ; 顶端 Y 小于或等于顶框上沿时立即结束。比较阈值加一，可以在无符号
    ; 比较中把“相等”也归入越界，而不会晚一个像素。
    cmp #(PLAYER_TOP_GAME_OVER_Y+1)
    bcs @supported_in_bounds
    jmp request_game_over
@supported_in_bounds:

    ; 走出当前支撑平台的范围后开始下落。
    jsr find_platform_below_player
    cmp #1
    beq @supported_normal
    cmp #2
    beq @hit_spikes
    cmp #3
    beq @activate_supported_fade
    cmp #4
    beq @supported_fade
    jmp @begin_fall
@hit_spikes:
    jmp request_game_over
@supported_normal:
    lda #0
    sta player_on_fade_platform
    rts
@activate_supported_fade:
    jsr activate_fade_platform
@supported_fade:
    lda #1
    sta player_on_fade_platform
    rts
@begin_fall:
    lda #0
    sta player_on_platform
    sta player_on_fade_platform
    sta player_fall_fraction
    sta player_fall_velocity
    rts

@falling:
    lda player_y
    cmp #PLAYER_BOTTOM_GAME_OVER_Y
    bcc @falling_in_bounds
    jmp request_game_over
@falling_in_bounds:

    lda player_fall_velocity
    cmp #GRAVITY_MAX
    bcs @velocity_ready
    inc player_fall_velocity
@velocity_ready:
    lda player_fall_fraction
    clc
    adc player_fall_velocity
    cmp #8
    bcc @store_fraction
    sbc #8
    sta player_fall_fraction
    inc player_y
    lda player_y
    sta VIC_SPRITE_0_Y

    ; 本帧重力可能刚好让 sprite 底端接触底框；必须在同一帧检查，而不是
    ; 留到下一次 update_player_vertical 才结束。
    cmp #PLAYER_BOTTOM_GAME_OVER_Y
    bcc @check_landing
    jmp request_game_over

@check_landing:
    jsr find_platform_below_player
    cmp #1
    beq @land
    cmp #2
    beq @fall_on_spikes
    cmp #3
    beq @land
    cmp #4
    beq @land
    jmp @fall_done
@fall_on_spikes:
    jmp request_game_over
@fall_done:
    rts

@store_fraction:
    sta player_fall_fraction
@done:
    rts

@land:
    lda collision_row
    sta spawn_row

    ; 保持 X 不变，将 sprite 脚底吸附到平台顶部。
    lda spawn_row
    asl
    asl
    asl
    clc
    adc fine_scroll
    adc #SCREEN_ROW0_BASE_Y-PLAYER_HEIGHT
    sta player_y
    sta VIC_SPRITE_0_Y

    lda #0
    sta player_fall_fraction
    sta player_fall_velocity

    lda collision_type
    cmp #3
    beq @activate_landed_fade
    cmp #4
    beq @mark_landed_fade
    lda #0
    sta player_on_fade_platform
    beq @finish_landing
@activate_landed_fade:
    ; 若此前还有另一个正在倒计时的平台，激活流程会先将旧平台删除。
    lda #0
    sta player_on_fade_platform
    jsr activate_fade_platform
@mark_landed_fade:
    lda #1
    sta player_on_fade_platform
@finish_landing:
    lda #1
    sta player_on_platform
    rts

request_game_over:
    lda #1
    sta game_over_flag
    rts

; 返回 A=0 空白、1 普通、2 尖刺、3 未触发消失平台、4 已触发消失平台。
; 探测点位于 sprite 脚底，并允许两像素的跨越检测窗口。
;
; 平滑滚动期间，一个逻辑平台会同时显示在相邻的两个字符格中：LOWER
; 保留平台原来的逻辑行，UPPER 只绘制越过字符边界的像素。碰撞必须始终
; 使用 LOWER 所在行作为锚点；如果把 UPPER 也视作平台，小人会先在上一
; 字符行落地，视觉上比真实平台顶部高 8 像素。
find_platform_below_player:
    lda player_y
    clc
    adc #PLAYER_HEIGHT
    sta collision_probe_y

    lda fine_scroll
    clc
    adc #SCREEN_ROW0_BASE_Y
    sta collision_screen_top

    lda collision_probe_y
    cmp collision_screen_top
    bcc @none
    sec
    sbc collision_screen_top
    sta collision_relative_y
    and #%00000111
    cmp #3
    bcs @none

    lda collision_relative_y
    lsr
    lsr
    lsr
    cmp #SCREEN_ROWS
    bcs @none
    sta collision_row
    jsr set_collision_row_pointer

    lda #5
    jsr sample_platform_at_player_offset
    sta collision_type
    lda #18
    jsr sample_platform_at_player_offset
    cmp collision_type
    bcc @return_saved
    sta collision_type
@return_saved:
    lda collision_type
    rts

@none:
    lda #0
    rts

set_collision_row_pointer:
    lda active_screen
    beq @screen_a
    lda #<SCREEN_B
    sta ROW_PTR
    lda #>SCREEN_B
    bne @base_ready
@screen_a:
    lda #<SCREEN_A
    sta ROW_PTR
    lda #>SCREEN_A
@base_ready:
    sta ROW_PTR+1

    ldx collision_row
    beq @done
@advance:
    jsr advance_row_pointer
    dex
    bne @advance
@done:
    rts

; 输入 A 为 24 像素宽 sprite 内部的 X 偏移。
sample_platform_at_player_offset:
    clc
    adc player_x_low
    sta collision_x_low
    lda player_x_high
    adc #0
    sta collision_x_high

    sec
    lda collision_x_low
    sbc #24
    sta collision_x_low
    lda collision_x_high
    sbc #0
    sta collision_x_high

    lsr collision_x_high
    ror collision_x_low
    lsr collision_x_high
    ror collision_x_low
    lsr collision_x_high
    ror collision_x_low

    ldy collision_x_low
    lda (ROW_PTR), y
    cmp #CHAR_PLATFORM_FULL
    beq @normal
    cmp #CHAR_NORMAL_LOWER
    beq @normal
    cmp #CHAR_SPIKE_TRIANGLE
    beq @spikes
    cmp #CHAR_SPIKE_LOWER
    beq @spikes
    cmp #CHAR_FADE_FULL
    beq @fade_inactive
    cmp #CHAR_FADE_LOWER
    beq @fade_inactive
    cmp #CHAR_FADE_ACTIVE_RED_FULL
    beq @fade_active
    cmp #CHAR_FADE_ACTIVE_RED_LOWER
    beq @fade_active
    cmp #CHAR_FADE_ACTIVE_GRAY_FULL
    beq @fade_active
    cmp #CHAR_FADE_ACTIVE_GRAY_LOWER
    beq @fade_active
    lda #0
    rts
@normal:
    lda #1
    rts
@spikes:
    lda #2
    rts
@fade_inactive:
    lda #3
    rts
@fade_active:
    lda #4
    rts

; 查找画面中最低的普通平台并将玩家放到上面。玩家碰到尖刺或离开竖井后
; 使用此流程重新定位。
player_movement_ready:
    lda player_move_counter
    beq @ready
    dec player_move_counter
    clc
    rts
@ready:
    lda #PLAYER_MOVE_DELAY
    sta player_move_counter
    sec
    rts

write_player_x:
    lda player_x_low
    sta VIC_SPRITE_0_X

    lda VIC_SPRITE_X_HIGH
    and #%11111110
    ldx player_x_high
    beq @store_high
    ora #%00000001
@store_high:
    sta VIC_SPRITE_X_HIGH
    rts
