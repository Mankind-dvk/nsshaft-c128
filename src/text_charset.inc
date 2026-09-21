; 第二源字库：老师示例 c64.bin 的第一个 2048 字节大写/图形字库。
; Source charset 2: the uppercase half of the teacher's c64.bin.
; 数据随 PRG 装载，不调用 C64 KERNAL，也不依赖运行时字符 ROM/MMU 切换。
; 前 64 项含 @、A-Z、数字和基础标点；来源说明见 assets/fonts/README.md。
; EN: Embedded in the PRG. No disk, C64 KERNAL calls, or character-ROM mapping.
.segment "TEXT_CHARSET"
text_charset:
    .incbin "../assets/fonts/c64-upper.bin"
text_charset_end:
.assert text_charset = TEXT_CHARSET_ADDRESS, error, "Text source moved"
.assert text_charset_end-text_charset = SOURCE_CHARSET_BYTES, error, "Text source must be 2 KB"
