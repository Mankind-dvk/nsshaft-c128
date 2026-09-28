; 第一源字库：只含自定义图形，不含字母、数字或标点。
; Source charset 1: immutable platform/UI graphics, no text.
; 两套源库和老师示例一样各为 2048 字节；未用槽补零，不生成占位文字。
; Both input charsets have 256 eight-byte slots; unused graphics slots are zero.
.segment "GRAPHICS_CHARSET"
graphics_charset:
.macro GRAPHICS_SLOT glyph_id
    .assert (*-graphics_charset) = (glyph_id)*CHARSET_GLYPH_BYTES, error, "Graphics slot/order mismatch"
.endmacro

; 两个位置共用同一材质定义；00=黑，01=白，10=灰，11=Color RAM 红色。
; EN: A multicolor pixel is two display pixels wide; eight rows still scroll by 1 px.
.macro BRICK_GLYPH
    .byte $f7,$f7,$55,$7f,$7f,$55,$f7,$f7
.endmacro

; --- 空白、本体 / Blank and bodies ---
; 00: 空白 / Blank
    GRAPHICS_SLOT GLYPH_BLANK
graphics_slot_00:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 01: 普通红砖台阶 / Normal brick platform
    GRAPHICS_SLOT GLYPH_PLATFORM_FULL
graphics_slot_01:
    BRICK_GLYPH

; 02: 白绿尖刺；每对位为一个色块，00=空，01=白，11=绿。
; EN: White/green multicolor spike.
    GRAPHICS_SLOT GLYPH_SPIKE_FULL
graphics_slot_02:
    .byte %00000100 ; 空空白空 / empty empty white empty
    .byte %00000100 ; 空空白空 / empty empty white empty
    .byte %00001100 ; 空空绿空 / empty empty green empty
    .byte %00110101 ; 空绿白白 / empty green white white
    .byte %00011101 ; 空白绿白 / empty white green white
    .byte %00010111 ; 空白白绿 / empty white white green
    .byte %11011101 ; 绿白绿白 / green white green white
    .byte %01110111 ; 白绿白绿 / white green white green

; 03: 放松弹簧，双像素宽折线 / Relaxed spring, two-pixel-wide coil
    GRAPHICS_SLOT GLYPH_SPRING_FULL
graphics_slot_03:
    .byte $ff,$0c,$30,$f0,$0f,$0c,$30,$ff

; 04: 压缩弹簧，下半格实心 / Compressed spring, solid lower half
    GRAPHICS_SLOT GLYPH_SPRING_COMPRESSED_FULL
graphics_slot_04:
    .byte $00,$00,$00,$00,$ff,$ff,$ff,$ff

; 05: 右传送纹理按多色像素水平镜像，FULL/LOWER 共用
    GRAPHICS_SLOT GLYPH_CONVEYOR_LEFT_FULL
graphics_slot_05:
    .byte $f5,$d7,$5f,$7d,$7d,$5f,$d7,$f5

; 06: 原始白蓝纹理向右循环，FULL/LOWER 共用
    GRAPHICS_SLOT GLYPH_CONVEYOR_RIGHT_FULL
graphics_slot_06:
    .byte $5f,$d7,$f5,$7d,$7d,$f5,$d7,$5f

; 07: 虚化第二级，灰色填缝部分脱落 / Cracked fade, partial mortar
    GRAPHICS_SLOT GLYPH_FADE_CRACKED_FULL
graphics_slot_07:
    .byte $fb,$f3,$20,$3f,$3f,$02,$f3,$fb

; --- 滚动碎片初值 / Initial fragments ---
; 08: 砖台阶 UPPER
    GRAPHICS_SLOT GLYPH_NORMAL_UPPER
graphics_slot_08:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 09: 砖台阶 LOWER
    GRAPHICS_SLOT GLYPH_NORMAL_LOWER
graphics_slot_09:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 10: 尖刺 UPPER
    GRAPHICS_SLOT GLYPH_SPIKE_UPPER
graphics_slot_10:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 11: 尖刺 LOWER
    GRAPHICS_SLOT GLYPH_SPIKE_LOWER
graphics_slot_11:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 12: 弹簧 UPPER
    GRAPHICS_SLOT GLYPH_SPRING_UPPER
graphics_slot_12:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 13: 弹簧 LOWER
    GRAPHICS_SLOT GLYPH_SPRING_LOWER
graphics_slot_13:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 14: 压缩弹簧 UPPER
    GRAPHICS_SLOT GLYPH_SPRING_COMPRESSED_UPPER
graphics_slot_14:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 15: 压缩弹簧 LOWER
    GRAPHICS_SLOT GLYPH_SPRING_COMPRESSED_LOWER
graphics_slot_15:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 16: 虚化第二级 UPPER
    GRAPHICS_SLOT GLYPH_FADE_CRACKED_UPPER
graphics_slot_16:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 17: 虚化第二级 LOWER
    GRAPHICS_SLOT GLYPH_FADE_CRACKED_LOWER
graphics_slot_17:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 18: 左传送 UPPER
    GRAPHICS_SLOT GLYPH_CONVEYOR_LEFT_UPPER
graphics_slot_18:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 19: 右传送 UPPER
    GRAPHICS_SLOT GLYPH_CONVEYOR_RIGHT_UPPER
graphics_slot_19:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; --- 边框和顶部倒尖刺 / Frame and ceiling ---
; 20: 外框和 UI 分隔列共用红白多色砖纹。
; EN: Shared red/white multicolor brick pattern for the frame and UI divider.
    GRAPHICS_SLOT GLYPH_UI_FRAME
graphics_slot_20:
    BRICK_GLYPH

; 21: 槽 2 的尖刺上下翻转，不做左右镜像。
; EN: Vertical flip of slot 2, with no horizontal mirroring.
    GRAPHICS_SLOT GLYPH_CEILING_SPIKE
graphics_slot_21:
    .byte $77,$dd,$17,$1d,$35,$0c,$04,$04

graphics_defined_end:
.assert graphics_defined_end-graphics_charset = GRAPHICS_GLYPH_COUNT*CHARSET_GLYPH_BYTES, error, "Unexpected graphics count"
    ; 22..39 已从表盘释放；40..55 由第二源字库提供文字。
    .res (GLYPH_FADE_FULL-GRAPHICS_GLYPH_COUNT)*CHARSET_GLYPH_BYTES, 0
    GRAPHICS_SLOT GLYPH_FADE_FULL
    ; 第一级：红砖与灰色填缝，11=红、10=灰。
    .byte $fb,$fb,$aa,$bf,$bf,$aa,$fb,$fb
    GRAPHICS_SLOT GLYPH_FADE_UPPER
    .res 8, 0
    GRAPHICS_SLOT GLYPH_FADE_LOWER
    .res 8, 0
    GRAPHICS_SLOT GLYPH_FADE_BROKEN_FULL
    ; 第三级：只剩红砖，填缝位置为空白。
    .byte $f3,$f3,$00,$3f,$3f,$00,$f3,$f3
    GRAPHICS_SLOT GLYPH_FADE_BROKEN_UPPER
    .res 8, 0
    GRAPHICS_SLOT GLYPH_FADE_BROKEN_LOWER
    .res 8, 0
    .res SOURCE_CHARSET_BYTES-(*-graphics_charset), 0
graphics_charset_end:
.delmacro GRAPHICS_SLOT
.delmacro BRICK_GLYPH
.assert graphics_charset = GRAPHICS_CHARSET_ADDRESS, error, "Graphics source moved"
.assert graphics_charset_end-graphics_charset = SOURCE_CHARSET_BYTES, error, "Graphics source must be 2 KB"
