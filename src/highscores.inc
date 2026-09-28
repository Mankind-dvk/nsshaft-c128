; 排行榜上层：管理前五名、输入姓名、绘制结束界面；磁盘操作在 highscore_storage.inc。
; 建议先看 run_highscore_game_over，了解“查排名 -> 输入 -> 插入 -> 保存 -> 展示”。
; 下方分区：数据规则 / 姓名编辑 / 主流程 / 键盘 / 绘制 / 静态数据。
; 仅启动和游戏结束时执行，不加入游戏每帧更新；独立段避开图形、字库和音乐区。
.segment "HIGHSCORES"

; === 1. 数据规则与排名维护 ===
; 每条记录 = 8 字节姓名屏幕码（不足补空格）+ 6 字节分数（每字节一位，高位在前）。
; 五条共 70 字节；count 标记有效条数。分数不是 ASCII，也不是压缩 BCD。
HIGHSCORE_COUNT = 5
HIGHSCORE_NAME_LENGTH = 8
HIGHSCORE_RECORD_SIZE = HIGHSCORE_NAME_LENGTH + SCORE_DIGIT_COUNT
HIGHSCORE_TABLE_SIZE = HIGHSCORE_COUNT * HIGHSCORE_RECORD_SIZE
HS_NO_RANK = $ff
HS_KEY_RETURN = $0d
HS_KEY_DELETE = $14
HS_EXT_KEYBOARD = $d02f

.macro HS_TEXT text_address, offset, color
    SET_WORD ROW_PTR, text_address
    SET_WORD DEST_PTR, SCREEN_A+offset
    lda #color
    sta highscore_draw_color
    jsr highscore_print
.endmacro

; 仅程序启动、读盘之前清空一次；每局重开不能调用，否则会丢掉已有排名。
initialize_highscores:
    lda #0
    sta highscore_count
    sta highscore_dirty
    ldx #HIGHSCORE_TABLE_SIZE-1
@clear:
    sta highscore_entries,x
    dex
    bpl @clear
    lda #HS_NO_RANK
    sta highscore_rank
    jmp highscore_name_reset

; 只查位置，不修改记录；同分时旧记录在前，新记录继续向后找位置。
; 输出 highscore_rank：0..4 表示插入位置，$ff 表示不能上榜。
; 从最高位向后比较，首个不同数字决定大小；未满五条时允许追加。
highscore_find_rank:
    lda #0
    sta highscore_work_row
    sta highscore_work_offset
@row:
    lda highscore_work_row
    cmp highscore_count
    bcs @append
    ldx highscore_work_offset
    ldy #0
@digit:
    lda score_digits,y
    cmp highscore_entries+HIGHSCORE_NAME_LENGTH,x
    bcc @next
    bne @found
    inx
    iny
    cpy #SCORE_DIGIT_COUNT
    bne @digit
@next:
    inc highscore_work_row
    lda highscore_work_offset
    clc
    adc #HIGHSCORE_RECORD_SIZE
    sta highscore_work_offset
    jmp @row
@append:
    cmp #HIGHSCORE_COUNT
    bcc @found
    lda #HS_NO_RANK
    sta highscore_rank
    rts
@found:
    lda highscore_work_row
    sta highscore_rank
    rts

; 名次有效且姓名确认后才插入：从后向前搬动记录，避免覆盖还没搬走的数据。
; 表满时丢弃原第五名。绘制函数不调用这里，因此刷新画面不会重复插入成绩。
highscore_insert:
    ldx highscore_rank
    cpx #HIGHSCORE_COUNT
    bcs @done
    lda highscore_record_offsets,x
    sta highscore_work_offset
    ldx #HIGHSCORE_TABLE_SIZE-HIGHSCORE_RECORD_SIZE-1
@shift:
    cpx highscore_work_offset
    bcc @copy
    lda highscore_entries,x
    sta highscore_entries+HIGHSCORE_RECORD_SIZE,x
    dex
    bpl @shift
@copy:
    ldx highscore_work_offset
    ldy #0
@name:
    lda highscore_name,y
    sta highscore_entries,x
    inx
    iny
    cpy #HIGHSCORE_NAME_LENGTH
    bne @name
    ldy #0
@score:
    lda score_digits,y
    sta highscore_entries,x
    inx
    iny
    cpy #SCORE_DIGIT_COUNT
    bne @score
    lda highscore_count
    cmp #HIGHSCORE_COUNT
    bcs @dirty
    inc highscore_count
