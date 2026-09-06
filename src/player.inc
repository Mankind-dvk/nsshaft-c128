; ---------------------------------------------------------------------------
; 玩家控制、重力、着陆与碰撞
; ---------------------------------------------------------------------------
; 本模块负责 sprite 0，以及玩家和碰撞相关的全部状态转换。水平输入使用
; 共用的 POTX 死区。垂直碰撞在 24 像素宽的 sprite 下方采样两个点，并
; 分别处理普通、尖刺、消失、弹簧和左右传送平台。
;
; 公共入口：initialize_player、update_player_horizontal、
;   update_player_vertical、update_player_sprite_frame。
;
; 坐标约定：player_x_low/high 是 VIC 的 9 位绝对 sprite X；player_y 是
; 21 像素高 sprite 的顶边。垂直速度使用 1/8 像素定点数。平台为单向支撑：
; 上升时不检测头顶平台，只有下落脚底接近平台顶部时才允许着陆。
;
; English summary:
;   Owns sprite 0, 9-bit X coordinates, 1/8-pixel vertical physics, landing, and
;   platform behavior dispatch. Platforms support only from above. Feet are
;   sampled at two X offsets; collision results 1..7 identify normal, spikes,
;   inactive/active fade, spring, and left/right conveyor support. Conveyor and
;   paddle counters are independent, so their average horizontal speeds add.

.segment "GAMEPLAY"

; 玩家 sprite ---------------------------------------------------------------

; EN: Configure sprite 0 and center it on the forced-safe spawn platform.
initialize_player:
    ; 输入：seed_platforms 留下的最低平台 start/width，fine_scroll=7。
    ; 输出：配置硬件 sprite 0，并把玩家水平居中放到第 22 行出生平台。
    ldy selected_character
    lda character_normal_blocks, y
    STORE_SPRITE_POINTER 0

    lda #0
    sta VIC_SPRITE_MULTICOLOR
    sta VIC_SPRITE_X_EXPAND
    sta VIC_SPRITE_Y_EXPAND
    sta VIC_SPRITE_PRIORITY
    sta player_move_counter
    sta player_hurt_timer
    sta conveyor_move_counter

    ; Spritemate 位图只描述透明/实心像素，实际颜色由 sprite 0 寄存器决定。
    lda character_color_table, y
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
; EN: Select hurt/fall/normal/left/right bitmap and mirror both pointer tables.
update_player_sprite_frame:
    ; 输入：player_on_platform 与 potx_raw。输出：同步修改 A/B 两张屏幕的
    ; sprite 0 指针。hurt_timer 非零时受伤帧拥有最高优先级；这里只换位图
    ; 块号，不复制 63 字节，所以每帧成本很低。
    ldy selected_character
    lda player_hurt_timer
    beq @select_motion
    dec player_hurt_timer
    lda character_hurt_blocks, y
    bne @store

@select_motion:
    lda player_on_platform
    beq @fall

    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcs @right
    lda character_normal_blocks, y
    bne @store

@fall:
    lda character_fall_blocks, y
    bne @store
@left:
    lda character_left_blocks, y
    bne @store
@right:
    lda character_right_blocks, y
@store:
    STORE_SPRITE_POINTER 0
    rts

; 将玩家放置到 spawn_row 中由 platform_start/platform_width 指定的平台上。
; EN: Convert platform columns/row to 9-bit sprite coordinates and supported state.
position_player_on_platform:
    ; 输入：spawn_row、platform_start、platform_width。
    ; 输出：player_x/player_y 与硬件 sprite 坐标；玩家进入“受平台支撑”状态。
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
    sta player_on_spring
    sta player_rise_velocity
    sta conveyor_move_counter
    lda #1
    sta player_on_platform
    rts

; EN: Apply five-range paddle movement with independent delay and boundary clamps.
update_player_horizontal:
    ; 输入：potx_raw。输出：必要时修改 9 位 player_x 并写 VIC。
    ; 中立区清零计数器，使下一次离开死区可以立即移动。最外两档只改变等待
    ; 帧数，单次位移始终为 2 像素，因此碰撞采样不会突然跨过大段平台。
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

