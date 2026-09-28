; ---------------------------------------------------------------------------
; 图形资源与不可变游戏数据
; ---------------------------------------------------------------------------
; c128-prg.cfg 将 CHARSET 固定在 $2800，将 Elien 四帧固定在 $3000，
; 将 Elien hurt、Ember/Wasser 原有帧固定在 $3a00-$3cbf，
; 六张高速方向帧固定在 $3e80-$3fff。
; charset.inc 引入图形源库、文字源库及合成例程，编号放在 charset_ids.inc。
; 运行时代码只改写标明的动态字形；POT 数字
; 更新的是屏幕码，不是数字字形。其余字节均为不可变模板。
;
; 初学者提示：每个字符固定 8 字节，每个字节是一条从左到右的像素行，bit 7
; 是最左像素。`.res 8, value` 保留八字节并填入初值。单色 sprite 固定
; 24x21 像素，每行 3 字节，共 63 字节，再补 1 字节对齐到 64。
;
; English summary:
;   The linker fixes CHARSET at $2800, the main SPRITE bank at $3000, and the
;   remaining character frames at $3a00-$3cbf. A character is eight bytes with
;   bit 7 at the left. A monochrome sprite is 24x21 pixels (63 bytes) plus one
;   alignment byte. Labels
;   marked as runtime glyphs may change; all other bytes
;   are immutable templates and lookup tables.

.include "charset.inc"

.segment "SPRITE"

; Spritemate 导出的四张 24x21 单色玩家帧。每帧前 63 字节是位图，最后
; 一个字节只用于 64 字节对齐；编辑器导出的颜色元数据不参与 VIC 显示，
; 因此统一改为 0。它们只是 sprite 0 可切换的内存块，不会占用硬件
; sprite 1..3。
player_normal_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $18,$00,$01,$99,$80,$01,$ff,$80
    .byte $03,$ff,$c0,$03,$ff,$c0,$1f,$ff
    .byte $f8,$1f,$3c,$f8,$07,$3c,$e0,$07
    .byte $3c,$e0,$3f,$3c,$fc,$3f,$ff,$fc
    .byte $0f,$ff,$f0,$0f,$ff,$f0,$03,$81
    .byte $c0,$03,$81,$c0,$03,$81,$c0,$00

player_fall_sprite:
    .byte $00,$00,$00,$00,$00,$00,$01,$99
    .byte $80,$01,$99,$80,$01,$ff,$80,$03
    .byte $ff,$c0,$03,$ff,$c0,$1f,$ff,$f8
    .byte $1f,$7e,$f8,$07,$bd,$e0,$07,$db
    .byte $e0,$3f,$bd,$fc,$3f,$7e,$fc,$0f
    .byte $ff,$f0,$0f,$c3,$f0,$03,$81,$c0
    .byte $01,$e7,$80,$00,$e7,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00

player_right_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $18,$00,$03,$19,$80,$03,$ff,$80
    .byte $03,$ff,$c0,$03,$ff,$c0,$3f,$ff
    .byte $f8,$3f,$9e,$78,$07,$9e,$60,$07
    .byte $9e,$60,$7f,$9e,$7c,$7f,$ff,$fc
    .byte $0f,$ff,$f0,$0f,$ff,$f0,$03,$81
    .byte $c0,$03,$81,$c0,$03,$81,$c0,$00

player_left_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $18,$00,$01,$98,$c0,$01,$ff,$c0
    .byte $03,$ff,$c0,$03,$ff,$c0,$1f,$ff
    .byte $fc,$1e,$79,$fc,$06,$79,$e0,$06
    .byte $79,$e0,$3e,$79,$fe,$3f,$ff,$fe
    .byte $0f,$ff,$f0,$0f,$ff,$f0,$03,$81
    .byte $c0,$03,$81,$c0,$03,$81,$c0,$00