@dirty:
    ; dirty=1 只表示内存已修改；写盘并回读成功后，才算永久保存。
    lda #1
    sta highscore_dirty
@done:
    rts

; === 2. 姓名编辑：固定八格，输入与绘制分开 ===
; 用文字字库的空格填满，不能用字符码 0 冒充空格。
highscore_name_reset:
    lda #0
    sta highscore_name_length
    lda #TEXT_SPACE
    ldx #HIGHSCORE_NAME_LENGTH-1
@blank:
    sta highscore_name,x
    dex
    bpl @blank
    rts

; 输入 A 为兼容 PETSCII 的键值；仅非空姓名按 RETURN 确认时返回 C=1。
; 接受字母、数字、空格；字母转成大写屏幕码，DEL 删除末格，最多八格。
; 其他编辑或无效按键返回 C=0；全空格姓名也不能确认。
highscore_name_key:
    cmp #HS_KEY_RETURN
    beq @confirm
    cmp #HS_KEY_DELETE
    beq @delete
    cmp #'a'
    bcc @uppercase
    cmp #'z'+1
    bcs @uppercase
    and #$df
@uppercase:
    cmp #'A'
    bcc @number
    cmp #'Z'+1
    bcs @ignore
    and #$3f
    bne @append
@number:
    cmp #TEXT_SPACE
    beq @append
    cmp #'0'
    bcc @ignore
    cmp #'9'+1
    bcs @ignore
@append:
    ldx highscore_name_length
    cpx #HIGHSCORE_NAME_LENGTH
    bcs @ignore
    sta highscore_name,x
    inc highscore_name_length
@ignore:
    clc
    rts
@delete:
    ldx highscore_name_length
    beq @ignore
    dex
    stx highscore_name_length
    lda #TEXT_SPACE
    sta highscore_name,x
    clc
    rts
@confirm:
    ldx #0
@check:
    lda highscore_name,x
    cmp #TEXT_SPACE
    bne @valid
    inx
    cpx #HIGHSCORE_NAME_LENGTH
    bne @check
    clc
    rts
@valid:
    sec
    rts

; === 3. 主流程：启动读档与游戏结束 ===
; 读档失败时显示提示：R 重试，FIRE 使用当前内存记录继续。
highscore_load_at_start:
    jsr highscore_storage_load
    bcc @done
@error:
    jsr prepare_modal_screen
    HS_TEXT hs_load_failed, 8*40+10, COLOR_RED
    HS_TEXT hs_load_help, 11*40+4, COLOR_WHITE
    HS_TEXT hs_retry_prompt, 15*40+4, COLOR_WHITE
    jsr enable_charset_display
    jsr highscore_wait_choice
    cmp #'R'
    bne @done
    jsr highscore_storage_load
    bcs @error
@done:
    rts

; 阅读入口：游戏和音乐停止后进入，输入期间 score_digits 不再变化。
; 能上榜才询问姓名；RETURN 确认后只插入一次，保存失败仍保留内存成绩。
run_highscore_game_over:
    jsr highscore_find_rank
    lda highscore_rank
    cmp #HS_NO_RANK
    beq @save_pending
    jsr highscore_name_reset
    jsr show_highscore_name_screen
    jsr highscore_wait_key_release
@input:
    jsr highscore_read_key
    pha
    jsr highscore_wait_key_release
    pla
    jsr highscore_name_key
    bcs @commit
    jsr highscore_render_name
    jmp @input
@commit:
    jsr highscore_insert
@save_pending:
    ; 即使本局没上榜，也尝试保存上次失败后留下的未保存记录。
    lda highscore_dirty
    beq @show
@save:
    jsr show_highscore_table
    HS_TEXT hs_saving, 21*40+4, COLOR_YELLOW
    jsr highscore_storage_save
    bcs @show
    ; 只有写盘和回读验证成功才清除 dirty；显示出排行榜不代表已经保存。
    lda #0
    sta highscore_dirty
@show:
    jsr show_highscore_table
    jsr highscore_wait_choice
    cmp #'R'
    bne @done
    lda highscore_dirty
    bne @save
    jmp @show
@done:
    rts

; R 返回重试选择；FIRE 必须经过“松开 -> 按下 -> 松开”，避免按键穿透界面。
highscore_wait_choice:
    jsr highscore_wait_key_release
