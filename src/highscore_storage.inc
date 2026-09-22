; Native C128 sequential-file persistence. Call bootstrap once, before the
; game uses zero page. Call load/save only with music stopped (CIA IRQs disabled).
; Both routines preserve game ZP, mapping, CPU ports and CIA port/timer setup.
; A/X/Y are scratch. C=0 success, C=1 failure. load leaves the table unchanged
; unless a fully validated slot was found. status: 0 OK, 1 first run (no files),
; 2 load failure, 3 save failure. A valid backup still counts as a successful load.
;
; Each 80-byte SEQ file holds NSHS, version, count, generation LE, 70 record
; bytes, CRC-16/CCITT LE. A/B alternate; the current valid slot is NEVER scratched.
; Success requires CLOSE, DOS status 00, then exact read-back verification.
; No LOAD/SAVE headers and no BASIC/screen-editor calls are involved.

.segment "HIGHSCORES"

HS_DISK_BYTES = 80
HS_DISK_CRC_OFFSET = 78
HS_DISK_RECORD_OFFSET = 8
HS_DISK_FILE = 14
HS_DISK_COMMAND = 15
HS_DISK_ZP_FIRST = $02
HS_DISK_ZP_BYTES = $fe
HS_K_SETBNK = $ff68
HS_K_SETMSG = $ff90
HS_K_READST = $ffb7
HS_K_SETLFS = $ffba
HS_K_SETNAM = $ffbd
HS_K_OPEN = $ffc0
HS_K_CLOSE = $ffc3
HS_K_CHKIN = $ffc6
HS_K_CHKOUT = $ffc9
HS_K_CLRCHN = $ffcc
HS_K_CHRIN = $ffcf
HS_K_CHROUT = $ffd2

highscore_storage_bootstrap:
    ldx #0
@copy:
    lda HS_DISK_ZP_FIRST,x
    sta hs_disk_kernal_zp,x
    inx
    cpx #HS_DISK_ZP_BYTES
    bne @copy
    lda $00
    sta hs_disk_boot_cpu_ddr
    lda $01
    sta hs_disk_boot_cpu_port
    lda $ba
    cmp #8
    bcc @default_device
    cmp #31
    bcs @default_device
    sta highscore_storage_device
@default_device:
    rts

highscore_storage_load:
    php
    sei
    cld
    jsr hs_disk_enter
    lda #$ff
    sta hs_disk_active_slot
    lda #0
    sta hs_disk_generation
    sta hs_disk_generation+1
    sta hs_disk_load_error
    sta hs_disk_slot
    jsr hs_disk_open_command
    bcs @open_failed
@slot:
    jsr hs_disk_read_slot
    bcs @failed_slot
    lda hs_disk_active_slot
    cmp #$ff
    beq @accept
    ; Modular 16-bit sequence ordering remains valid across $ffff -> $0000.
    lda hs_disk_buffer+6
    sec
    sbc hs_disk_generation
    sta hs_disk_difference
    lda hs_disk_buffer+7
    sbc hs_disk_generation+1
    bmi @next
    ora hs_disk_difference
    beq @next
@accept:
    lda hs_disk_slot
    sta hs_disk_active_slot
    lda hs_disk_buffer+6
    sta hs_disk_generation
    lda hs_disk_buffer+7
    sta hs_disk_generation+1
    lda hs_disk_buffer+5
    sta highscore_count
    ldx #69
@copy:
    lda hs_disk_buffer+HS_DISK_RECORD_OFFSET,x
    sta highscore_entries,x
    dex
    bpl @copy
    jmp @next
@failed_slot:
    lda hs_disk_error
    cmp #62
    beq @next
    lda #1
    sta hs_disk_load_error
@next:
    inc hs_disk_slot
    lda hs_disk_slot
    cmp #2
    bcc @slot
    jsr hs_disk_close_command
    lda hs_disk_active_slot
    cmp #$ff
    beq @no_valid_slot
    lda #0
    sta highscore_storage_status
    jmp hs_disk_success
@no_valid_slot:
    lda hs_disk_load_error
    bne @open_failed
    lda #1
    sta highscore_storage_status
    jmp hs_disk_success
