; Modal-only TOP 5. Names are screen codes; scores are six unpacked digits.
; This segment is outside the packed VIC graphics/music address ranges.
.segment "HIGHSCORES"

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

; Lookup only: equal scores stay ahead of the new entry.
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
    lda #1
    sta highscore_dirty
@done:
    rts

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

; A = PETSCII-compatible key. C=1 only for a nonblank RETURN confirmation.
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

; Disk failures are visible before starting, with R to retry / FIRE to continue.
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
    lda highscore_dirty
    beq @show
@save:
    jsr show_highscore_table
    HS_TEXT hs_saving, 21*40+4, COLOR_YELLOW
    jsr highscore_storage_save
    bcs @show
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

; R retries only; FIRE requires a full release/press/release cycle.
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

; Poll the base keyboard matrix with IRQs stopped; no screen-editor IRQ needed.
; Ignore PB lines already held low with no row selected (paddle/fire inputs).
; The matrix order matches VICE C128/gtk3_sym.vkm. Shift is deliberately ignored.
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

highscore_copy_current_digits:
    ldx #SCORE_DIGIT_COUNT-1
@copy:
    lda score_digits,x
    sta highscore_display_digits,x
    dex
    bpl @copy
    rts

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

; ROW_PTR = zero-terminated ASCII label; screen conversion is local to menus.
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

highscore_record_offsets: .byte 0,14,28,42,56
highscore_screen_low:
    .repeat HIGHSCORE_COUNT,I
    .byte <(SCREEN_A+(10+I*2)*40+9)
    .endrepeat
highscore_screen_high:
    .repeat HIGHSCORE_COUNT,I
    .byte >(SCREEN_A+(10+I*2)*40+9)
    .endrepeat
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