@release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @release
    lda #0
    sta highscore_last_key
@wait:
    jsr wait_for_frame
    jsr highscore_scan_key
    cmp highscore_last_key
    beq @fire
    sta highscore_last_key
    cmp #'R'
    beq @retry
@fire:
    jsr action_button_pressed
    bcc @wait
@fire_release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @fire_release
    lda #0
    rts
@retry:
    jsr highscore_wait_key_release
    lda #'R'
    rts

; === 4. 键盘输入：直接扫描 CIA1，不依赖系统屏幕编辑器 ===
; 按帧等待输入；调用者每次读到按键后等待松开，按住不会每帧重复录入。
highscore_read_key:
    jsr wait_for_frame
    jsr highscore_scan_key
    beq highscore_read_key
    rts

highscore_wait_key_release:
    jsr wait_for_frame
    jsr highscore_scan_key
    bne highscore_wait_key_release
    rts

; PA 每次拉低一行，PB 读取八列；未选行时已被 paddle/FIRE 拉低的输入线要屏蔽。
; 键位顺序对应 VICE C128/gtk3_sym.vkm；忽略 Shift，不扫描完整扩展键盘。
; 无支持的按键返回 A=0。扫描后恢复端口及方向寄存器，避免影响共用 CIA1 的 paddle。
highscore_scan_key:
    lda CIA1_PORT_A
    sta highscore_saved_pa
    lda CIA1_PORT_B
    sta highscore_saved_pb
    lda CIA1_DDR_A
    sta highscore_saved_ddra
    lda CIA1_DDR_B
    sta highscore_saved_ddrb
    lda HS_EXT_KEYBOARD
    sta highscore_saved_ext
    lda #$ff
    sta HS_EXT_KEYBOARD
    sta CIA1_PORT_A
    sta CIA1_DDR_A
    lda #0
    sta CIA1_DDR_B
    sta highscore_key_result
    lda CIA1_PORT_B
    eor #$ff
    sta highscore_key_blocked
    ldx #0
    lda #$fe
    sta highscore_scan_mask
@row:
    lda highscore_scan_mask
    sta CIA1_PORT_A
    nop
    lda CIA1_PORT_B
    ora highscore_key_blocked
    sta highscore_scan_bits
    ldy #8
@bit:
    lsr highscore_scan_bits
    bcs @next
    lda highscore_keymap,x
    beq @next
    sta highscore_key_result
    jmp @restore
@next:
    inx
    dey
    bne @bit
    sec
    rol highscore_scan_mask
    cpx #64
    bne @row
@restore:
    lda #0
    sta CIA1_DDR_A
    lda highscore_saved_pa
    sta CIA1_PORT_A
    lda highscore_saved_pb
    sta CIA1_PORT_B
    lda highscore_saved_ddrb
    sta CIA1_DDR_B
    lda highscore_saved_ddra
    sta CIA1_DDR_A
    lda highscore_saved_ext
    sta HS_EXT_KEYBOARD
    lda highscore_key_result
    rts

; === 5. 界面绘制：姓名输入页与排行榜 ===
; prepare_modal_screen 切回文字字库并关闭游戏 sprite，不再显示受伤角色。
show_highscore_name_screen:
    jsr prepare_modal_screen
    HS_TEXT hs_qualified, 5*40+9, COLOR_YELLOW
    HS_TEXT hs_your_score, 8*40+9, COLOR_WHITE
    jsr highscore_copy_current_digits
    SET_WORD DEST_PTR, SCREEN_A+8*40+23
    jsr highscore_draw_digits
    HS_TEXT hs_rank_label, 10*40+15, COLOR_WHITE
    lda highscore_rank
    clc
    adc #TEXT_DIGIT_0+1
    STORE_SCREEN_PAIR 10*40+21
    lda #COLOR_YELLOW
    sta COLOR_RAM+10*40+21
    HS_TEXT hs_name_label, 12*40+16, COLOR_WHITE
    HS_TEXT hs_name_help, 16*40+5, COLOR_WHITE
    HS_TEXT hs_confirm_help, 18*40+8, COLOR_WHITE
    jsr highscore_render_name
    jmp enable_charset_display

; 横线只是输入位置的光标，不写入姓名缓冲区，也不会进入存档。
highscore_render_name:
    ldx #0
