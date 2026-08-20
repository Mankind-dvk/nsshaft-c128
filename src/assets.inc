; ---------------------------------------------------------------------------
; 图形资源与不可变游戏数据
; ---------------------------------------------------------------------------
; c128-prg.cfg 将 CHARSET 固定在 $2800，将 SPRITE 固定在 $3000。
; 运行时代码只允许改写下文标明的过渡字形、POT 数字和 paddle 指针 sprite
; 缓冲区；其余字节均为不可变模板。
;
; 初学者提示：每个字符固定 8 字节，每个字节是一条从左到右的像素行，bit 7
; 是最左像素。`.res 8, value` 保留八字节并填入初值。单色 sprite 固定
; 24x21 像素，每行 3 字节，共 63 字节，再补 1 字节对齐到 64。

.segment "CHARSET"

; 0：空白。全 0 时前景像素不存在，只显示所选背景色。
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000

; 1：普通平台。扩展背景色模式下，屏幕码 $41 选择背景色 1（$d022），
; 因此字形中的 0 位会填满整个 8x8 字符格。
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000
    .byte %00000000

; 2：红色等腰三角形。屏幕码 $82 选择背景色 2（$d023）；字形中的 1 位
; 使用黑色 Color RAM 遮住三角形以外的区域。
spike_source_glyph:
    .byte %11100111
    .byte %11100111
    .byte %11000011
    .byte %11000011
    .byte %10000001
    .byte %10000001
    .byte %00000000
    .byte %00000000

; 3：竖井墙壁
    .byte %11100111
    .byte %11000011
    .byte %11011011
    .byte %11000011
    .byte %11100111
    .byte %11000011
    .byte %11011011
    .byte %11000011

; 4..13：数字 0..9
digit_font:
    .byte $3c,$66,$6e,$76,$66,$66,$3c,$00
    .byte $18,$38,$18,$18,$18,$18,$7e,$00
    .byte $3c,$66,$06,$0c,$18,$30,$7e,$00
    .byte $3c,$66,$06,$1c,$06,$66,$3c,$00
    .byte $0c,$1c,$3c,$6c,$7e,$0c,$0c,$00
    .byte $7e,$60,$7c,$06,$06,$66,$3c,$00
    .byte $1c,$30,$60,$7c,$66,$66,$3c,$00
    .byte $7e,$06,$0c,$18,$30,$30,$30,$00
    .byte $3c,$66,$66,$3c,$66,$66,$3c,$00
    .byte $3c,$66,$66,$3e,$06,$0c,$38,$00

; 14..18：P、O、T、X、冒号
    .byte $7c,$66,$66,$7c,$60,$60,$60,$00
    .byte $3c,$66,$66,$66,$66,$66,$3c,$00
    .byte $7e,$18,$18,$18,$18,$18,$18,$00
    .byte $66,$66,$3c,$18,$3c,$66,$66,$00
    .byte $00,$18,$18,$00,$00,$18,$18,$00

; 19..22：N、S、连字符、H。标题/结束画面使用这些原始字形；进入游戏后，
; conveyor_platforms.inc 会把四个连续槽临时改成左/右传送平台的完整/上部
; 字形。使用标签标出准确地址，避免运行时代码依赖手算 $2800+编号*8。
conveyor_left_full_glyph:
    .byte $66,$76,$7e,$7e,$6e,$66,$66,$00
conveyor_left_upper_glyph:
    .byte $3c,$66,$60,$3c,$06,$66,$3c,$00
conveyor_right_full_glyph:
    .byte $00,$00,$00,$7e,$00,$00,$00,$00
conveyor_right_upper_glyph:
    .byte $66,$66,$66,$7e,$66,$66,$66,$00

; 23..30：A、F、R、E、I、G、M、V。
    .byte $18,$3c,$66,$66,$7e,$66,$66,$00
    .byte $7e,$60,$60,$7c,$60,$60,$60,$00
    .byte $7c,$66,$66,$7c,$6c,$66,$66,$00
    .byte $7e,$60,$60,$7c,$60,$60,$7e,$00
    .byte $7e,$18,$18,$18,$18,$18,$7e,$00
    .byte $3c,$66,$60,$6e,$66,$66,$3e,$00
    .byte $63,$77,$7f,$6b,$63,$63,$63,$00
    .byte $66,$66,$66,$66,$66,$3c,$18,$00

; 31..34：运行时生成的上/下碎片，用于逐像素滚动。
; `.res` 的初值只负责启动前的确定内容；第一帧滚动会由 video.inc 重写。
normal_upper_glyph:
    .res 8, $ff
normal_lower_glyph:
    .res 8, $00
spike_upper_glyph:
    .res 8, $ff
spike_lower_glyph:
    .res 8, $ff

; 35：实心蓝色状态栏分隔字符。
    .byte $ff,$ff,$ff,$ff,$ff,$ff,$ff,$ff