.assert player_normal_sprite = PLAYER_NORMAL_ADDRESS, error, "normal sprite address changed"
.assert player_fall_sprite = PLAYER_FALL_ADDRESS, error, "fall sprite address changed"
.assert player_right_sprite = PLAYER_RIGHT_ADDRESS, error, "right sprite address changed"
.assert player_left_sprite = PLAYER_LEFT_ADDRESS, error, "left sprite address changed"
.assert * = $3100, error, "player sprite frames must occupy four 64-byte blocks"
; $3100-$313f 不再存表针；sprite 4 直接指向所选角色已有的位图块。

.segment "PLAYERSETS"

; Spritemate 的 elien_hurt 单色帧。导出数据第 64 字节是颜色元数据 $07，
; 不是位图；这里与其他玩家帧一样改成 0 作为对齐填充。它仍由硬件 sprite 0
; 显示，不会额外占用 sprite 1..3。
player_hurt_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $18,$00,$01,$99,$80,$01,$ff,$80
    .byte $03,$ff,$c0,$03,$ff,$c0,$1f,$ff
    .byte $f8,$1f,$3c,$f8,$07,$3c,$e0,$07
    .byte $3c,$e0,$3f,$3c,$fc,$3f,$e6,$fc
    .byte $0f,$da,$f0,$0f,$ff,$f0,$03,$81
    .byte $c0,$03,$81,$c0,$03,$81,$c0,$00
player_hurt_sprite_end:

.assert player_hurt_sprite = PLAYER_HURT_ADDRESS, error, "hurt sprite address changed"
.assert player_hurt_sprite_end-player_hurt_sprite = 64, error, "hurt sprite must occupy one block"

; Ember 与 Wasser 的 normal/fall/right/left/hurt。Spritemate 工程中的
; `emeber_right` 只有名字拼写错误，位图在这里按正确的 ember_right 标签导入。
ember_normal_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$18
    .byte $00,$00,$18,$00,$00,$3c,$00,$02
    .byte $7e,$40,$06,$7e,$60,$06,$ff,$60
    .byte $0e,$ff,$70,$0f,$ff,$f0,$1f,$ff
    .byte $f8,$1f,$ff,$f8,$1f,$3c,$f8,$1f
    .byte $3c,$f8,$1f,$3c,$f8,$0f,$3c,$f0
    .byte $0f,$ff,$f0,$07,$ff,$e0,$03,$ff
    .byte $c0,$01,$c3,$80,$01,$c3,$80,$00

ember_fall_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$18
    .byte $00,$00,$18,$00,$00,$3c,$00,$02
    .byte $7e,$40,$06,$7e,$60,$06,$ff,$60
    .byte $0e,$ff,$70,$0f,$ff,$f0,$1f,$ff
    .byte $f8,$1f,$ff,$f8,$1f,$ff,$f8,$1f
    .byte $ff,$f8,$1f,$ff,$f8,$0e,$18,$70
    .byte $0f,$3c,$f0,$07,$3c,$e0,$03,$ff
    .byte $c0,$00,$ff,$00,$00,$3c,$00,$00

ember_right_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$c0
    .byte $00,$00,$78,$00,$30,$7c,$00,$1c
    .byte $3e,$00,$1e,$3e,$c0,$0e,$ff,$60
    .byte $0f,$ff,$70,$0f,$ff,$70,$1f,$ff
    .byte $f8,$1f,$ff,$f8,$1f,$9e,$78,$1f
    .byte $9e,$78,$1f,$9e,$78,$0f,$9e,$70
    .byte $0f,$ff,$f0,$07,$ff,$e0,$03,$ff
    .byte $c0,$01,$c3,$80,$01,$c3,$80,$00