@cell:
    lda highscore_name,x
    STORE_SCREEN_PAIR 13*40+16,x
    lda #COLOR_YELLOW
    sta COLOR_RAM+13*40+16,x
    inx
    cpx #HIGHSCORE_NAME_LENGTH
    bne @cell
    ldx highscore_name_length
    cpx #HIGHSCORE_NAME_LENGTH
    bcs @done
    lda #TEXT_DASH
    STORE_SCREEN_PAIR 13*40+16,x
@done:
    rts

; 只读取内存排行榜来绘制，不修改排名；本局记录高亮。
; 根据 dirty 和读写状态显示“已保存”或失败提示，不能仅凭有记录就认为保存成功。
show_highscore_table:
    jsr prepare_modal_screen
    HS_TEXT hs_game_over, 3*40+15, COLOR_RED
    HS_TEXT hs_your_score, 5*40+9, COLOR_WHITE
    jsr highscore_copy_current_digits
    SET_WORD DEST_PTR, SCREEN_A+5*40+23
    jsr highscore_draw_digits
    HS_TEXT hs_best_score, 6*40+9, COLOR_WHITE
    ldx #SCORE_DIGIT_COUNT-1
@best:
    lda #0
    ldy highscore_count
    beq @best_store
    lda highscore_entries+HIGHSCORE_NAME_LENGTH,x
@best_store:
    sta highscore_display_digits,x
    dex
    bpl @best
    SET_WORD DEST_PTR, SCREEN_A+6*40+23
    jsr highscore_draw_digits
    HS_TEXT hs_top_five, 8*40+17, COLOR_CYAN
    lda #0
    sta highscore_work_row
@row:
    ldx highscore_work_row
    lda highscore_screen_low,x
    sta DEST_PTR
    lda highscore_screen_high,x
    sta DEST_PTR+1
    lda #COLOR_WHITE
    cpx highscore_rank
    bne @color
    lda #COLOR_YELLOW
@color:
    sta highscore_draw_color
    jsr highscore_output_pointers
    ldy #0
    lda #TEXT_SPACE
    cpx highscore_rank
    bne @marker
    lda #TEXT_GREATER
@marker:
    jsr highscore_put_cell
    iny
    txa
    clc
    adc #TEXT_DIGIT_0+1
    jsr highscore_put_cell
    iny
    lda #TEXT_PERIOD
    jsr highscore_put_cell
    ldy #4
    cpx highscore_count
    bcc @record
@empty:
    lda #TEXT_DASH
    jsr highscore_put_cell
    iny
    cpy #20
    bne @empty
    beq @next_row
@record:
    lda highscore_record_offsets,x
    tax
@name:
    lda highscore_entries,x
    jsr highscore_put_cell
    inx
    iny
    cpy #12
    bne @name
    ldy #14
@score:
    lda highscore_entries,x
    clc
    adc #TEXT_DIGIT_0
    jsr highscore_put_cell
    inx
    iny
    cpy #20
    bne @score
@next_row:
    inc highscore_work_row
    lda highscore_work_row
    cmp #HIGHSCORE_COUNT
    bne @row
    lda highscore_dirty
    beq @clean
    HS_TEXT hs_save_failed, 21*40+4, COLOR_RED
    HS_TEXT hs_retry_prompt, 23*40+4, COLOR_WHITE
    jmp enable_charset_display
@clean:
    lda highscore_storage_status
    cmp #2
    bne @saved
    HS_TEXT hs_load_failed, 21*40+10, COLOR_RED
    jmp @prompt
@saved:
    lda highscore_count
    beq @prompt
    HS_TEXT hs_saved, 21*40+13, COLOR_GREEN
@prompt:
    HS_TEXT hs_fire_prompt, 23*40+15, COLOR_WHITE
    jmp enable_charset_display

; === 6. 绘制辅助：数字转换与双屏幕写入 ===
highscore_copy_current_digits:
    ldx #SCORE_DIGIT_COUNT-1
@copy:
    lda score_digits,x
    sta highscore_display_digits,x
    dex
    bpl @copy
    rts

; 分数每位是 0..9，加上 TEXT_DIGIT_0 才是文字字库字符码；不能用游戏 HUD 的基址。
highscore_draw_digits:
    jsr highscore_output_pointers
    ldy #0
@digit:
    lda highscore_display_digits,y
    clc
    adc #TEXT_DIGIT_0
    jsr highscore_put_cell
    iny
    cpy #SCORE_DIGIT_COUNT
    bne @digit
    rts

