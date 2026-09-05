; ---------------------------------------------------------------------------
; Quarter Slot：SID 三声道背景音乐
; ---------------------------------------------------------------------------
; 原始 MP3 约为 129 BPM。这里将它重编为适合真实 SID 的 128 个八分音符：
; 声道 1 使用脉冲波演奏主旋律，声道 2 使用三角波演奏低音，声道 3 在
; 锯齿波琶音和噪声鼓之间切换。它不是播放 MP3 采样，而是根据原曲节奏、
; 调性和段落重新构造的硬件 SID 编曲。
;
; 播放器位于 $2a00，处在字符集末尾与 sprite 数据之间。VIC-IIe 在主循环
; 帧同步线之前每个 PAL 帧调用一次 play_music_frame。基础档 44/512 与旧版
; 22/256 相同，约为 129 BPM；20/40/60/80 分时和滚动一起提升到
; 1.25/1.5/1.75/2 倍。IRQ 不使用零页，只改音乐状态并完整保存 A/X/Y，
; 因此不会干扰正在运行的平台滚动和碰撞代码。
;
; 公共入口：initialize_music、play_music_frame、stop_music。
;
; SID 控制寄存器的 bit 0 是 GATE：从 0 写到 1 会重新触发 ADSR 包络。这里
; 先写不带 GATE 的波形值，再写带 GATE 的值。频率表索引 0 表示休止，
; 1..61 表示音符；编曲数组只保存索引，不直接保存 16 位 SID 频率。
;
; English summary:
;   A three-voice SID arrangement reconstructed from Quarter Slot: pulse lead,
;   triangle bass, and shared saw/noise arpeggio-percussion voice. Raster IRQ 240
;   advances playback independently of the main loop. A score-controlled modulo-
;   512 phase accumulator matches the five world-scroll multipliers exactly. The
;   handler preserves A/X/Y, avoids shared zero-page pointers, acknowledges VIC,
;   and returns through the C128 KERNAL restore path. Pattern entries are
;   frequency-table indices.

.segment "MUSIC"

; EN: Clear SID/player state, configure instruments, and install raster IRQ 240.
initialize_music:
    ; 输出：清 SID/播放器状态，写三声道音色，安装并启用光栅 IRQ。
    ; SEI/CLI 包住向量和中断源配置，避免只写完向量低字节时就发生 IRQ。
    sei
    lda #0
    sta music_step
    sta music_tempo_accumulator_low
    sta music_tempo_accumulator_high
    sta music_gate_timer
    sta music_voice3_delay
    sta SID_FILTER_CUTOFF_LOW
    sta SID_FILTER_CUTOFF_HIGH
    sta SID_FILTER_CONTROL

    ; 主旋律使用 50% 占空比脉冲波。
    sta SID_VOICE_1_PULSE_LOW
    lda #$08
    sta SID_VOICE_1_PULSE_HIGH
    lda #$24
    sta SID_VOICE_1_ATTACK
    lda #$84
    sta SID_VOICE_1_SUSTAIN

    ; 低音采用快速起音的三角波，避免与主旋律争夺高频空间。
    lda #$03
    sta SID_VOICE_2_ATTACK
    lda #$a4
    sta SID_VOICE_2_SUSTAIN

    lda #$02
    sta SID_VOICE_3_ATTACK
    lda #$52
    sta SID_VOICE_3_SUSTAIN

    lda #$0f
    sta SID_VOLUME
    jsr music_trigger_current_step

    ; 游戏关闭 KERNAL/CIA 中断，只保留一个稳定的 VIC-IIe 音乐光栅 IRQ。
    lda #0
    sta VIC_IRQ_ENABLE
    lda #$7f
    sta CIA1_INTERRUPT_CONTROL
    sta CIA2_INTERRUPT_CONTROL
    lda CIA1_INTERRUPT_CONTROL
    lda CIA2_INTERRUPT_CONTROL

    ; C128 KERNAL 通过 $0314/$0315 分派 IRQ。硬件入口已经保存 A/X/Y 和
    ; MMU 配置，因此本处理器不能再调用原 KERNAL IRQ 服务，否则它会把
    ; VIC-IIe 光栅 IRQ 当成系统显示中断继续处理，造成 sprite 闪烁。
    lda #<music_raster_irq
    sta KERNAL_IRQ_VECTOR
    lda #>music_raster_irq
    sta KERNAL_IRQ_VECTOR+1
    ; 不要与 wait_for_frame 使用同一条光栅线，否则 IRQ 返回时主循环可能
    ; 已错过同步点，并无谓地再等待一帧。
    lda #MUSIC_IRQ_RASTER
    sta VIC_RASTER
    lda VIC_CONTROL_1
    and #%01111111
    sta VIC_CONTROL_1
    lda #1
    sta VIC_IRQ_STATUS
    sta VIC_IRQ_ENABLE
    cli
    rts