; EN: Run supported, spring-rise, gravity, landing, and game-over state transitions.
update_player_vertical:
    ; 这是玩家垂直状态机，按顺序处理三种互斥状态：
    ;   1. player_on_platform != 0：检查支撑和顶框；
    ;   2. player_rise_velocity != 0：执行弹簧上升；
    ;   3. 其他：执行重力下落和着陆检测。
    ; 返回值不使用；需要结束游戏时只设置 game_over_flag，由 main_loop 收口。
    lda player_on_platform
    bne @supported
    jmp @airborne
@supported:

    lda player_y
    ; 顶端 Y 小于或等于顶框上沿时立即结束。比较阈值加一，可以在无符号
    ; 比较中把“相等”也归入越界，而不会晚一个像素。
    cmp #(PLAYER_TOP_GAME_OVER_Y+1)
    bcs @supported_in_bounds
    jmp handle_top_damage
@supported_in_bounds:

    ; 走出当前支撑平台的范围后开始下落。
    jsr find_platform_below_player
    cmp #COLLISION_NORMAL
    beq @supported_normal
    cmp #COLLISION_SPIKES
    beq @hit_spikes
    cmp #COLLISION_FADE_INACTIVE
    beq @activate_supported_fade
    cmp #COLLISION_FADE_ACTIVE
    beq @supported_fade
    cmp #COLLISION_SPRING
    beq @supported_spring
    cmp #COLLISION_CONVEYOR_LEFT
    beq @supported_conveyor
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @supported_conveyor
    jmp @begin_fall
@hit_spikes:
    jmp handle_spike_damage
@supported_normal:
    jsr cancel_spring_platform
    lda #0
    sta player_on_fade_platform
    sta conveyor_move_counter
    rts
@activate_supported_fade:
    jsr cancel_spring_platform
    jsr activate_fade_platform
    jmp @mark_supported_fade
@supported_fade:
    jsr cancel_spring_platform
@mark_supported_fade:
    lda #0
    sta conveyor_move_counter
    lda #1
    sta player_on_fade_platform
    rts
@supported_spring:
    lda #0
    sta player_on_fade_platform
    sta conveyor_move_counter
    lda player_on_spring
    bne @spring_ready
    jsr compress_spring_platform
@spring_ready:
    rts
@supported_conveyor:
    ; 传送平台提供正常支撑，但用独立计数器每三帧额外推动 X 两像素。水平
    ; paddle 更新先执行，因此玩家可以反向抵消传送，或顺向叠加速度。
    jsr cancel_spring_platform
    lda #0
    sta player_on_fade_platform
    jsr apply_conveyor_motion
    rts
@begin_fall:
    jsr cancel_spring_platform
    lda #0
    sta player_on_platform
    sta player_on_fade_platform
    sta player_fall_fraction
    sta player_fall_velocity
    sta player_rise_velocity
    sta conveyor_move_counter
    rts

@airborne:
    ; 离地时清除传送节流相位，使下一次落上传送平台能立即获得第一次推动。
    lda #0
    sta conveyor_move_counter
    lda player_rise_velocity
    beq @falling

; 弹簧上升速度和重力一样以 1/8 像素表示。初速度大于 8，因此一帧可能
; 上移多个像素；逐像素循环可保证高速上升也不会越过顶框判定。
@rising:
    lda player_y
    cmp #(PLAYER_TOP_GAME_OVER_Y+1)
    bcs @rise_in_bounds
    jmp handle_top_damage
@rise_in_bounds:
    lda player_fall_fraction
    clc
    adc player_rise_velocity
    sta player_fall_fraction
@rise_pixel:
    lda player_fall_fraction
    cmp #8
    bcc @decelerate_rise
    sec
    sbc #8
    sta player_fall_fraction
    dec player_y
    lda player_y
    sta VIC_SPRITE_0_Y
    cmp #(PLAYER_TOP_GAME_OVER_Y+1)
    bcs @rise_pixel
    jmp handle_top_damage
