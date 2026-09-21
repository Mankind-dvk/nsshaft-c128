# 字体来源 / Font provenance

`c64-upper.bin` 是课堂示例目录中 `c64.bin` 的前 2048 字节，未修改像素。
原始文件为 4096 字节，包含两套 256 字形字库；本项目选择第一套大写字库。

`c64-upper.bin` is the unchanged first 2048 bytes of the 4096-byte `c64.bin`
provided with the teacher's charset example. It selects the first uppercase
256-glyph set, not the second lowercase set.

- 来源 / Source: teacher-provided `c64-demos-main/charsets/c64.bin`
- 方法示例 / Method reference: `c64-demos-main/charsets/main.asm`
- 提取区间 / Byte slice: `[0, 2048)`; raw binary, no PRG load-address header
- SHA-256: `3cf89732b10b1d51a267f74df35f10a154108b444a3a0ec9e51ef7ddefb668a1`

由 `src/text_charset.inc` 内嵌到 PRG，构建不再依赖上述外部路径。运行时只
将需要的字形复制到输出字库。部分数字和标点与先前手写字体略有区别，
这是采用示例字体后的预期变化，不是平滑滚动造成的覆盖。

The project-local binary is embedded by `src/text_charset.inc`; builds do not
depend on the external path. Runtime copies selected glyphs to the output.
Some digits and punctuation differ from the previous hand-coded font by design.
