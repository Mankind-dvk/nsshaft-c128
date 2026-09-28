; 排行榜底层：通过 C128 KERNAL 顺序文件接口保存，不调用 BASIC 或屏幕编辑器。
; 建议先看 highscore_storage_save/load，其余是环境保护、文件操作和校验辅助函数。
; 分区：格式 / 启动快照 / 读写主流程 / 环境切换 / 通道 / 文件读写 / 校验 / 数据。
; bootstrap 在游戏使用零页之前调用一次；load/save 仅在音乐停止、CIA IRQ 关闭后调用。
; A/X/Y 可被改写；返回 C=0 成功、C=1 失败。未找到有效文件时，不覆盖内存成绩。
; status：0=成功，1=首次运行无文件，2=读取失败，3=保存失败；有效备份也算读取成功。
;
; 两份 SEQ 文件各 80 字节，A/B 交替写入，绝不先删掉当前有效的那份。
; 保存成功要求：关闭文件、DOS 状态为 00、重新读取并核对全部内容。
; 文件偏移（不包含 PRG 加载地址）：
;   0..3  "NSHS" 标识       4       格式版本（1）
;   5     有效记录数         6..7    保存序号，低字节在前
;   8..77 五条成绩           78..79  CRC，低字节在前
; CRC 只覆盖 0..77，用于发现损坏，不是加密；A/B 也不能保证整张磁盘绝对抗掉电。

.segment "HIGHSCORES"

; === 1. 文件格式常量与 KERNAL 入口地址 ===
HS_DISK_BYTES = 80
HS_DISK_CRC_OFFSET = 78
HS_DISK_RECORD_OFFSET = 8
; 14/15 是程序使用的逻辑文件号，不是磁盘驱动器的设备号。
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

; === 2. 启动快照：保留系统原有的零页工作区 ===
; 必须在游戏占用零页前调用，且先映射本段 RAM，否则 BASIC ROM 可能遮住代码。
; 从 $ba 沿用加载程序的设备号；只接受 8..30，否则保持默认设备 8。
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

; === 3. 读写主流程：先看这部分，再按需追踪下方辅助函数 ===
; 读取：分别验证 A/B，选择较新的有效文件；另一份缺失或损坏不妨碍使用好的一份。
; 只有通过完整验证的候选文件，才能替换内存排行榜。
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
    ; 用 16 位序号差判断新旧，支持 $ffff 回到 $0000。
    ; 前提是序号相差小于 $8000；正常交替保存时，两份有效记录只差一代。
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
    ; DOS 62 是文件不存在，首次运行很正常；与通信失败、格式损坏区分处理。
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

; 保存：只替换非当前有效的槽位（首次写 A），保留上一份有效记录。
; 顺序：删目标旧文件 -> 打包 -> 写入并关闭 -> 回读校验 -> 更新有效槽位和序号。
; highscore_dirty 由上层维护；这里失败返回 C=1，不清除未保存标记。
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
    ; CRC 正确还不够：必须与本次序号、条数及全部成绩一致，防止误认旧文件为新存档。
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

; 无论成功失败都先恢复游戏环境，再用进位标志 C 返回结果。
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

; === 4. 环境切换：游戏与 KERNAL 分别保留自己的零页 ===
; 暂存游戏的映射、端口、速度及 CIA 配置，恢复系统零页，磁盘通信期间强制 1 MHz。
; 两套零页快照不能混用，否则系统文件变量和游戏指针会互相覆盖。
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
    ; SETBNK 参数是 RAM bank 编号，不是直接写入 MMU 的配置值。
    lda #0
    tax
    jsr HS_K_SETBNK
    lda #0
    jsr HS_K_SETMSG
    lda #0
    sta hs_disk_command_open
    sta hs_disk_data_open
    rts

; 留下 KERNAL 更新后的零页供下次读写，再恢复游戏零页和硬件配置。
; 这不是游戏运行中任意时刻可用的现场恢复：不保证定时器相位原样连续，CIA IRQ 仍关闭。
hs_disk_leave:
    ; 上层已经停止音乐；返回时保持两颗 CIA 的中断禁用，并清除挂起状态。
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

; === 5. KERNAL 通道与驱动器状态 ===
; 次级地址 15 用于 DOS 命令/状态，次级地址 2 用于文件内容；不同于物理设备号。
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
    ; 先读走开机或上次操作的状态，后续才能拿到本次操作的新状态。
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

; hs_disk_error 返回十进制 DOS 状态码，$ff 表示 IEC 通信失败或无效响应。
; 这里只把 00 作为成功；删除文件的调用者另外接受 01（FILES SCRATCHED）。
; 解析前两位 ASCII 数字，再读到回车清掉本行；尾部最多读 64 字符，避免无休止读取。
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

; === 6. 文件操作与打包：每个辅助函数只负责一个步骤 ===
; 根据目标槽位同时修改文件名和删除命令中的 A/B 字母。
hs_disk_set_slot_names:
    lda hs_disk_slot
    clc
    adc #'A'
    sta hs_disk_filename_slot
    sta hs_disk_scratch_slot_char
    rts

; 文件名中的 S 表示 SEQ；模式字符由读写调用者设置为 R 或 W。
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

; 必须恰好读到 80 字节：结束标志 EOI（$40）只能随最后一字节到达。
; 提前结束或还有多余内容都拒绝；READST 的其他状态位作为通信错误处理。
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

; 仅删除 save 选出的非当前槽位，绝不先删当前有效备份。
; DOS 01（FILES SCRATCHED）表示删除完成，此处也算成功。
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

; 组装下一代存档，但暂不更新已确认的序号；验证成功后才提交。
; 固定复制五条记录，实际有效条数由 count 决定；最后补上 CRC。
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

; CHROUT 成功只说明发送过程未报错；关闭文件后还要检查驱动器 DOS 状态。
; 上层 save 随后重新打开并核对内容，不能在这里直接认定永久保存成功。
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

; === 7. 存档验证：校验值正确之外，还要确认内容合法 ===
; 依次检查 CRC、标识、版本、条数、姓名和数字，再检查排名顺序。
; 姓名允许屏幕码 1..26、数字 48..57、空格 32，至少有一个非空格字符。
; 只检查 count 条有效记录；未使用的槽位允许保持全零。
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

; 相邻成绩必须从高到低，同分合法；查排名算法依赖这个顺序，因此读档时必须检查。
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

; CRC-16/CCITT-FALSE：初值 $ffff，多项式 $1021，末尾不再异或。
; 每字节先异或到高八位，再移位八次；移出 1 时异或多项式。末尾两个 CRC 字节不参与。
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

; === 8. 文件名、状态与工作缓冲区 ===
; 状态：0=成功，1=没有旧存档，2=读取失败，3=保存失败。
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
; 槽位 0=A、1=B、$ff=尚无有效存档。当前有效槽位与本次目标槽位分开，
; 防止写入失败时把未经验证的文件当成当前存档。
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
; 两份各 254 字节的快照，隔离 KERNAL 和游戏在 $02..$ff 的工作状态。
; $00/$01 是 CPU 端口寄存器，单独保存恢复，不按普通 RAM 复制。
hs_disk_kernal_zp: .res HS_DISK_ZP_BYTES,0
hs_disk_game_zp: .res HS_DISK_ZP_BYTES,0
hs_disk_buffer: .res HS_DISK_BYTES,0