@open_failed:
    jsr hs_disk_close_command
    lda #2
    sta highscore_storage_status
    jmp hs_disk_failure

highscore_storage_save:
    php
    sei
    cld
    jsr hs_disk_enter
    lda hs_disk_active_slot
    cmp #$ff
    bne @alternate
    lda #1
@alternate:
    eor #1
    sta hs_disk_slot
    jsr hs_disk_open_command
    bcs @failed
    jsr hs_disk_scratch_slot
    bcs @failed
    jsr hs_disk_build_buffer
    jsr hs_disk_write_slot
    bcs @failed
    jsr hs_disk_read_slot
    bcs @failed
    lda hs_disk_expected_generation
    cmp hs_disk_buffer+6
    bne @failed
    lda hs_disk_expected_generation+1
    cmp hs_disk_buffer+7
    bne @failed
    lda highscore_count
    cmp hs_disk_buffer+5
    bne @failed
    ldx #69
@verify:
    lda highscore_entries,x
    cmp hs_disk_buffer+HS_DISK_RECORD_OFFSET,x
    bne @failed
    dex
    bpl @verify
    lda hs_disk_slot
    sta hs_disk_active_slot
    lda hs_disk_buffer+6
    sta hs_disk_generation
    lda hs_disk_buffer+7
    sta hs_disk_generation+1
    jsr hs_disk_close_command
    lda #0
    sta highscore_storage_status
    jmp hs_disk_success
@failed:
    jsr hs_disk_close_data
    jsr hs_disk_close_command
    lda #3
    sta highscore_storage_status
    jmp hs_disk_failure

hs_disk_success:
    jsr hs_disk_leave
    plp
    clc
    rts
hs_disk_failure:
    jsr hs_disk_leave
    plp
    sec
    rts

hs_disk_enter:
    lda MMU_CONFIG
    sta hs_disk_saved_mmu
    lda #MMU_CONFIG_BANK0_IO
    sta MMU_CONFIG
    lda $00
    sta hs_disk_saved_cpu_ddr
    lda $01
    sta hs_disk_saved_cpu_port
    lda hs_disk_boot_cpu_ddr
    sta $00
    lda hs_disk_boot_cpu_port
    sta $01
    lda VIC_CPU_SPEED
    sta hs_disk_saved_speed
    and #$fe
    sta VIC_CPU_SPEED
    lda $d505
    sta hs_disk_saved_mcr
    lda $dc0e
    sta hs_disk_saved_cra
    lda $dc0f
    sta hs_disk_saved_crb
    lda $dd00
    sta hs_disk_saved_cia2_port
    lda $dd02
    sta hs_disk_saved_cia2_ddr
    ora #$3f
    sta $dd02
    ldx #7
@cia:
    lda $dc00,x
    sta hs_disk_saved_cia1,x
    dex
    bpl @cia
    ldx #0
@zp:
    lda HS_DISK_ZP_FIRST,x
    sta hs_disk_game_zp,x
    lda hs_disk_kernal_zp,x
    sta HS_DISK_ZP_FIRST,x
    inx
    cpx #HS_DISK_ZP_BYTES
    bne @zp
    ; SETBNK uses bank numbers, NOT literal MMU configurations.
    lda #0
    tax
    jsr HS_K_SETBNK
    lda #0
    jsr HS_K_SETMSG
    lda #0
    sta hs_disk_command_open
    sta hs_disk_data_open
    rts

hs_disk_leave:
    ; Music is stopped at every public call; leave both CIA IRQ masks disabled.
    lda #$7f
    sta $dc0d
    sta $dd0d
    lda $dc0d
    lda $dd0d
    lda #0
    sta $dc0e
    sta $dc0f
    ldx #7
@cia:
    lda hs_disk_saved_cia1,x
    sta $dc00,x
    dex
    bpl @cia
    lda hs_disk_saved_cra
    sta $dc0e
    lda hs_disk_saved_crb
    sta $dc0f
    lda hs_disk_saved_cia2_port
    sta $dd00
    lda hs_disk_saved_cia2_ddr
    sta $dd02
    lda hs_disk_saved_mcr
    sta $d505
    lda hs_disk_saved_speed
    sta VIC_CPU_SPEED
    ldx #0