; EN: Acknowledge VIC, advance music, then use the KERNAL register/MMU restore path.
music_raster_irq:
    ; C128 KERNAL 的硬件入口已经压栈保存寄存器；本例程先向 $d019 写 1 确认
    ; 光栅 IRQ，再播放一帧，最后跳到 $ff33 统一恢复，而不是执行普通 RTS。
    lda #1
    sta VIC_IRQ_STATUS
    jsr play_music_frame
    jmp KERNAL_IRQ_RETURN

; 每帧先处理包络和声道 3 的鼓声延时，再用模 512 相位累加器推进八分音符。
; EN: Accumulate modulo-512 PAL tempo and trigger the next pattern event on wrap.
play_music_frame:
    ; 输入：无。输出：可能关闭上一音符 GATE、恢复琶音、推进 music_step。
    ; 低字节 ADC 的进位加到 high；high 达到 2 就减 2 并触发下一步。该 9 位
    ; 相位既保留原始 BPM，又能精确表示 1.25 和 1.75 两个倍率。
    lda music_voice3_delay
    beq @update_gate
    dec music_voice3_delay
    bne @update_gate
    jsr music_trigger_arp

@update_gate:
    lda music_gate_timer
    beq @update_tempo
    dec music_gate_timer
    bne @update_tempo
    lda #MUSIC_LEAD_WAVE
    sta SID_VOICE_1_CONTROL
    lda #MUSIC_BASS_WAVE
    sta SID_VOICE_2_CONTROL
    lda #MUSIC_ARP_WAVE
    sta SID_VOICE_3_CONTROL

@update_tempo:
    clc
    lda music_tempo_accumulator_low
    adc music_tempo_increment
    sta music_tempo_accumulator_low
    lda music_tempo_accumulator_high
    adc #0
    cmp #MUSIC_TEMPO_WRAP_HIGH
    bcc @store_tempo_high
    sbc #MUSIC_TEMPO_WRAP_HIGH
    sta music_tempo_accumulator_high

    inc music_step
    lda music_step
    and #MUSIC_PATTERN_MASK
    sta music_step
    jsr music_trigger_current_step
@done:
    rts

@store_tempo_high:
    sta music_tempo_accumulator_high
    rts

; EN: Disable the raster source, restore the default IRQ vector, and silence SID.
stop_music:
    ; 关闭 IRQ 和三个 GATE，并把总音量设为 0。保持 SEI，后续 new_game 的
    ; initialize_music 会重新安装 IRQ 并执行 CLI。
    sei
    lda #0
    sta VIC_IRQ_ENABLE
    sta SID_VOICE_1_CONTROL
    sta SID_VOICE_2_CONTROL
    sta SID_VOICE_3_CONTROL
    sta SID_VOLUME
    sta music_gate_timer
    sta music_voice3_delay
    lda #1
    sta VIC_IRQ_STATUS
    rts

; 同一个步骤同时触发旋律和低音。鼓点只占用声道 3 的前两帧，随后恢复
; 当前琶音，因此在三个硬件声道内仍能保留节奏感和和声轮廓。
; EN: Dispatch lead, bass, arpeggio, and drum events for music_step.
music_trigger_current_step:
    ; 输入：music_step=0..127。输出：触发声道 1/2，并让声道 3 播鼓或琶音。
    ; drum_pattern 只有 8 步，所以 AND #7 等价于对 8 取模。
    ldx music_step
    lda music_lead_pattern,x
    jsr music_set_voice_1

    ldx music_step
    lda music_bass_pattern,x
    jsr music_set_voice_2

    lda #9
    sta music_gate_timer

    lda music_step
    and #7
    tax
    lda music_drum_pattern,x
    beq @play_arp_now
    jsr music_trigger_drum
    lda #2
    sta music_voice3_delay
    rts

@play_arp_now:
    lda #0
    sta music_voice3_delay
    jmp music_trigger_arp

; EN: Load voice-1 frequency and retrigger the pulse-wave ADSR gate.
music_set_voice_1:
    ; 输入：A=音符编号，0 为休止。先 TAX 保存索引，再关闭 GATE、查低/高
    ; 频率字节，最后重新打开脉冲波 GATE。
    tax
    lda #MUSIC_LEAD_WAVE
    sta SID_VOICE_1_CONTROL
    cpx #0
    beq @done
    lda music_frequency_low,x
    sta SID_VOICE_1_FREQ_LOW
    lda music_frequency_high,x
    sta SID_VOICE_1_FREQ_HIGH
    lda #MUSIC_LEAD_GATE
    sta SID_VOICE_1_CONTROL
@done:
    rts

