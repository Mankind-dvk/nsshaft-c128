; ---------------------------------------------------------------------------
; VIC-IIe 屏幕系统
; ---------------------------------------------------------------------------
; 本模块负责平台逐像素平滑过渡、双缓冲屏幕复制、开始/结束画面、固定列
; 分隔条，以及字符表盘的放置。硬件 D011 固定在 25 行模式的自然相位 3；
; 平滑运动改由实时改写各平台类型的上下过渡字形实现。
;
; 公共入口：
;   scroll_one_pixel_up、prepare_screen_b_from_a、show_start_screen、
;   show_game_over_screen、wait_for_action_button、clear_screens_and_colors、
;   initialize_ui。
;
; 双缓冲模型：active_screen 指出的矩阵正在被 VIC 显示，所有大规模转场先写
; 另一张隐藏矩阵，完成后只改一次 $d018。Color RAM 没有第二份，所以弹簧
; 紫色只在对应字符确实移动时同步更新，不能像屏幕矩阵那样整页翻转。
;
; English summary:
;   Owns double-buffered VIC-IIe text rendering, dynamic glyph scroll phases,
;   fixed UI construction, and modal screens. active_screen is visible; large
;   transitions are written to the other matrix and exposed with one $d018
;   write. Color RAM is shared, so colored spring/conveyor cells are moved
;   explicitly. Software fine_scroll never changes the fixed HUD's $d011 phase.

.segment "CODE"

; EN: Advance the playfield by one pixel and carry a supported player upward.
scroll_one_pixel_up:
    ; 输入：fine_scroll=0..7、active_screen、player_on_platform。
    ; 输出：世界向上推进一个像素。相位 7 开始一次跨字符行转场，相位 6..1
    ; 只重画动态字形，相位 0 完成粗滚动并翻页。
    ; 玩家站在平台上时，会随滚动平台一起向上移动。
    lda player_on_platform
    beq @scroll_playfield
    dec player_y
    lda player_y
    sta VIC_SPRITE_0_Y

@scroll_playfield:
    lda fine_scroll
    cmp #7
    bne @fragments_exist

    dec fine_scroll
    jsr update_fragment_glyphs
    jsr start_fragment_transition
    jmp flip_hidden_screen

@fragments_exist:
    lda fine_scroll
    beq @coarse_step
    dec fine_scroll
    jmp update_fragment_glyphs

@coarse_step:
    lda #7
    sta fine_scroll
    jsr advance_platform_score_rows
    jsr complete_fragment_transition
    jsr advance_fade_platform_scroll_position
    jsr advance_spring_platform_scroll_position
    jmp flip_hidden_screen

; VIC 垂直滚动寄存器始终保持自然相位 3，使 25 行字符完整覆盖可视区。
; 普通/未触发灰色平台共用一对矩形碎片；已触发平台使用另一对矩形碎片，
; 尖刺、弹簧和左右传送平台各自生成保持图案形状的动态碎片。
; EN: Rebuild every platform's runtime UPPER/LOWER bitmaps for fine_scroll.
update_fragment_glyphs:
    ; 输入：fine_scroll。输出：重写普通、尖刺、消失、弹簧和传送平台碎片。
    ; 屏幕码保持不变时，修改字形 RAM 会立即改变所有引用该字形的单元格，
    ; 因此中间六个像素相位不需要遍历整个屏幕矩阵。
    lda fine_scroll
    clc
    adc #1
    sta fragment_split
    lda #7
    sec
    sbc fine_scroll
    sta fragment_count

    ldy #0
@normal_masks:
    cpy fragment_split
    bcc @normal_top
    lda #$00
    sta normal_upper_glyph, y
    sta fade_active_upper_glyph, y
    lda #$ff
    sta normal_lower_glyph, y
    sta fade_active_lower_glyph, y
    jmp @normal_next
@normal_top:
    lda #$ff
    sta normal_upper_glyph, y
    sta fade_active_upper_glyph, y
    lda #$00
    sta normal_lower_glyph, y
    sta fade_active_lower_glyph, y
