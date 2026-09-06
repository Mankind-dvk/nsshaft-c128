; ---------------------------------------------------------------------------
; Short compile-time operations / 短操作宏
; ---------------------------------------------------------------------------
; 宏在汇编时原地展开，不是 JSR 子程序，也不自动保存寄存器。
; 本次仅封装已有指令序列：机器码、寄存器副作用和周期数保持不变。
; 参数中的地址、偏移和位掩码都是编译期表达式；A/X/Y 的值属于运行期。
; 不在宏中隐藏循环、状态机、内存 bank 切换或 IRQ 栈协议。
; EN: Inline assembly helpers, not runtime calls. Each contract below describes
; inputs, writes and clobbers. No loops, hidden bank switches or stack handling.

; 将编译期16位值写入连续两个字节；不是从 value 所指内存复制数据。
; EN: Store an immediate word, low byte first. destination reserves two bytes.
; Out: A = high byte; clobbers A, N/Z; preserves X/Y and C/V/D/I.
; Not atomic: protect IRQ vectors at the call site (SEI remains explicit).
.macro SET_WORD destination, value
    lda #<(value)
    sta destination
    lda #>(value)
    sta destination+1
.endmacro

; 选择预计算的 D018 布局；不会更新 active_screen，也不切换 VIC bank。
; EN: Write a complete D018 layout, including both screen and charset selection.
; Out: A = layout; clobbers A, N/Z; preserves X/Y and C/V/D/I.
.macro SHOW_SCREEN layout
    lda #layout
    sta VIC_MEMORY_POINTERS
.endmacro

; 把 A 中的字符码写到两个屏幕的相同位置；颜色 RAM 仍由调用者单独写。
; index_register 可省略，或传 x/y；索引值必须保证最终偏移位于字符区。
; EN: Store A into both matrices at offset [+ X/Y]. Does not write Color RAM.
; Preserves A/X/Y and all flags. Expands to exactly two STA instructions.
.macro STORE_SCREEN_PAIR offset, index_register
    .ifblank index_register
        sta SCREEN_A+offset
        sta SCREEN_B+offset
    .else
        sta SCREEN_A+offset, index_register
        sta SCREEN_B+offset, index_register
    .endif
.endmacro

; A 是当前 VIC bank 内的64字节块编号，不是位图地址。
; slot 为固定槽号；可选 x/y 用于选人循环，调用者保证 slot+索引在0..7。
; EN: Store A's sprite block number into BOTH pointer tables, not bitmap RAM.
; Preserves A/X/Y and all flags; caller bounds optional runtime index to 0..7.
.macro STORE_SPRITE_POINTER slot, index_register
    .assert (slot >= 0) .and (slot < 8), error, "Invalid sprite slot"
    .ifblank index_register
        sta SPRITE_POINTER_A+slot
        sta SPRITE_POINTER_B+slot
    .else
        sta SPRITE_POINTER_A+slot, index_register
        sta SPRITE_POINTER_B+slot, index_register
    .endif
.endmacro

; 只适用于可安全读回再写入的寄存器；不是所有 I/O 都能这样操作。
; 禁用于 D019/CIA 中断状态等有读写副作用的寄存器；D011 光栅比较高位
; 与读回光栅高位语义不同，也不使用这两个宏。不会自动关中断。
; EN: Non-atomic read/modify/write for ordinary readback-safe registers ONLY.
; Never use for IRQ status/acknowledgement registers or D011.
; Out: A = updated register; clobbers A, N/Z; preserves X/Y and C/V/D/I.
.macro SET_REGISTER_BITS target, bits
    lda target
    ora #bits
    sta target
.endmacro

; bits 表示要清除的位，不是保留掩码，例如清 bit 0 应传 %00000001。
; EN: bits names bits to CLEAR (not an AND keep-mask). Same contract as above.
.macro CLEAR_REGISTER_BITS target, bits
    lda target
    and #($ff ^ (bits))
    sta target
.endmacro

; D019 写1确认对应中断；绝不能用上面的读改写宏代替。
; EN: Acknowledge raster IRQ only; leaves A=1, N=0, Z=0; preserves other flags
; and X/Y. Does not enable IRQs, save registers, change MMU or return from IRQ.
.macro ACK_VIC_RASTER_IRQ
    lda #1
    sta VIC_IRQ_STATUS
.endmacro