; 36..59：56 像素宽的表盘，以及复用表盘空白槽的弹簧平台字形。
; 弹簧使用普通黑色背景和紫色 Color RAM 前景；字形中的 1 是紫色图形。
; 36：完整弹簧，暂时画成“工”字形。
spring_source_glyph:
    .byte %11111111,%11111111,%00011000,%00011000
    .byte %00011000,%00011000,%11111111,%11111111
    .byte $00,$00,$00,$00,$00,$00,$01,$07
    .byte $00,$00,$00,$07,$1e,$70,$c0,$00
    .byte $00,$00,$7f,$f0,$00,$00,$00,$00
    .byte $00,$00,$fe,$0f,$00,$00,$00,$00
    .byte $00,$00,$00,$e0,$78,$0e,$03,$00
    .byte $00,$00,$00,$00,$00,$00,$80,$e0
; 43：滚动时位于上一字符行的弹簧碎片，运行时重建。
spring_upper_glyph:
    .res 8, $00

    .byte $00,$00,$00,$00,$00,$00,$01,$01
    .byte $0e,$18,$30,$60,$e0,$c0,$80,$80
; 46：完整弹簧的下部滚动碎片。
spring_lower_glyph:
    .res 8, $00
; 47：压扁后的“工”，保留短柱并把上下横梁压到字符底部。
spring_compressed_source_glyph:
    .byte %00000000,%00000000,%00000000,%00000000
    .byte %11111111,%00011000,%11111111,%11111111
; 48..49：压扁状态的上/下滚动碎片。
spring_compressed_upper_glyph:
    .res 8, $00
spring_compressed_lower_glyph:
    .res 8, $00
    .byte $70,$18,$0c,$06,$07,$03,$01,$01
    .byte $00,$00,$00,$00,$00,$00,$80,$80

    .byte $03,$03,$02,$06,$06,$06,$07,$02
    .byte $00,$00,$00,$00,$02,$02,$ff,$02
    .byte $00,$00,$00,$00,$01,$01,$ff,$01
    .byte $00,$00,$00,$00,$00,$00,$ff,$00
    .byte $00,$00,$00,$00,$80,$80,$ff,$80
    .byte $00,$00,$00,$00,$40,$40,$ff,$40
    .byte $00,$00,$00,$00,$20,$20,$ff,$20
    .byte $c0,$c0,$40,$60,$60,$60,$e0,$40

; 60..62：已触发消失平台的完整、上部和下部字形。低六位与普通灰色
; 平台不同，因此闪烁/删除逻辑可以只处理已经踩过的平台。63 保留为空白。
    .byte $00,$00,$00,$00,$00,$00,$00,$00
fade_active_upper_glyph:
    .res 8, $ff
fade_active_lower_glyph:
    .res 8, $00
    .res 8, $00

.assert * = $2a00, error, "64-character ECM charset must end at $2a00"
.assert conveyor_left_full_glyph = $2800 + CHAR_CONVEYOR_LEFT_FULL*8, error, "left conveyor glyph slot changed"
.assert conveyor_left_upper_glyph = $2800 + CHAR_CONVEYOR_LEFT_UPPER*8, error, "left conveyor upper slot changed"
.assert conveyor_right_full_glyph = $2800 + CHAR_CONVEYOR_RIGHT_FULL*8, error, "right conveyor glyph slot changed"
.assert conveyor_right_upper_glyph = $2800 + CHAR_CONVEYOR_RIGHT_UPPER*8, error, "right conveyor upper slot changed"

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
.assert * = PADDLE_POINTER_ADDRESS, error, "player sprite frames must occupy four 64-byte blocks"

; Sprite 4 位于可写 RAM。根据玩家移动使用的同一 paddle 分档，将五种
; 表针形状之一复制到此处。
paddle_pointer_sprite:
    .res 63, 0
    .byte 0

.segment "CODE"

potx_label:
    .byte CHAR_P, CHAR_O, CHAR_T, CHAR_X, CHAR_COLON

; 复制到可写 sprite 4 缓冲区的 paddle 表针模板。两张地址表分别保存低/高
; 字节；update_dashboard_pointer 用相同 X 索引组合出一个 16 位源指针。
pointer_shape_low:
    .byte <pointer_left_shape, <pointer_left_inner_shape
    .byte <pointer_center_shape, <pointer_right_inner_shape
    .byte <pointer_right_shape
pointer_shape_high:
    .byte >pointer_left_shape, >pointer_left_inner_shape
    .byte >pointer_center_shape, >pointer_right_inner_shape
    .byte >pointer_right_shape

pointer_left_shape:
    .res 18, 0
    .byte $10,$00,$00, $08,$00,$00, $08,$00,$00, $04,$00,$00
    .byte $02,$00,$00, $01,$00,$00, $01,$00,$00, $00,$80,$00
    .byte $00,$40,$00, $00,$20,$00, $00,$30,$00, $00,$38,$00
    .byte $00,$10,$00
    .res 6, 0