@decelerate_rise:
    dec player_rise_velocity
    bne @done
    lda #0
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
    cmp #COLLISION_NORMAL
    beq @land
    cmp #COLLISION_SPIKES
    beq @fall_on_spikes
    cmp #COLLISION_FADE_INACTIVE
    beq @land
    cmp #COLLISION_FADE_ACTIVE
    beq @land
    cmp #COLLISION_SPRING
    beq @land
    cmp #COLLISION_CONVEYOR_LEFT
    beq @land
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @land
    jmp @fall_done
@fall_on_spikes:
    ; 尖刺仍然是一个真正到达的新平台：先领取该行的一次性分数，再执行
    ; 扣 HP/反弹或死亡。claimed 标志保证同一尖刺不会反复刷分。
    jsr award_platform_landing_score
    jmp handle_spike_damage
@fall_done:
    rts

@store_fraction:
    sta player_fall_fraction
@done:
    rts

@land:
    ; 碰撞行对应的平台只有第一次从空中落上去时得分。平台自己的 claimed 位
    ; 会随字符行滚动，所以弹簧弹起后重落、走开后回到旧平台都不会重复加分。
    jsr award_platform_landing_score

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
    sta player_rise_velocity

    lda collision_type
    cmp #COLLISION_FADE_INACTIVE
    beq @activate_landed_fade
    cmp #COLLISION_FADE_ACTIVE
    beq @mark_landed_fade
    cmp #COLLISION_SPRING
    beq @compress_landed_spring
    cmp #COLLISION_CONVEYOR_LEFT
    beq @landed_conveyor
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @landed_conveyor
    lda #0
    sta player_on_fade_platform
    sta player_on_spring
    sta conveyor_move_counter
    beq @finish_landing
@activate_landed_fade:
    ; 若此前还有另一个正在倒计时的平台，激活流程会先将旧平台删除。
    lda #0
    sta player_on_fade_platform
    jsr activate_fade_platform
@mark_landed_fade:
    lda #1
    sta player_on_fade_platform
    lda #0
    sta player_on_spring
    sta conveyor_move_counter
    beq @finish_landing
@compress_landed_spring:
    lda #0
    sta player_on_fade_platform
    sta conveyor_move_counter
    jsr compress_spring_platform
    jmp @finish_landing
@landed_conveyor:
    ; 着陆帧立即获得一次传送位移，避免出现“先停一帧再开始移动”的顿感。
    lda #0
    sta player_on_fade_platform
    sta player_on_spring
    jsr apply_conveyor_motion
@finish_landing:
    lda #1
    sta player_on_platform
    rts

; EN: Set the deferred game-over flag without escaping the current JSR stack.
request_game_over:
    ; 不在物理例程中直接清屏。置位后返回主循环，可避免从多层 JSR 中强行
    ; 跳走并留下不平衡的返回地址。
    lda #1
    sta game_over_flag
    rts

; 返回 A=0 空白、1 普通、2 尖刺、3 未触发消失平台、4 已触发消失平台、
; 5 弹簧、6 向左传送、7 向右传送。完整和压扁弹簧使用同一种碰撞类型。
; 探测点位于 sprite 脚底，并允许两像素的跨越检测窗口。
;
; 平滑滚动期间，一个逻辑平台会同时显示在相邻的两个字符格中：LOWER
; 保留平台原来的逻辑行，UPPER 只绘制越过字符边界的像素。碰撞必须始终
; 使用 LOWER 所在行作为锚点；如果把 UPPER 也视作平台，小人会先在上一
; 字符行落地，视觉上比真实平台顶部高 8 像素。
; EN: Convert feet Y to a row, sample two X points, and return collision type in A.
find_platform_below_player:
    ; 输入：player_x、player_y、fine_scroll、active_screen。
    ; 输出：A=碰撞类型，并保存 collision_row/type 供着陆和平台效果使用。
    ; 只在脚底相对平台顶部 0..2 像素的窗口内返回平台，避免下落时从平台
    ; 侧面或底面“吸附”上去。
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