ember_left_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$03
    .byte $00,$00,$1e,$00,$00,$3e,$0c,$00
    .byte $7c,$38,$03,$7c,$78,$06,$ff,$70
    .byte $0e,$ff,$f0,$0e,$ff,$f0,$1f,$ff
    .byte $f8,$1f,$ff,$f8,$1e,$79,$f8,$1e
    .byte $79,$f8,$1e,$79,$f8,$0e,$79,$f0
    .byte $0f,$ff,$f0,$07,$ff,$e0,$03,$ff
    .byte $c0,$01,$c3,$80,$01,$c3,$80,$00

ember_hurt_sprite:
    .byte $00,$00,$00,$00,$18,$00,$0e,$18
    .byte $70,$3c,$bd,$3c,$1c,$ff,$38,$1e
    .byte $ff,$70,$0e,$ff,$72,$2e,$ff,$72
    .byte $1d,$ff,$f8,$1a,$ff,$f8,$5d,$ff
    .byte $fa,$7e,$db,$7e,$3e,$18,$7c,$1f
    .byte $3c,$f8,$1f,$3c,$f8,$0f,$ff,$f0
    .byte $0f,$e7,$f0,$07,$db,$e0,$03,$ff
    .byte $c0,$01,$c3,$80,$01,$c3,$80,$00

wasser_normal_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$3c,$00,$00,$f3,$00,$03,$fd
    .byte $c0,$07,$fe,$e0,$0f,$ff,$70,$0f
    .byte $ff,$b0,$1f,$3c,$b8,$1f,$3c,$b8
    .byte $1f,$3c,$b8,$1f,$3c,$f8,$0f,$ff
    .byte $f0,$07,$ff,$e0,$03,$ff,$c0,$00

wasser_fall_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$3c,$00,$00
    .byte $fb,$00,$03,$fd,$c0,$07,$fe,$e0
    .byte $0e,$7e,$70,$0e,$7e,$70,$1e,$7e
    .byte $78,$1e,$7e,$38,$1f,$ff,$b8,$1f
    .byte $ff,$b8,$1f,$ff,$78,$0f,$e7,$70
    .byte $0f,$e6,$f0,$07,$fe,$e0,$03,$fd
    .byte $c0,$00,$fb,$00,$00,$3c,$00,$00

wasser_right_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$3c,$00,$00,$f3,$00,$03,$fd
    .byte $c0,$07,$fe,$e0,$0f,$ff,$70,$0f
    .byte $ff,$70,$1f,$9e,$78,$1f,$9e,$78
    .byte $1f,$9e,$78,$1f,$9e,$78,$0f,$ff
    .byte $f0,$07,$ff,$e0,$03,$ff,$c0,$00

wasser_left_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$3c,$00,$00,$f3,$00,$03,$fd
    .byte $c0,$07,$fe,$e0,$0f,$ff,$70,$0f
    .byte $ff,$b0,$1e,$79,$b8,$1e,$79,$b8
    .byte $1e,$79,$b8,$1e,$79,$f8,$0f,$ff
    .byte $f0,$07,$ff,$e0,$03,$ff,$c0,$00

wasser_hurt_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$06
    .byte $00,$10,$04,$01,$90,$0d,$c7,$80
    .byte $08,$fe,$20,$00,$f3,$00,$03,$fd
    .byte $c0,$07,$fe,$e0,$0f,$ff,$70,$0f
    .byte $ff,$b0,$1d,$fb,$b8,$1e,$f7,$b8
    .byte $5f,$6f,$ba,$9e,$f7,$f8,$0d,$fb
    .byte $f2,$67,$ff,$e4,$7f,$ff,$fc,$00