@zp:
    lda HS_DISK_ZP_FIRST,x
    sta hs_disk_kernal_zp,x
    lda hs_disk_game_zp,x
    sta HS_DISK_ZP_FIRST,x
    inx
    cpx #HS_DISK_ZP_BYTES
    bne @zp
    lda hs_disk_saved_cpu_port
    sta $01
    lda hs_disk_saved_cpu_ddr
    sta $00
    lda hs_disk_saved_mmu
    sta MMU_CONFIG
    rts

hs_disk_open_command:
    lda #0
    jsr HS_K_SETNAM
    lda #HS_DISK_COMMAND
    ldx highscore_storage_device
    ldy #15
    jsr HS_K_SETLFS
    jsr HS_K_OPEN
    bcs @failed
    lda #1
    sta hs_disk_command_open
    ; Consume power-on/prior DOS status. The next operation gets fresh status.
    jsr hs_disk_dos_status
    lda hs_disk_error
    cmp #$ff
    beq @failed
    clc
    rts
@failed:
    sec
    rts

hs_disk_close_command:
    lda hs_disk_command_open
    beq @done
    jsr HS_K_CLRCHN
    lda #HS_DISK_COMMAND
    jsr HS_K_CLOSE
    lda #0
    sta hs_disk_command_open
@done:
    rts

hs_disk_close_data:
    lda hs_disk_data_open
    beq @done
    jsr HS_K_CLRCHN
    lda #HS_DISK_FILE
    jsr HS_K_CLOSE
    lda #0
    sta hs_disk_data_open
@done:
    rts

; Return decimal DOS status in hs_disk_error, $ff for IEC/invalid response.
; Only 00 is C=0. Scratch separately also accepts 01 FILES SCRATCHED.
hs_disk_dos_status:
    lda #$ff
    sta hs_disk_error
    ldx #HS_DISK_COMMAND
    jsr HS_K_CHKIN
    bcs @bad
    jsr HS_K_CHRIN
    cmp #'0'
    bcc @bad
    cmp #'9'+1
    bcs @bad
    sec
    sbc #'0'
    asl
    sta hs_disk_status_tens
    asl
    asl
    clc
    adc hs_disk_status_tens
    sta hs_disk_status_tens
    jsr HS_K_CHRIN
    cmp #'0'
    bcc @bad
    cmp #'9'+1
    bcs @bad
    sec
    sbc #'0'
    clc
    adc hs_disk_status_tens
    sta hs_disk_status_code
    lda #64
    sta hs_disk_index
@drain:
    jsr HS_K_CHRIN
    cmp #13
    beq @received
    jsr HS_K_READST
    bne @bad
    dec hs_disk_index
    bne @drain
    beq @bad
@received:
    jsr HS_K_READST
    and #$bf
    bne @bad
    jsr HS_K_CLRCHN
    lda hs_disk_status_code
    sta hs_disk_error
    beq @ok
    sec
    rts
@bad:
    jsr HS_K_CLRCHN
    sec
    rts
@ok:
    clc
    rts

hs_disk_set_slot_names:
    lda hs_disk_slot
    clc
    adc #'A'
    sta hs_disk_filename_slot
    sta hs_disk_scratch_slot_char
    rts

hs_disk_open_data:
    jsr hs_disk_set_slot_names
    lda #hs_disk_filename_end-hs_disk_filename
    ldx #<hs_disk_filename
    ldy #>hs_disk_filename
    jsr HS_K_SETNAM
    lda #HS_DISK_FILE
    ldx highscore_storage_device
    ldy #2
    jsr HS_K_SETLFS
    jsr HS_K_OPEN
    bcs @failed
    lda #1
    sta hs_disk_data_open
    jmp hs_disk_dos_status
@failed:
    lda #$ff
    sta hs_disk_error
    sec
    rts

hs_disk_read_slot:
    lda #'R'
    sta hs_disk_filename_mode
    jsr hs_disk_open_data
    bcs @failed
    ldx #HS_DISK_FILE
    jsr HS_K_CHKIN
    bcs @invalid
    lda #0
    sta hs_disk_index