; ROW_PTR 指向以 0 结尾的 ASCII 标签；这里只对菜单文字做屏幕码转换。
highscore_print:
    jsr highscore_output_pointers
    ldy #0
@char:
    lda (ROW_PTR),y
    beq @done
    and #$3f
    jsr highscore_put_cell
    iny
    bne @char
@done:
    rts

; DEST_PTR 指向屏幕 A，计算屏幕 B 与颜色 RAM 的对应地址。
; 两块屏幕写入相同内容，防止切换缓冲区时露出旧画面。
highscore_output_pointers:
    lda DEST_PTR
    sta NEXT_ROW_PTR
    sta COLOR_PTR
    lda DEST_PTR+1
    clc
    adc #>(SCREEN_B-SCREEN_A)
    sta NEXT_ROW_PTR+1
    lda DEST_PTR+1
    clc
    adc #>(COLOR_RAM-SCREEN_A)
    sta COLOR_PTR+1
    rts

highscore_put_cell:
    sta (DEST_PTR),y
    sta (NEXT_ROW_PTR),y
    lda highscore_draw_color
    sta (COLOR_PTR),y
    rts

; === 7. 查表、界面文字与状态变量 ===
; 记录偏移和屏幕行地址预先列出，运行时不必计算乘法。
highscore_record_offsets: .byte 0,14,28,42,56
highscore_screen_low:
    .repeat HIGHSCORE_COUNT,I
    .byte <(SCREEN_A+(10+I*2)*40+9)
    .endrepeat
highscore_screen_high:
    .repeat HIGHSCORE_COUNT,I
    .byte >(SCREEN_A+(10+I*2)*40+9)
    .endrepeat
; 基础 8x8 键盘矩阵查表；0 表示该键不参与姓名输入。
highscore_keymap:
    .byte HS_KEY_DELETE,HS_KEY_RETURN,0,0,0,0,0,0
    .byte "3WA4ZSE",0
    .byte "5RD6CFTX"
    .byte "7YG8BHUV"
    .byte "9IJ0MKON"
    .byte 0,"PL",0,0,0,0,0
    .byte 0,0,0,0,0,0,0,0
    .byte "1",0,0,"2 ",0,"Q",0

hs_game_over: .asciiz "GAME OVER"
hs_your_score: .asciiz "YOUR SCORE"
hs_best_score: .asciiz "BEST SCORE"
hs_top_five: .asciiz "TOP 5"
hs_qualified: .asciiz "YOU MADE THE TOP 5"
hs_rank_label: .asciiz "RANK:"
hs_name_label: .asciiz "NAME:"
hs_name_help: .asciiz "A-Z  0-9  SPACE  DEL TO ERASE"
hs_confirm_help: .asciiz "RETURN TO CONFIRM NAME"
hs_saved: .asciiz "SCORES SAVED"
hs_saving: .asciiz "SAVING SCORES...                "
hs_save_failed: .asciiz "SAVE FAILED - SCORES NOT ON DISK"
hs_load_failed: .asciiz "SCORE LOAD FAILED"
hs_load_help: .asciiz "CHECK DISK 8. FIRE USES RAM ONLY."
hs_retry_prompt: .asciiz "R RETRY / FIRE CONTINUE"
hs_fire_prompt: .asciiz "PRESS FIRE"

; entries/count 是要存盘的成绩；rank/name/work 是本次界面的工作状态。
; dirty 跨局保留，直到上层确认保存成功才清除。
highscore_entries: .res HIGHSCORE_TABLE_SIZE,0
highscore_count: .byte 0
highscore_rank: .byte HS_NO_RANK
highscore_dirty: .byte 0
highscore_name: .res HIGHSCORE_NAME_LENGTH,TEXT_SPACE
highscore_name_length: .byte 0
highscore_work_row: .byte 0
highscore_work_offset: .byte 0
highscore_draw_color: .byte COLOR_WHITE
highscore_display_digits: .res SCORE_DIGIT_COUNT,0
highscore_last_key: .byte 0
highscore_saved_pa: .byte 0
highscore_saved_pb: .byte 0
highscore_saved_ddra: .byte 0
highscore_saved_ddrb: .byte 0
highscore_saved_ext: .byte 0
highscore_scan_mask: .byte 0
highscore_scan_bits: .byte 0
highscore_key_blocked: .byte 0
highscore_key_result: .byte 0
.delmacro HS_TEXT
