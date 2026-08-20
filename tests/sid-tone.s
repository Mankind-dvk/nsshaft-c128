.setcpu "6502"

; 独立的 SID 持续音测试。它不使用游戏主循环、sprite 或中断，
; 因而可以把 VICE/Windows 音频问题与游戏代码问题分开验证。

MMU_CONFIG = $ff00
SID_BASE   = $d400

.segment "LOADADDR"
    .word $1c01

.segment "BASIC"
    .word basic_end
    .word 10
    .byte $9e
    .byte "7424"
    .byte 0
basic_end:
    .word 0

.segment "CODE"

start:
    sei

    ; 显式选择 C128 RAM bank 0，并让 $d000-$dfff 保持 I/O 可见。
    lda #$0e
    sta MMU_CONFIG

    ; 清空全部 29 个 SID 寄存器，避免继承 BASIC 或旧程序的状态。
    ldx #$1c
    lda #0
clear_sid:
    sta SID_BASE,x
    dex
    bpl clear_sid

    ; Voice 1: A4，三角波，立即起音并保持最大 sustain。
    lda #$45
    sta SID_BASE + $00
    lda #$1d
    sta SID_BASE + $01
    lda #$00
    sta SID_BASE + $05
    lda #$f0
    sta SID_BASE + $06
    lda #$0f
    sta SID_BASE + $18
    lda #$11
    sta SID_BASE + $04

hold_tone:
    jmp hold_tone