@read:
    jsr HS_K_CHRIN
    ldx hs_disk_index
    sta hs_disk_buffer,x
    jsr HS_K_READST
    sta hs_disk_read_status
    and #$bf
    bne @invalid
    inc hs_disk_index
    lda hs_disk_index
    cmp #HS_DISK_BYTES
    beq @last
    lda hs_disk_read_status
    bne @invalid
    beq @read
@last:
    lda hs_disk_read_status
    cmp #$40
    bne @invalid
    jsr hs_disk_close_data
    jsr hs_disk_validate_buffer
    bcs @invalid
    clc
    rts
@invalid:
    lda #$fe
    sta hs_disk_error
@failed:
    jsr hs_disk_close_data
    sec
    rts

hs_disk_scratch_slot:
    jsr hs_disk_set_slot_names
    ldx #HS_DISK_COMMAND
    jsr HS_K_CHKOUT
    bcs @failed
    lda #0
    sta hs_disk_index
@write:
    ldx hs_disk_index
    lda hs_disk_scratch,x
    jsr HS_K_CHROUT
    jsr HS_K_READST
    bne @failed
    inc hs_disk_index
    lda hs_disk_index
    cmp #hs_disk_scratch_end-hs_disk_scratch
    bcc @write
    jsr HS_K_CLRCHN
    jsr hs_disk_dos_status
    bcc @done
    lda hs_disk_error
    cmp #1
    bne @failed
    clc
@done:
    rts
@failed:
    jsr HS_K_CLRCHN
    sec
    rts

hs_disk_build_buffer:
    ldx #3
@magic:
    lda hs_disk_magic,x
    sta hs_disk_buffer,x
    dex
    bpl @magic
    lda #1
    sta hs_disk_buffer+4
    lda highscore_count
    sta hs_disk_buffer+5
    lda hs_disk_generation
    clc
    adc #1
    sta hs_disk_buffer+6
    sta hs_disk_expected_generation
    lda hs_disk_generation+1
    adc #0
    sta hs_disk_buffer+7
    sta hs_disk_expected_generation+1
    ldx #69
@records:
    lda highscore_entries,x
    sta hs_disk_buffer+HS_DISK_RECORD_OFFSET,x
    dex
    bpl @records
    jsr hs_disk_crc16
    lda hs_disk_crc
    sta hs_disk_buffer+HS_DISK_CRC_OFFSET
    lda hs_disk_crc+1
    sta hs_disk_buffer+HS_DISK_CRC_OFFSET+1
    rts

hs_disk_write_slot:
    lda #'W'
    sta hs_disk_filename_mode
    jsr hs_disk_open_data
    bcs @failed
    ldx #HS_DISK_FILE
    jsr HS_K_CHKOUT
    bcs @failed
    lda #0
    sta hs_disk_index
@write:
    ldx hs_disk_index
    lda hs_disk_buffer,x
    jsr HS_K_CHROUT
    jsr HS_K_READST
    bne @failed
    inc hs_disk_index
    lda hs_disk_index
    cmp #HS_DISK_BYTES
    bcc @write
    jsr hs_disk_close_data
    jmp hs_disk_dos_status
@failed:
    jsr hs_disk_close_data
    sec
    rts

; Reject malformed bytes and ascending scores, not just bad checksums.
hs_disk_validate_buffer:
    jsr hs_disk_crc16
    lda hs_disk_crc
    cmp hs_disk_buffer+HS_DISK_CRC_OFFSET
    bne @bad
    lda hs_disk_crc+1
    cmp hs_disk_buffer+HS_DISK_CRC_OFFSET+1
    bne @bad
    ldx #3
@magic:
    lda hs_disk_buffer,x
    cmp hs_disk_magic,x
    bne @bad
    dex
    bpl @magic
    lda hs_disk_buffer+4
    cmp #1
    bne @bad
    lda hs_disk_buffer+5
    cmp #6
    bcs @bad
    sta hs_disk_rows_left
    beq @ok
    ldx #HS_DISK_RECORD_OFFSET
@record:
    lda #0
    sta hs_disk_nonspace
    ldy #8