@normal_next:
    iny
    cpy #8
    bne @normal_masks

    ldy #0
    lda #$ff
@clear_spikes:
    sta spike_upper_glyph, y
    sta spike_lower_glyph, y
    iny
    cpy #8
    bne @clear_spikes

    ldx #0
    ldy fragment_split
@spike_upper:
    lda spike_source_glyph, x
    sta spike_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @spike_upper

    ldx fragment_count
    ldy #0
@spike_lower:
    lda spike_source_glyph, x
    sta spike_lower_glyph, y
    inx
    iny
    cpx #8
    bne @spike_lower
    jsr update_conveyor_fragment_glyphs
    jmp update_spring_fragment_glyphs

; EN: Make the hidden screen a complete copy of the active screen.
prepare_screen_b_from_a:
copy_active_to_hidden:
    ; 输入：active_screen。输出：隐藏屏成为可见屏的 1000 字节完整副本。
    ; 主要在新游戏开始时调用；运行中的转场只复制游戏区所需单元格。
    jsr set_screen_copy_pointers
    ldx #SCREEN_ROWS
@row:
    ldy #SCREEN_COLUMNS-1
@column:
    lda (ROW_PTR), y
    sta (DEST_PTR), y
    dey
    bpl @column
    jsr advance_copy_pointers
    dex
    bne @row
    rts

; EN: Set ROW_PTR=visible and DEST_PTR=hidden according to active_screen.
set_screen_copy_pointers:
    ; 输出：ROW_PTR=当前可见屏，DEST_PTR=隐藏屏。后续代码统一使用间接寻址，
    ; 无需为 A->B 和 B->A 写两套循环。
    lda active_screen
    beq @a_to_b
    SET_WORD ROW_PTR, SCREEN_B
    SET_WORD DEST_PTR, SCREEN_A
    rts
@a_to_b:
    SET_WORD ROW_PTR, SCREEN_A
    SET_WORD DEST_PTR, SCREEN_B
    rts

; EN: Add one 40-byte row to ROW_PTR and DEST_PTR.
advance_copy_pointers:
    ; 将 ROW_PTR 和 DEST_PTR 同时增加 40。ADC 前必须 CLC；低字节溢出时再
    ; INC 高字节，这是 6502 实现 16 位加法的标准写法。
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta ROW_PTR
    bcc @source_ready
    inc ROW_PTR+1
@source_ready:
    clc
    lda DEST_PTR
    adc #SCREEN_COLUMNS
    sta DEST_PTR
    bcc @done
    inc DEST_PTR+1
@done:
    rts

; EN: Advance source, destination, and look-ahead row pointers together.
advance_transition_pointers:
    ; 在 advance_copy_pointers 基础上，再推进 NEXT_ROW_PTR。开始转场时它
    ; 始终指向源行的下一行，用来判断空白格下方是否有完整平台。
    jsr advance_copy_pointers
    clc
    lda NEXT_ROW_PTR
    adc #SCREEN_COLUMNS
    sta NEXT_ROW_PTR
    bcc @done
    inc NEXT_ROW_PTR+1
@done:
    rts

; 屏幕 A/B 的地址低十位都与 Color RAM 对应，因此保留低字节，并把高字节
; 的低两位映射到 $d8..$db，即可得到当前目标行的颜色指针。
; EN: Map DEST_PTR's low 10 address bits to the corresponding Color RAM cell.
set_color_pointer_from_dest:
    ; 输入：DEST_PTR 指向某个屏幕行。输出：COLOR_PTR 指向同一行的 Color RAM。
    lda DEST_PTR
    sta COLOR_PTR
    lda DEST_PTR+1
    and #%00000011
    ora #>COLOR_RAM
    sta COLOR_PTR+1
    rts

; EN: Add one 40-byte row to COLOR_PTR.
advance_color_pointer:
    ; COLOR_PTR += 40；与目标屏幕行保持同步。
    clc
    lda COLOR_PTR
    adc #SCREEN_COLUMNS
    sta COLOR_PTR
    bcc @done
    inc COLOR_PTR+1
@done:
    rts