player_sets_end:
.assert ember_normal_sprite = EMBER_NORMAL_ADDRESS, error, "Ember normal address changed"
.assert ember_fall_sprite = EMBER_FALL_ADDRESS, error, "Ember fall address changed"
.assert ember_right_sprite = EMBER_RIGHT_ADDRESS, error, "Ember right address changed"
.assert ember_left_sprite = EMBER_LEFT_ADDRESS, error, "Ember left address changed"
.assert ember_hurt_sprite = EMBER_HURT_ADDRESS, error, "Ember hurt address changed"
.assert wasser_normal_sprite = WASSER_NORMAL_ADDRESS, error, "Wasser normal address changed"
.assert wasser_fall_sprite = WASSER_FALL_ADDRESS, error, "Wasser fall address changed"
.assert wasser_right_sprite = WASSER_RIGHT_ADDRESS, error, "Wasser right address changed"
.assert wasser_left_sprite = WASSER_LEFT_ADDRESS, error, "Wasser left address changed"
.assert wasser_hurt_sprite = WASSER_HURT_ADDRESS, error, "Wasser hurt address changed"
.assert player_sets_end = PLAYER_SETS_END_ADDRESS, error, "player sets must end before select code"

; 来自 highsprites.txt 的左右高速帧。Spritemate 的第 64 字节是颜色元数据，
; 这里统一改成 0 作为对齐填充；实际颜色仍由角色颜色寄存器控制。
; 六帧连续占用 $3e80-$3fff。玩家 sprite 0 与 HUD sprite 4 直接复用这些块。
.segment "FASTSPRITES"

player_fastleft_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$7e,$00,$01,$ff,$f0
    .byte $03,$ff,$f0,$07,$ff,$c0,$0f,$ff
    .byte $fc,$0c,$f3,$fc,$0c,$f3,$e0,$1c
    .byte $f3,$e0,$1c,$f3,$fe,$1f,$ff,$fe
    .byte $1f,$ff,$f0,$1f,$ff,$fc,$03,$81
    .byte $c0,$03,$81,$c0,$03,$81,$c0,$00

player_fastright_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$7e,$00,$0f,$ff,$80
    .byte $0f,$ff,$c0,$03,$ff,$e0,$3f,$ff
    .byte $f0,$3f,$cf,$30,$07,$cf,$30,$07
    .byte $cf,$38,$7f,$cf,$38,$7f,$ff,$f8
    .byte $0f,$ff,$f8,$3f,$ff,$f8,$03,$81
    .byte $c0,$03,$81,$c0,$03,$81,$c0,$00

ember_fastleft_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$3f,$f0,$00
    .byte $ff,$e0,$03,$ff,$fc,$07,$ff,$fe
    .byte $0f,$ff,$f0,$0f,$ff,$e0,$1f,$ff
    .byte $f0,$1f,$ff,$fc,$1c,$f3,$ff,$1c
    .byte $f3,$fe,$1c,$f3,$f8,$0c,$f3,$f0
    .byte $0f,$ff,$e0,$07,$ff,$f8,$03,$ff
    .byte $fe,$01,$c3,$f8,$01,$c3,$80,$00

ember_fastright_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$0f,$fc,$00,$07
    .byte $ff,$00,$3f,$ff,$c0,$7f,$ff,$e0
    .byte $0f,$ff,$f0,$07,$ff,$f0,$0f,$ff
    .byte $f8,$3f,$ff,$f8,$ff,$cf,$38,$7f
    .byte $cf,$38,$1f,$cf,$38,$0f,$cf,$30
    .byte $07,$ff,$f0,$1f,$ff,$e0,$7f,$ff
    .byte $c0,$1f,$c3,$80,$01,$c3,$80,$00

wasser_fastleft_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$ff,$da,$03,$f1
    .byte $f0,$07,$fe,$fa,$0f,$ff,$70,$0f
    .byte $ff,$be,$1c,$f3,$b8,$1c,$f3,$be
    .byte $1c,$f3,$be,$1c,$f3,$f8,$0f,$ff
    .byte $f0,$07,$ff,$f8,$03,$ff,$fd,$00

wasser_fastright_sprite:
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$00,$00,$00,$00,$00
    .byte $00,$00,$00,$5b,$ff,$00,$0f,$8f
    .byte $c0,$5f,$7f,$e0,$0e,$ff,$f0,$7d
    .byte $ff,$f0,$1d,$cf,$38,$7d,$cf,$38
    .byte $7d,$cf,$38,$1f,$cf,$38,$0f,$ff
    .byte $f0,$1f,$ff,$e0,$bf,$ff,$c0,$00