@name:
    lda hs_disk_buffer,x
    cmp #32
    beq @next_char
    inc hs_disk_nonspace
    cmp #1
    bcc @bad
    cmp #27
    bcc @next_char
    cmp #'0'
    bcc @bad
    cmp #'9'+1
    bcs @bad
@next_char:
    inx
    dey
    bne @name
    lda hs_disk_nonspace
    beq @bad
    ldy #6
@digit:
    lda hs_disk_buffer,x
    cmp #10
    bcs @bad
    inx
    dey
    bne @digit
    dec hs_disk_rows_left
    bne @record
    jmp hs_disk_validate_order
@ok:
    clc
    rts
@bad:
    sec
    rts

hs_disk_validate_order:
    lda hs_disk_buffer+5
    cmp #2
    bcc @ok
    sec
    sbc #1
    sta hs_disk_rows_left
    lda #HS_DISK_RECORD_OFFSET+8
    sta hs_disk_index
@pair:
    ldx hs_disk_index
    ldy #6
@digit:
    lda hs_disk_buffer,x
    cmp hs_disk_buffer+14,x
    bcc @bad
    bne @next_pair
    inx
    dey
    bne @digit
@next_pair:
    lda hs_disk_index
    clc
    adc #14
    sta hs_disk_index
    dec hs_disk_rows_left
    bne @pair
@ok:
    clc
    rts
@bad:
    sec
    rts

; CRC-16/CCITT-FALSE: initial $ffff, polynomial $1021, no final XOR.
hs_disk_crc16:
    lda #$ff
    sta hs_disk_crc
    sta hs_disk_crc+1
    ldx #0
@byte:
    lda hs_disk_buffer,x
    eor hs_disk_crc+1
    sta hs_disk_crc+1
    ldy #8
@bit:
    asl hs_disk_crc
    rol hs_disk_crc+1
    bcc @next_bit
    lda hs_disk_crc
    eor #$21
    sta hs_disk_crc
    lda hs_disk_crc+1
    eor #$10
    sta hs_disk_crc+1
@next_bit:
    dey
    bne @bit
    inx
    cpx #HS_DISK_CRC_OFFSET
    bcc @byte
    rts

highscore_storage_status: .byte 1
highscore_storage_device: .byte 8
hs_disk_magic: .byte "NSHS"
hs_disk_filename: .byte "0:NSSHAFT."
hs_disk_filename_slot: .byte "A"
    .byte ",S,"
hs_disk_filename_mode: .byte "R"
hs_disk_filename_end:
hs_disk_scratch: .byte "S0:NSSHAFT."
hs_disk_scratch_slot_char: .byte "A",13
hs_disk_scratch_end:
hs_disk_active_slot: .byte $ff
hs_disk_slot: .byte 0
hs_disk_generation: .word 0
hs_disk_expected_generation: .word 0
hs_disk_difference: .byte 0
hs_disk_load_error: .byte 0
hs_disk_error: .byte 0
hs_disk_command_open: .byte 0
hs_disk_data_open: .byte 0
hs_disk_status_tens: .byte 0
hs_disk_status_code: .byte 0
hs_disk_index: .byte 0
hs_disk_read_status: .byte 0
hs_disk_rows_left: .byte 0
hs_disk_nonspace: .byte 0
hs_disk_crc: .word 0
hs_disk_boot_cpu_ddr: .byte 0
hs_disk_boot_cpu_port: .byte 0
hs_disk_saved_mmu: .byte 0
hs_disk_saved_cpu_ddr: .byte 0
hs_disk_saved_cpu_port: .byte 0
hs_disk_saved_speed: .byte 0
hs_disk_saved_mcr: .byte 0
hs_disk_saved_cra: .byte 0
hs_disk_saved_crb: .byte 0
hs_disk_saved_cia2_port: .byte 0
hs_disk_saved_cia2_ddr: .byte 0
hs_disk_saved_cia1: .res 8,0
hs_disk_kernal_zp: .res HS_DISK_ZP_BYTES,0
hs_disk_game_zp: .res HS_DISK_ZP_BYTES,0
hs_disk_buffer: .res HS_DISK_BYTES,0