; 相位 1 将每个完整平台替换为下部碎片，并在上一字符行插入对应的上部碎片。
; EN: Convert FULL cells to LOWER and insert UPPER cells in the hidden buffer.
start_fragment_transition:
    ; 相位 7->6 时调用。对每个游戏区单元格执行：
    ;   当前行完整平台 -> 当前行 LOWER；
    ;   当前行空白且下一行有完整平台 -> 当前行 UPPER；
    ;   其他字符 -> 原样或空白。
    ; 结果写隐藏屏，循环结束后由调用者 flip_hidden_screen 一次性显示。
    jsr set_screen_copy_pointers
    ; 青色框占据第 1/23 行，平台只允许在第 2..22 行滚动。跳过前两行也
    ; 会让最上方平台自然消失在框线后面，而不是覆盖顶框。
    jsr advance_copy_pointers
    jsr advance_copy_pointers
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta NEXT_ROW_PTR
    lda ROW_PTR+1
    adc #0
    sta NEXT_ROW_PTR+1
    jsr set_color_pointer_from_dest

    ldx #PLAYFIELD_ROW_COUNT
@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    beq @check_next_row
    cmp #CHAR_PLATFORM_FULL
    beq @normal_lower
    cmp #CHAR_SPIKE_TRIANGLE
    beq @spike_lower
    cmp #CHAR_FADE_FULL
    beq @fade_lower
    cmp #CHAR_FADE_ACTIVE_RED_FULL
    beq @fade_active_red_lower
    cmp #CHAR_FADE_ACTIVE_GRAY_FULL
    beq @fade_active_gray_lower
    cmp #CHAR_SPRING_FULL
    beq @spring_lower
    cmp #CHAR_SPRING_COMPRESSED_FULL
    beq @spring_compressed_lower
    cmp #CHAR_CONVEYOR_LEFT_FULL
    beq @conveyor_left_lower
    cmp #CHAR_CONVEYOR_RIGHT_FULL
    beq @conveyor_right_lower
    ; 游戏区中若以后加入其他固定字符，原样复制到隐藏屏。
    jmp @store

@check_next_row:
    cpx #1
    bne @read_next_row
    jmp @store
@read_next_row:
    lda (NEXT_ROW_PTR), y
    beq @blank
    cmp #CHAR_PLATFORM_FULL
    beq @normal_upper
    cmp #CHAR_SPIKE_TRIANGLE
    beq @spike_upper
    cmp #CHAR_FADE_FULL
    beq @fade_upper
    cmp #CHAR_FADE_ACTIVE_RED_FULL
    beq @fade_active_red_upper
    cmp #CHAR_FADE_ACTIVE_GRAY_FULL
    beq @fade_active_gray_upper
    cmp #CHAR_SPRING_FULL
    beq @spring_upper
    cmp #CHAR_SPRING_COMPRESSED_FULL
    beq @spring_compressed_upper
    cmp #CHAR_CONVEYOR_LEFT_FULL
    beq @conveyor_left_upper
    cmp #CHAR_CONVEYOR_RIGHT_FULL
    beq @conveyor_right_upper
@blank:
    lda #CHAR_BLANK
    beq @store
@normal_lower:
    lda #CHAR_NORMAL_LOWER
    bne @store
@spike_lower:
    lda #CHAR_SPIKE_LOWER
    bne @store
@fade_lower:
    lda #CHAR_FADE_LOWER
    bne @store
@fade_active_red_lower:
    lda #CHAR_FADE_ACTIVE_RED_LOWER
    bne @store
@fade_active_gray_lower:
    lda #CHAR_FADE_ACTIVE_GRAY_LOWER
    bne @store
@spring_lower:
    lda #COLOR_PURPLE
    sta (COLOR_PTR), y
    lda #CHAR_SPRING_LOWER
    bne @store
@spring_compressed_lower:
    lda #COLOR_PURPLE
    sta (COLOR_PTR), y
    lda #CHAR_SPRING_COMPRESSED_LOWER
    bne @store