fast_sprites_end:
.assert player_fastleft_sprite = PLAYER_FASTLEFT_ADDRESS, error, "Elien fast-left address changed"
.assert player_fastright_sprite = PLAYER_FASTRIGHT_ADDRESS, error, "Elien fast-right address changed"
.assert ember_fastleft_sprite = EMBER_FASTLEFT_ADDRESS, error, "Ember fast-left address changed"
.assert ember_fastright_sprite = EMBER_FASTRIGHT_ADDRESS, error, "Ember fast-right address changed"
.assert wasser_fastleft_sprite = WASSER_FASTLEFT_ADDRESS, error, "Wasser fast-left address changed"
.assert wasser_fastright_sprite = WASSER_FASTRIGHT_ADDRESS, error, "Wasser fast-right address changed"
.assert fast_sprites_end = $4000, error, "fast sprites must end at VIC bank boundary"

.segment "CODE"

potx_label:
    .byte CHAR_P, CHAR_O, CHAR_T, CHAR_X, CHAR_COLON

; 地址表、屏幕码行和开始/结束画面文字。初始平台地址拆成 low/high 两表，
; 是 6502 常见的“结构分离数组”：用同一个 X 同时取出一个 16 位地址。
seed_platform_address_low:
    .byte <(SCREEN_A + PLAYFIELD_TOP_ROW*SCREEN_COLUMNS)
    .byte <(SCREEN_A + 7*SCREEN_COLUMNS)
    .byte <(SCREEN_A + 12*SCREEN_COLUMNS)
    .byte <(SCREEN_A + 17*SCREEN_COLUMNS)
    .byte <(SCREEN_A + PLAYFIELD_BOTTOM_ROW*SCREEN_COLUMNS)

seed_platform_address_high:
    .byte >(SCREEN_A + PLAYFIELD_TOP_ROW*SCREEN_COLUMNS)
    .byte >(SCREEN_A + 7*SCREEN_COLUMNS)
    .byte >(SCREEN_A + 12*SCREEN_COLUMNS)
    .byte >(SCREEN_A + 17*SCREEN_COLUMNS)
    .byte >(SCREEN_A + PLAYFIELD_BOTTOM_ROW*SCREEN_COLUMNS)

; 初始行 3/7/12/17/22；相对索引从 PLAYFIELD_TOP_ROW 推导，避开倒尖刺行。
seed_platform_score_index:
    .byte 0, 7-PLAYFIELD_TOP_ROW, 12-PLAYFIELD_TOP_ROW
    .byte 17-PLAYFIELD_TOP_ROW, PLAYFIELD_BOTTOM_ROW-PLAYFIELD_TOP_ROW

platform_chars:
    .byte CHAR_PLATFORM_FULL, CHAR_SPIKE_TRIANGLE, CHAR_FADE_FULL
    .byte CHAR_SPRING_FULL, CHAR_CONVEYOR_LEFT_FULL
    .byte CHAR_CONVEYOR_RIGHT_FULL
platform_chars_end:
.assert platform_chars_end-platform_chars = PLATFORM_TYPE_COUNT, error, "platform type table size mismatch"

start_title:
    .byte TEXT_N, TEXT_S, TEXT_DASH, TEXT_S
    .byte TEXT_H, TEXT_A, TEXT_F, TEXT_T
game_over_title:
    .byte TEXT_G, TEXT_A, TEXT_M, TEXT_E, TEXT_SPACE
    .byte TEXT_O, TEXT_V, TEXT_E, TEXT_R
action_prompt:
    .byte TEXT_P, TEXT_R, TEXT_E, TEXT_S, TEXT_S
    .byte TEXT_SPACE, TEXT_F, TEXT_I, TEXT_R, TEXT_E
