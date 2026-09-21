; VIC-IIe 实际读取的输出字库 / Live output charset
; ---------------------------------------------------------------------------
; 与老师示例的 outputCharsetStart 对应。源库并不直接拿来显示：场景入口把
; 需要的字形复制到本区，游戏帧循环只更新这里的动态碎片，不再改写文字源库。
; EN: One 66-glyph mixed-mode output assembled from two immutable source charsets.
.segment "CHARSET"
charset_begin:
    .res CHARSET_GLYPH_COUNT*CHARSET_GLYPH_BYTES, 0
charset_end:
.assert charset_begin = CHARSET_ADDRESS, error, "Live charset moved"
.assert charset_end = $2a10, error, "Live charset overlaps music"

.include "graphics_charset.inc"
.include "text_charset.inc"
.include "charset_loader.inc"