@conveyor_left_lower:
    lda #COLOR_GREEN
    sta (COLOR_PTR), y
    lda #CHAR_CONVEYOR_LEFT_FULL
    bne @store
@conveyor_right_lower:
    lda #COLOR_GREEN
    sta (COLOR_PTR), y
    lda #CHAR_CONVEYOR_RIGHT_FULL
    bne @store
@normal_upper:
    lda #CHAR_NORMAL_UPPER
    bne @store
@spike_upper:
    lda #CHAR_SPIKE_UPPER
    bne @store
@fade_upper:
    lda #CHAR_FADE_UPPER
    bne @store
@fade_active_red_upper:
    lda #CHAR_FADE_ACTIVE_RED_UPPER
    bne @store
@fade_active_gray_upper:
    lda #CHAR_FADE_ACTIVE_GRAY_UPPER
    bne @store
@spring_upper:
    lda #COLOR_PURPLE
    sta (COLOR_PTR), y
    lda #CHAR_SPRING_UPPER
    bne @store
@spring_compressed_upper:
    lda #COLOR_PURPLE
    sta (COLOR_PTR), y
    lda #CHAR_SPRING_COMPRESSED_UPPER
    bne @store
@conveyor_left_upper:
    lda #COLOR_GREEN
    sta (COLOR_PTR), y
    lda #CHAR_CONVEYOR_LEFT_UPPER
    bne @store
@conveyor_right_upper:
    lda #COLOR_GREEN
    sta (COLOR_PTR), y
    lda #CHAR_CONVEYOR_RIGHT_UPPER
@store:
    sta (DEST_PTR), y
@next_column:
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcc @row_done
    jmp @column
@row_done:
    jsr advance_transition_pointers
    jsr advance_color_pointer
    dex
    beq @done
    jmp @row
@done:
    rts

; 相位 7 完成后，上部碎片在上移一行的位置变为完整字符，下部碎片消失；
; 此时可以从画面底部加入新平台。
; EN: Promote UPPER to FULL, erase LOWER, and generate the new bottom row.
complete_fragment_transition:
    ; 相位 0->7 时调用。UPPER 在上移后的字符行变回 FULL，LOWER 清为空白，
    ; 从而完成“像素滚动八次等于字符行上移一次”。随后重建最底逻辑行。
    jsr set_screen_copy_pointers
    jsr advance_copy_pointers
    jsr advance_copy_pointers
    jsr set_color_pointer_from_dest
    ldx #PLAYFIELD_ROW_COUNT
@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    bne @classify
    jmp @store
@classify:
    cmp #CHAR_NORMAL_UPPER
    beq @normal_full
    cmp #CHAR_SPIKE_UPPER
    beq @spike_full
    cmp #CHAR_FADE_UPPER
    beq @fade_full
    cmp #CHAR_FADE_ACTIVE_RED_UPPER
    beq @fade_active_red_full
    cmp #CHAR_FADE_ACTIVE_GRAY_UPPER
    beq @fade_active_gray_full
    cmp #CHAR_SPRING_UPPER
    beq @spring_full
    cmp #CHAR_SPRING_COMPRESSED_UPPER
    beq @spring_compressed_full
    cmp #CHAR_CONVEYOR_LEFT_UPPER
    beq @conveyor_left_full
    cmp #CHAR_CONVEYOR_RIGHT_UPPER
    beq @conveyor_right_full
    cmp #CHAR_NORMAL_LOWER
    beq @blank
    cmp #CHAR_SPIKE_LOWER
    beq @blank
    cmp #CHAR_FADE_LOWER
    beq @blank
    cmp #CHAR_FADE_ACTIVE_RED_LOWER
    beq @blank
    cmp #CHAR_FADE_ACTIVE_GRAY_LOWER
    beq @blank
    cmp #CHAR_SPRING_LOWER
    beq @spring_blank
    cmp #CHAR_SPRING_COMPRESSED_LOWER
    beq @spring_blank
    cmp #CHAR_CONVEYOR_LEFT_FULL
    beq @conveyor_blank
    cmp #CHAR_CONVEYOR_RIGHT_FULL
    beq @conveyor_blank
    bne @store