; 左内档：方向位于垂直与最左档之间，尖端约向左偏四个像素。
pointer_left_inner_shape:
    .res 12, 0
    .byte $01,$00,$00, $01,$00,$00, $00,$80,$00, $00,$80,$00
    .byte $00,$80,$00, $00,$40,$00, $00,$40,$00, $00,$40,$00
    .byte $00,$20,$00, $00,$20,$00, $00,$20,$00, $00,$10,$00
    .byte $00,$38,$00, $00,$10,$00, $00,$10,$00
    .res 6, 0

pointer_center_shape:
    .res 9, 0
    .byte $00,$10,$00, $00,$10,$00, $00,$10,$00, $00,$10,$00
    .byte $00,$10,$00, $00,$10,$00, $00,$10,$00, $00,$10,$00
    .byte $00,$10,$00, $00,$10,$00, $00,$10,$00, $00,$10,$00
    .byte $00,$10,$00, $00,$10,$00, $00,$38,$00, $00,$10,$00
    .res 6, 0

; 右内档与左内档镜像。
pointer_right_inner_shape:
    .res 12, 0
    .byte $00,$01,$00, $00,$01,$00, $00,$02,$00, $00,$02,$00
    .byte $00,$02,$00, $00,$04,$00, $00,$04,$00, $00,$04,$00
    .byte $00,$08,$00, $00,$08,$00, $00,$08,$00, $00,$10,$00
    .byte $00,$38,$00, $00,$10,$00, $00,$10,$00
    .res 6, 0

pointer_right_shape:
    .res 18, 0
    .byte $00,$00,$08, $00,$00,$10, $00,$00,$20, $00,$00,$20
    .byte $00,$00,$40, $00,$00,$80, $00,$01,$00, $00,$02,$00
    .byte $00,$04,$00, $00,$04,$00, $00,$18,$00, $00,$38,$00
    .byte $00,$10,$00
    .res 6, 0

; 地址表、屏幕码行和开始/结束画面文字。初始平台地址拆成 low/high 两表，
; 是 6502 常见的“结构分离数组”：用同一个 X 同时取出一个 16 位地址。
seed_platform_address_low:
    .byte <(SCREEN_A + 2*SCREEN_COLUMNS)
    .byte <(SCREEN_A + 7*SCREEN_COLUMNS)
    .byte <(SCREEN_A + 12*SCREEN_COLUMNS)
    .byte <(SCREEN_A + 17*SCREEN_COLUMNS)
    .byte <(SCREEN_A + 22*SCREEN_COLUMNS)

seed_platform_address_high:
    .byte >(SCREEN_A + 2*SCREEN_COLUMNS)
    .byte >(SCREEN_A + 7*SCREEN_COLUMNS)
    .byte >(SCREEN_A + 12*SCREEN_COLUMNS)
    .byte >(SCREEN_A + 17*SCREEN_COLUMNS)
    .byte >(SCREEN_A + 22*SCREEN_COLUMNS)

platform_chars:
    .byte CHAR_PLATFORM_FULL, CHAR_SPIKE_TRIANGLE, CHAR_FADE_FULL
    .byte CHAR_SPRING_FULL, CHAR_CONVEYOR_LEFT_FULL
    .byte CHAR_CONVEYOR_RIGHT_FULL
platform_chars_end:
.assert platform_chars_end-platform_chars = PLATFORM_TYPE_COUNT, error, "platform type table size mismatch"

dial_row_1:
    .byte CHAR_BLANK, CHAR_DIAL_ROW_1+1
    .byte CHAR_DIAL_ROW_1+2, CHAR_DIAL_ROW_1+3
    .byte CHAR_DIAL_ROW_1+4, CHAR_DIAL_ROW_1+5
    .byte CHAR_DIAL_ROW_1+6, CHAR_BLANK
dial_row_2:
    .byte CHAR_DIAL_ROW_2+0, CHAR_DIAL_ROW_2+1
    .byte CHAR_BLANK, CHAR_BLANK
    .byte CHAR_BLANK, CHAR_BLANK
    .byte CHAR_DIAL_ROW_2+6, CHAR_DIAL_ROW_2+7
dial_row_3:
    .byte CHAR_DIAL_ROW_3+0, CHAR_DIAL_ROW_3+1
    .byte CHAR_DIAL_ROW_3+2, CHAR_DIAL_ROW_3+3
    .byte CHAR_DIAL_ROW_3+4, CHAR_DIAL_ROW_3+5
    .byte CHAR_DIAL_ROW_3+6, CHAR_DIAL_ROW_3+7

start_title:
    .byte CHAR_N, CHAR_S, CHAR_DASH, CHAR_S
    .byte CHAR_H, CHAR_A, CHAR_F, CHAR_T
game_over_title:
    .byte CHAR_G, CHAR_A, CHAR_M, CHAR_E, CHAR_BLANK
    .byte CHAR_O, CHAR_V, CHAR_E, CHAR_R
action_prompt:
    .byte CHAR_P, CHAR_R, CHAR_E, CHAR_S, CHAR_S
    .byte CHAR_BLANK, CHAR_F, CHAR_I, CHAR_R, CHAR_E
