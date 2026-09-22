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
; 另一张隐藏矩阵，完成后只改一次 $d018。Color RAM 没有第二份，所以各类
; 平台的颜色只在字符转场时同步更新，不能像屏幕矩阵那样整页翻转。
;
; English summary:
;   Owns double-buffered VIC-IIe text rendering, dynamic glyph scroll phases,
;   fixed UI construction, and modal screens. active_screen is visible; large
;   transitions are written to the other matrix and exposed with one $d018
;   write. Color RAM is shared, so occupied platform cells are recolored
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
    ; 台阶托举也要在本次像素步立即判定，不能等下一帧再扣血或检测。
    ; EN: Handle ceiling damage in the same carried pixel, not one frame later.
    cmp #(PLAYER_CEILING_CONTACT_Y+1)
    bcs @scroll_playfield
    jmp handle_ceiling_damage

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
; 普通砖台阶和虚化平台的三级砖纹各用独立碎片，
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

    jsr update_brick_and_fade_fragments

    ldy #0
    lda #$00
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
    ; 主要在新游戏开始时调用；运行中的转场改为按平台描述重建隐藏游戏区。
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

; EN: Advance the paired screen pointers and the UI color-row pointer together.
advance_transition_pointers:
    ; UI 构建时 NEXT_ROW_PTR 指向 Color RAM；三份行指针同时前进。
    jsr advance_copy_pointers
    clc
    lda NEXT_ROW_PTR
    adc #SCREEN_COLUMNS
    sta NEXT_ROW_PTR
    bcc @done
    inc NEXT_ROW_PTR+1
@done:
    rts

; EN: Rebuild hidden LOWER/UPPER spans from platform descriptions, not screen cells.
start_fragment_transition:
    jmp render_hidden_platforms

; fine_scroll 从 0 回到 7 时，上部碎片在上移一行的位置变为完整字符，下部碎片消失；
; 此时可以从画面底部加入新平台。
; EN: Shift descriptions, rebuild FULL spans, and generate the new bottom row.
complete_fragment_transition:
    ; Shift canonical rows alongside score claims, then rebuild FULL spans.
    jsr advance_platform_rows
    jsr render_hidden_platforms

    ; 翻屏前更新传送平台的独立 FULL 槽，保留旧屏仍引用的 LOWER 碎片。
    ; 使用当前水平相位，不能将表面动画重置回源图案。
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

; EN: Load the full text page, clear both matrices, and draw the title screen.
show_start_screen:
    ; 模态画面：关闭 sprite、固定显示 SCREEN_A、清两屏，再写标题和提示。
    jsr prepare_modal_screen

    ldx #0
@title:
    lda start_title, x
    STORE_SCREEN_PAIR TITLE_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+TITLE_OFFSET, x
    inx
    cpx #8
    bne @title
    jsr draw_action_prompt
    jmp enable_charset_display

; EN: Load the full text page, clear gameplay, and draw the game-over screen.
show_game_over_screen:
    jmp show_highscore_table
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
    ldy #CHAR_BLANK
; EN: Reset platform descriptions; Y is the fill code (game 0 or text space 32).
clear_screens_with_character:
    ; X 从 0 自增到溢出，恰好遍历 256 个偏移；四个页面合计覆盖每个 1 KB
    ; 屏幕区和 Color RAM。最后 24 个非可见字节也填充，包含 sprite 指针表，
    ; 所以后续 initialize_player/dashboard 必须重新写指针。
    tya
    pha
    jsr reset_platform_rows
    pla
    tay
    ldx #0
@loop:
    tya
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

; 固定字符框：硬件边框为黑色，屏幕第 1/23 行与第 1/38 列组成砖纹外框，
; 第 29 列分隔游戏区和八列状态栏。倒尖刺固定在第 2 行游戏区；第 0/24 行
; 以及第 0/39 列保留黑色，形成参考图中的外侧留白。
; EN: Draw the brick frame/divider, fixed character dial and white/green ceiling spikes.
initialize_ui:
    ; 输出：两张屏幕都有相同的砖纹框、分隔列、POTX 区、字符表盘和倒尖刺。
    ; 水平线适合绝对地址+X；垂直线适合三个行指针+固定 Y 列，这展示了两种
    ; 常见的 6502 屏幕遍历方式。
    ; 先画上下两条水平框线。
    ldx #0
@horizontal:
    lda #CHAR_UI_FRAME
    STORE_SCREEN_PAIR UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    STORE_SCREEN_PAIR UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    lda #UI_FRAME_COLOR
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
    lda #UI_FRAME_COLOR
    sta (NEXT_ROW_PTR), y

    ldy #UI_DIVIDER_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #UI_FRAME_COLOR
    sta (NEXT_ROW_PTR), y

    ldy #UI_FRAME_RIGHT_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #UI_FRAME_COLOR
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
    jmp draw_ceiling_spikes

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

; 固定危险区仅在建场景时绘制；两种滚动过程都不写这一行，不占 sprite。
; EN: Draw once into both matrices, using the same white/green multicolor palette.
.segment "EXTCODE"
draw_ceiling_spikes:
    ldx #PLAYFIELD_FIRST_COLUMN
@column:
    lda #CHAR_CEILING_SPIKE
    STORE_SCREEN_PAIR CEILING_SPIKE_ROW*SCREEN_COLUMNS, x
    lda #SPIKE_CELL_COLOR
    sta COLOR_RAM+CEILING_SPIKE_ROW*SCREEN_COLUMNS, x
    inx
    cpx #PLAYFIELD_LAST_COLUMN+1
    bne @column
    rts