@spring_blank:
    lda #COLOR_BLACK
    sta (COLOR_PTR), y
    beq @blank
@conveyor_blank:
    lda #COLOR_BLACK
    sta (COLOR_PTR), y
@blank:
    lda #CHAR_BLANK
    beq @store
@normal_full:
    lda #CHAR_PLATFORM_FULL
    bne @store
@spike_full:
    lda #CHAR_SPIKE_TRIANGLE
    bne @store
@fade_full:
    lda #CHAR_FADE_FULL
    bne @store
@fade_active_red_full:
    lda #CHAR_FADE_ACTIVE_RED_FULL
    bne @store
@fade_active_gray_full:
    lda #CHAR_FADE_ACTIVE_GRAY_FULL
    bne @store
@spring_full:
    lda #COLOR_PURPLE
    sta (COLOR_PTR), y
    lda #CHAR_SPRING_FULL
    bne @store
@spring_compressed_full:
    lda #COLOR_PURPLE
    sta (COLOR_PTR), y
    lda #CHAR_SPRING_COMPRESSED_FULL
    bne @store
@conveyor_left_full:
    lda #COLOR_GREEN
    sta (COLOR_PTR), y
    lda #CHAR_CONVEYOR_LEFT_FULL
    bne @store
@conveyor_right_full:
    lda #COLOR_GREEN
    sta (COLOR_PTR), y
    lda #CHAR_CONVEYOR_RIGHT_FULL
@store:
    sta (DEST_PTR), y
@next_column:
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcc @row_done
    jmp @column
@row_done:
    jsr advance_copy_pointers
    jsr advance_color_pointer
    dex
    beq @rows_done
    jmp @row
@rows_done:

    ; 传送平台的 FULL 槽在过渡阶段兼任 LOWER；在底部生成新平台和翻屏前
    ; 恢复完整箭头，否则新字符会短暂显示成只剩一行的碎片。
    jsr restore_conveyor_full_glyphs

    lda active_screen
    beq @bottom_b
    SET_WORD ROW_PTR, (SCREEN_A + PLAYFIELD_BOTTOM_OFFSET)
    jmp generate_bottom_row
@bottom_b:
    SET_WORD ROW_PTR, (SCREEN_B + PLAYFIELD_BOTTOM_OFFSET)
    jmp generate_bottom_row

; EN: Toggle active_screen and select its matrix with one $d018 write.
flip_hidden_screen:
    ; active_screen ^= 1。EOR #1 是切换 0/1 的常见写法；随后写 $d018 选择
    ; 对应屏幕矩阵。字符集地址不变，所以只需两个预计算常量。
    lda active_screen
    eor #1
    sta active_screen
    beq @show_screen_a

    SHOW_SCREEN SCREEN_B_D018
    rts

@show_screen_a:
    SHOW_SCREEN SCREEN_A_D018
    rts

; EN: Clear the hidden bottom row and optionally generate a random platform.
generate_bottom_row:
    ; 输入：ROW_PTR 指向刚完成粗滚动后的隐藏屏底部游戏行。
    ; 输出：清空该行；rows_until_platform=0 时调用随机生成并重新装载间距。
    lda #CHAR_BLANK
    ldy #PLAYFIELD_LAST_COLUMN
@clear:
    sta (ROW_PTR), y
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @clear

    lda rows_until_platform
    beq @add_platform
    dec rows_until_platform
    rts

@add_platform:
    jsr randomize_platform
    jsr commit_generated_platform_type
    jsr draw_platform

    ; 新平台位于游戏区最后一个逻辑行，等待玩家第一次落地领取分数。
    lda #PLATFORM_SCORE_UNCLAIMED
    sta platform_score_rows+PLAYFIELD_ROW_COUNT-1

    lda #PLATFORM_GAP_ROWS
    sta rows_until_platform
    rts

; 不滚动的开始与游戏结束画面 ---------------------------------------------