; EN: Point ROW_PTR at collision_row in the currently visible screen matrix.
set_collision_row_pointer:
    ; 输入：collision_row、active_screen。输出：ROW_PTR 指向当前可见屏幕的
    ; 对应字符行。advance_row_pointer 每次增加 40 字节，即前进一整行。
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
; EN: Convert a sprite-relative X offset to a column and classify its anchor cell.
sample_platform_at_player_offset:
    ; 输入：A=相对 sprite 左边的像素偏移（当前调用为 5 和 18）。
    ; 输出：A=该脚点下方的逻辑平台类型。
    ; 先把 sprite 坐标减去 VIC 可见区 X 起点 24，再对 16 位值右移三次，
    ; 相当于无符号除以 8，从像素 X 转换为字符列。UPPER 碎片故意不算支撑。
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
    cmp #CHAR_SPRING_FULL
    beq @spring
    cmp #CHAR_SPRING_LOWER
    beq @spring
    cmp #CHAR_SPRING_COMPRESSED_FULL
    beq @spring
    cmp #CHAR_SPRING_COMPRESSED_LOWER
    beq @spring
    cmp #CHAR_CONVEYOR_LEFT_FULL
    beq @conveyor_left
    cmp #CHAR_CONVEYOR_RIGHT_FULL
    beq @conveyor_right
    lda #0
    rts
@normal:
    lda #COLLISION_NORMAL
    rts
@spikes:
    lda #COLLISION_SPIKES
    rts
@fade_inactive:
    lda #COLLISION_FADE_INACTIVE
    rts
@fade_active:
    lda #COLLISION_FADE_ACTIVE
    rts
@spring:
    lda #COLLISION_SPRING
    rts
@conveyor_left:
    lda #COLLISION_CONVEYOR_LEFT
    rts
@conveyor_right:
    lda #COLLISION_CONVEYOR_RIGHT
    rts

; collision_type 已由本帧脚底采样确定。传送位移不使用 paddle 的节流计数器：
; 推动一次后独立跳过两帧，得到每三帧两像素；抵达竖井边缘时钳制。
; EN: Push two pixels every three calls in collision_type's conveyor direction.
apply_conveyor_motion:
    lda conveyor_move_counter
    beq @ready
    dec conveyor_move_counter
    rts
@ready:
    lda #CONVEYOR_MOVE_DELAY
    sta conveyor_move_counter

    lda collision_type
    cmp #COLLISION_CONVEYOR_LEFT
    beq @left
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @right
    rts

@left:
    lda player_x_high
    bne @subtract_left
    lda player_x_low
    cmp #PLAYER_MIN_X+CONVEYOR_PUSH_STEP
    bcs @subtract_left
    lda #PLAYER_MIN_X
    sta player_x_low
    lda #0
    sta player_x_high
    jmp write_player_x
@subtract_left:
    sec
    lda player_x_low
    sbc #CONVEYOR_PUSH_STEP
    sta player_x_low
    lda player_x_high
    sbc #0
    sta player_x_high
    jmp write_player_x

@right:
    clc
    lda player_x_low
    adc #CONVEYOR_PUSH_STEP
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

; 查找画面中最低的普通平台并将玩家放到上面。玩家碰到尖刺或离开竖井后
; 使用此流程重新定位。
; EN: Return C=1 when paddle movement is due; reload normal or fast delay.
player_movement_ready:
    ; 输出：C=1 表示本帧允许移动，C=0 表示仍在等待；调用者用 BCC 判断。
    ; SEC/CLC 在这里相当于一个布尔返回值，不是为后续加减法准备进位。
    lda player_move_counter
    beq @ready
    dec player_move_counter
    clc
    rts
@ready:
    ; 普通左右档仍是每三帧移动两像素。最外侧两档把等待值从 2 降为 1，
    ; 即每两帧移动两像素，平均速度恰好是原来的 1.5 倍。
    lda #PLAYER_MOVE_DELAY
    ldx potx_raw
    cpx #POTX_FAR_LEFT_END
    bcc @fast
    cpx #POTX_FAR_RIGHT_START
    bcc @store_delay
@fast:
    lda #PLAYER_FAST_MOVE_DELAY
@store_delay:
    sta player_move_counter
    sec
    rts

; EN: Write the software 9-bit X coordinate without disturbing sprite 4's high bit.
write_player_x:
    ; 把 9 位软件坐标写入 VIC：低八位写 $d000；高位写 $d010 bit 0。
    ; AND 只清玩家的 bit 0，绝不能破坏 sprite 4 表针使用的 bit 4。
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