; EN: Load voice-2 frequency and retrigger the triangle-wave ADSR gate.
music_set_voice_2:
    ; 与声道 1 相同，但写入 SID 声道 2 并使用三角波控制值。
    tax
    lda #MUSIC_BASS_WAVE
    sta SID_VOICE_2_CONTROL
    cpx #0
    beq @done
    lda music_frequency_low,x
    sta SID_VOICE_2_FREQ_LOW
    lda music_frequency_high,x
    sta SID_VOICE_2_FREQ_HIGH
    lda #MUSIC_BASS_GATE
    sta SID_VOICE_2_CONTROL
@done:
    rts

; EN: Trigger voice 3 as a short sawtooth arpeggio note.
music_trigger_arp:
    ; 从当前步骤读取琶音音符；鼓声占用结束后也会由 play_music_frame 调用，
    ; 使共享的第三声道恢复和声。
    ldx music_step
    lda music_arp_pattern,x
    tax
    lda #MUSIC_ARP_WAVE
    sta SID_VOICE_3_CONTROL
    cpx #0
    beq @done
    lda #$02
    sta SID_VOICE_3_ATTACK
    lda #$52
    sta SID_VOICE_3_SUSTAIN
    lda music_frequency_low,x
    sta SID_VOICE_3_FREQ_LOW
    lda music_frequency_high,x
    sta SID_VOICE_3_FREQ_HIGH
    lda #MUSIC_ARP_GATE
    sta SID_VOICE_3_CONTROL
@done:
    rts

; A=1 为低频噪声鼓，A=2 为较亮的军鼓。两者都使用极短包络。
; EN: Trigger voice 3 as a short noise drum, temporarily replacing arpeggio use.
music_trigger_drum:
    ; 输入：A=1 低鼓、A=2 军鼓。X 保存类型；二者共享噪声波形和短包络，
    ; 只用不同频率高字节改变噪声的明暗。
    tax
    lda #MUSIC_NOISE_WAVE
    sta SID_VOICE_3_CONTROL
    lda #$04
    sta SID_VOICE_3_ATTACK
    lda #$00
    sta SID_VOICE_3_SUSTAIN
    cpx #1
    bne @snare
    lda #$00
    sta SID_VOICE_3_FREQ_LOW
    lda #$10
    sta SID_VOICE_3_FREQ_HIGH
    jmp @gate
@snare:
    lda #$00
    sta SID_VOICE_3_FREQ_LOW
    lda #$50
    sta SID_VOICE_3_FREQ_HIGH
@gate:
    lda #MUSIC_NOISE_GATE
    sta SID_VOICE_3_CONTROL
    rts

; 音符编号 1..61 对应 C1..C6。频率值按 PAL 985248 Hz 时钟计算；编号 0
; 保留为空音符，使编曲表以后可以直接插入休止符。
music_frequency_low:
    .byte $00
    .byte $2d,$4e,$71,$96,$be,$e7,$14,$42,$74,$a9,$e0,$1b,$5a,$9c,$e2,$2d
    .byte $7b,$cf,$27,$85,$e8,$51,$c1,$37,$b4,$38,$c4,$59,$f7,$9d,$4e,$0a
    .byte $d0,$a2,$81,$6d,$67,$70,$89,$b2,$ed,$3b,$9c,$13,$a0,$45,$02,$da
    .byte $ce,$e0,$11,$64,$da,$76,$39,$26,$40,$89,$04,$b4,$9c

music_frequency_high:
    .byte $00
    .byte $02,$02,$02,$02,$02,$02,$03,$03,$03,$03,$03,$04,$04,$04,$04,$05
    .byte $05,$05,$06,$06,$06,$07,$07,$08,$08,$09,$09,$0a,$0a,$0b,$0c,$0d
    .byte $0d,$0e,$0f,$10,$11,$12,$13,$14,$15,$17,$18,$1a,$1b,$1d,$1f,$20
    .byte $22,$24,$27,$29,$2b,$2e,$31,$34,$37,$3a,$3e,$41,$45