; EN: Restore modal glyphs, clear both matrices, and draw the title screen.
show_start_screen:
    ; 模态画面：关闭 sprite、固定显示 SCREEN_A、清两屏，再写标题和提示。
    jsr restore_modal_font_glyphs
    lda #0
    sta VIC_SPRITE_ENABLE
    SHOW_SCREEN SCREEN_A_D018
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    jsr clear_screens_and_colors

    ldx #0
@title:
    lda start_title, x
    STORE_SCREEN_PAIR TITLE_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+TITLE_OFFSET, x
    inx
    cpx #8
    bne @title
    jmp draw_action_prompt

; EN: Restore modal glyphs, clear gameplay, and draw the game-over screen.
show_game_over_screen:
    ; 与开始画面相同，但标题使用红色。游戏世界不会在此期间继续滚动。
    jsr restore_modal_font_glyphs
    lda #0
    sta VIC_SPRITE_ENABLE
    SHOW_SCREEN SCREEN_A_D018
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    jsr clear_screens_and_colors

    ldx #0
@title:
    lda game_over_title, x
    STORE_SCREEN_PAIR GAME_OVER_OFFSET, x
    lda #COLOR_RED
    sta COLOR_RAM+GAME_OVER_OFFSET, x
    inx
    cpx #9
    bne @title

    ; 游戏画面和模态画面共用硬件 sprite 0。清屏已经清除了指针表，所以要在
    ; 此处重新写入 hurt 块号、固定坐标和单色寄存器，然后只启用 sprite 0。
    jsr show_game_over_player
    jmp draw_action_prompt

; EN: Center the hurt frame between GAME OVER and PRESS FIRE using sprite 0.
show_game_over_player:
    ldy selected_character
    lda character_hurt_blocks, y
    STORE_SPRITE_POINTER 0

    lda #GAME_OVER_PLAYER_X
    sta VIC_SPRITE_0_X
    lda #GAME_OVER_PLAYER_Y
    sta VIC_SPRITE_0_Y
    CLEAR_REGISTER_BITS VIC_SPRITE_X_HIGH, %00000001

    lda #0
    sta VIC_SPRITE_MULTICOLOR
    sta VIC_SPRITE_X_EXPAND
    sta VIC_SPRITE_Y_EXPAND
    sta VIC_SPRITE_PRIORITY
    lda character_color_table, y
    sta VIC_SPRITE_0_COLOR
    lda #%00000001
    sta VIC_SPRITE_ENABLE
    rts

; EN: Mirror the ten-character action prompt into both screen buffers.
draw_action_prompt:
    ; 同一份十字符提示写入 A/B；这样从菜单返回前无论 D018 原值如何都一致。
    ldx #0
@prompt:
    lda action_prompt, x
    STORE_SCREEN_PAIR PROMPT_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+PROMPT_OFFSET, x
    inx
    cpx #10
    bne @prompt
    rts

; 接受控制端口 1 上的 paddle 按钮 1/2 和摇杆 FIRE。按下前后都等待按钮
; 释放，可防止一次按键连续跳过两个画面。
; EN: Wait for release -> press -> release to debounce and prevent carry-over.
wait_for_action_button:
    ; 依次等待“已释放 -> 按下 -> 再释放”，形成软件去抖和边沿触发。若只等
    ; 按下，上一画面的同一次长按会立刻跳过下一画面。
@wait_release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @wait_release
@wait_press:
    jsr wait_for_frame
    jsr action_button_pressed
    bcc @wait_press
@wait_final_release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @wait_final_release
    rts

; EN: Return C=1 when any port-1 paddle/fire action button is active.
action_button_pressed:
    ; 输出：C=1 有任一动作按钮按下，C=0 全部释放。按钮低电平有效，因此
    ; PB2..PB4 全为 1 才表示没有按键。
    lda CIA1_PORT_B
    and #%00011100
    cmp #%00011100
    beq @not_pressed
    sec
    rts
@not_pressed:
    clc
    rts

; EN: Clear both 1 KiB matrices and the complete 1 KiB Color RAM region.
clear_screens_and_colors:
    ; X 从 0 自增到溢出，恰好遍历 256 个偏移；四个页面合计覆盖每个 1 KB
    ; 屏幕区和 Color RAM。最后 24 个非可见字节也清零，包含 sprite 指针表，
    ; 所以后续 initialize_player/dashboard 必须重新写指针。
    ldx #0