; 每行正好是一小节（八个八分音符），整段共 16 小节、128 步。十进制数
; 是 frequency 表索引，不是 MIDI 音高，也不是直接写入 SID 的寄存器值。
music_lead_pattern:
    .byte 44,44,52,51,49,44,47,49       ; 01: G4 G4 Eb5 D5 C5 G4 Bb4 C5
    .byte 52,49,45,49,52,49,47,45       ; 02: Eb5 C5 Ab4 C5 Eb5 C5 Bb4 Ab4
    .byte 44,47,52,56,54,52,47,44       ; 03: G4 Bb4 Eb5 G5 F5 Eb5 Bb4 G4
    .byte 51,54,56,59,56,54,51,48       ; 04: D5 F5 G5 Bb5 G5 F5 D5 B4
    .byte 49,44,49,51,52,56,54,52       ; 05: C5 G4 C5 D5 Eb5 G5 F5 Eb5
    .byte 49,52,57,56,52,49,47,49       ; 06: C5 Eb5 Ab5 G5 Eb5 C5 Bb4 C5
    .byte 45,49,54,57,56,54,49,45       ; 07: Ab4 C5 F5 Ab5 G5 F5 C5 Ab4
    .byte 48,51,56,60,58,56,51,48       ; 08: B4 D5 G5 B5 A5 G5 D5 B4
    .byte 56,52,49,44,47,49,51,52       ; 09: G5 Eb5 C5 G4 Bb4 C5 D5 Eb5
    .byte 54,51,47,42,47,51,54,56       ; 10: F5 D5 Bb4 F4 Bb4 D5 F5 G5
    .byte 57,56,52,49,45,49,52,56       ; 11: Ab5 G5 Eb5 C5 Ab4 C5 Eb5 G5
    .byte 56,54,51,48,51,54,56,51       ; 12: G5 F5 D5 B4 D5 F5 G5 D5
    .byte 49,54,57,61,57,54,52,49       ; 13: C5 F5 Ab5 C6 Ab5 F5 Eb5 C5
    .byte 49,52,57,61,59,57,52,49       ; 14: C5 Eb5 Ab5 C6 Bb5 Ab5 Eb5 C5
    .byte 48,51,56,60,58,56,54,51       ; 15: B4 D5 G5 B5 A5 G5 F5 D5
    .byte 49,44,52,51,49,44,47,49       ; 16: C5 G4 Eb5 D5 C5 G4 Bb4 C5

music_bass_pattern:
    .byte 13,20,25,20,13,20,23,20       ; 01: Cm
    .byte  9,16,21,16, 9,16,20,16       ; 02: Ab
    .byte 16,23,28,23,16,23,27,23       ; 03: Eb
    .byte  8,15,20,15, 8,15,18,15       ; 04: G
    .byte 13,20,25,20,13,20,23,20       ; 05: Cm
    .byte  9,16,21,16, 9,16,20,16       ; 06: Ab
    .byte 18,25,30,25,18,25,28,25       ; 07: Fm
    .byte  8,15,20,15, 8,15,18,15       ; 08: G
    .byte 13,20,25,20,13,20,23,20       ; 09: Cm
    .byte 11,18,23,18,11,18,21,18       ; 10: Bb
    .byte  9,16,21,16, 9,16,20,16       ; 11: Ab
    .byte  8,15,20,15, 8,15,18,15       ; 12: G
    .byte 18,25,30,25,18,25,28,25       ; 13: Fm
    .byte  9,16,21,16, 9,16,20,16       ; 14: Ab
    .byte  8,15,20,15, 8,15,18,15       ; 15: G
    .byte 13,20,25,28,20,28,25,20       ; 16: Cm，回到循环开头

music_arp_pattern:
    .byte 37,40,44,40,37,40,44,40       ; 01: Cm
    .byte 33,37,40,37,33,37,40,37       ; 02: Ab
    .byte 40,44,47,44,40,44,47,44       ; 03: Eb
    .byte 32,36,39,36,32,36,39,36       ; 04: G
    .byte 37,40,44,40,37,40,44,40       ; 05: Cm
    .byte 33,37,40,37,33,37,40,37       ; 06: Ab
    .byte 30,33,37,33,30,33,37,33       ; 07: Fm
    .byte 32,36,39,36,32,36,39,36       ; 08: G
    .byte 37,40,44,40,37,40,44,40       ; 09: Cm
    .byte 35,39,42,39,35,39,42,39       ; 10: Bb
    .byte 33,37,40,37,33,37,40,37       ; 11: Ab
    .byte 32,36,39,36,32,36,39,36       ; 12: G
    .byte 30,33,37,33,30,33,37,33       ; 13: Fm
    .byte 33,37,40,37,33,37,40,37       ; 14: Ab
    .byte 32,36,39,36,32,36,39,36       ; 15: G
    .byte 37,40,44,40,37,40,44,40       ; 16: Cm

music_drum_pattern:
    .byte 1,0,2,0,1,0,2,0

; 修改编曲表时立即检查长度，避免索引越界读取到下一张表或 sprite 数据。
.assert music_frequency_high - music_frequency_low = 62, error, "music low-frequency table must contain 62 bytes"
.assert music_lead_pattern - music_frequency_high = 62, error, "music high-frequency table must contain 62 bytes"
.assert music_bass_pattern - music_lead_pattern = 128, error, "music lead pattern must contain 128 steps"
.assert music_arp_pattern - music_bass_pattern = 128, error, "music bass pattern must contain 128 steps"
.assert music_drum_pattern - music_arp_pattern = 128, error, "music arp pattern must contain 128 steps"
.assert * - music_drum_pattern = 8, error, "music drum pattern must contain 8 steps"
.assert * <= $3000, error, "music segment overlaps sprite data"