@loop:
    lda #CHAR_BLANK
    sta SCREEN_A+$000, x
    sta SCREEN_A+$100, x
    sta SCREEN_A+$200, x
    sta SCREEN_A+$300, x
    sta SCREEN_B+$000, x
    sta SCREEN_B+$100, x
    sta SCREEN_B+$200, x
    sta SCREEN_B+$300, x

    lda #COLOR_BLACK
    sta COLOR_RAM+$000, x
    sta COLOR_RAM+$100, x
    sta COLOR_RAM+$200, x
    sta COLOR_RAM+$300, x
    inx
    bne @loop
    rts

; 参考旧版 NS-Shaft 的固定字符框：硬件边框为黑色，屏幕第 1/23 行与
; 第 1/38 列组成青色外框，第 29 列分隔游戏区和八列状态栏。第 0/24 行
; 以及第 0/39 列保留黑色，形成参考图中的外侧留白。
; EN: Draw the cyan frame/divider, POTX area, and fixed character dial.
initialize_ui:
    ; 输出：两张屏幕都有完全相同的青色框、分隔列、POTX 区和字符表盘。
    ; 水平线适合绝对地址+X；垂直线适合三个行指针+固定 Y 列，这展示了两种
    ; 常见的 6502 屏幕遍历方式。
    ; 先画上下两条水平框线。
    ldx #0
@horizontal:
    lda #CHAR_UI_FRAME
    STORE_SCREEN_PAIR UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    STORE_SCREEN_PAIR UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    lda #COLOR_CYAN
    sta COLOR_RAM+UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    sta COLOR_RAM+UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    inx
    cpx #UI_FRAME_WIDTH
    bne @horizontal

    ; 再逐行画左框、分隔条和右框。三个指针分别指向屏幕 A、屏幕 B 和
    ; Color RAM；先越过第 0 行，到达框线起始行 1。
    SET_WORD ROW_PTR, SCREEN_A
    SET_WORD DEST_PTR, SCREEN_B
    SET_WORD NEXT_ROW_PTR, COLOR_RAM
    jsr advance_transition_pointers

    ldx #UI_FRAME_HEIGHT
@vertical:
    ldy #UI_FRAME_LEFT_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #COLOR_CYAN
    sta (NEXT_ROW_PTR), y

    ldy #UI_DIVIDER_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #COLOR_CYAN
    sta (NEXT_ROW_PTR), y

    ldy #UI_FRAME_RIGHT_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #COLOR_CYAN
    sta (NEXT_ROW_PTR), y

    jsr advance_transition_pointers
    dex
    bne @vertical

    ldx #0
@dial_row_1:
    lda dial_row_1, x
    STORE_SCREEN_PAIR DIAL_TOP_ROW*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    lda #COLOR_WHITE
    sta COLOR_RAM+DIAL_TOP_ROW*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    inx
    cpx #8
    bne @dial_row_1

    ldx #0
@dial_row_2:
    lda dial_row_2, x
    STORE_SCREEN_PAIR (DIAL_TOP_ROW+1)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    lda #COLOR_WHITE
    sta COLOR_RAM+(DIAL_TOP_ROW+1)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    inx
    cpx #8
    bne @dial_row_2

    ldx #0
@dial_row_3:
    lda dial_row_3, x
    STORE_SCREEN_PAIR (DIAL_TOP_ROW+2)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    lda #COLOR_WHITE
    sta COLOR_RAM+(DIAL_TOP_ROW+2)*SCREEN_COLUMNS+DIAL_FIRST_COLUMN, x
    inx
    cpx #8
    bne @dial_row_3
    rts

; EN: Add one 40-byte row to ROW_PTR only.
advance_row_pointer:
    ; 只推进 ROW_PTR，一般用于碰撞查询从屏幕基址走到指定字符行。
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta ROW_PTR
    bcc @done
    inc ROW_PTR+1
@done:
    rts
