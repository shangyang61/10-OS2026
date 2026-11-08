/* ABI-safe test: preserve the caller, seed GPRs, trap, then compare snapshots. */
    .text
    .align 2
    .globl lab3_probe_registers
lab3_probe_registers:
    addi sp, sp, -384
    sd ra, 256(sp)
    sd gp, 264(sp)
    sd tp, 272(sp)
    sd x8, 280(sp)
    sd x9, 288(sp)
    sd x18, 296(sp)
    sd x19, 304(sp)
    sd x20, 312(sp)
    sd x21, 320(sp)
    sd x22, 328(sp)
    sd x23, 336(sp)
    sd x24, 344(sp)
    sd x25, 352(sp)
    sd x26, 360(sp)
    sd x27, 368(sp)
    li x1, 257
    li x5, 261
    li x6, 262
    li x7, 263
    li x8, 264
    li x9, 265
    li x10, 266
    li x11, 267
    li x12, 268
    li x13, 269
    li x14, 270
    li x15, 271
    li x16, 272
    li x17, 273
    li x18, 274
    li x19, 275
    li x20, 276
    li x21, 277
    li x22, 278
    li x23, 279
    li x24, 280
    li x25, 281
    li x26, 282
    li x27, 283
    li x28, 284
    li x29, 285
    li x30, 286
    li x31, 287
    .2byte 0x9002
    sd x0, 0(sp)
    sd x1, 8(sp)
    sd x2, 16(sp)
    sd x3, 24(sp)
    sd x4, 32(sp)
    sd x5, 40(sp)
    sd x6, 48(sp)
    sd x7, 56(sp)
    sd x8, 64(sp)
    sd x9, 72(sp)
    sd x10, 80(sp)
    sd x11, 88(sp)
    sd x12, 96(sp)
    sd x13, 104(sp)
    sd x14, 112(sp)
    sd x15, 120(sp)
    sd x16, 128(sp)
    sd x17, 136(sp)
    sd x18, 144(sp)
    sd x19, 152(sp)
    sd x20, 160(sp)
    sd x21, 168(sp)
    sd x22, 176(sp)
    sd x23, 184(sp)
    sd x24, 192(sp)
    sd x25, 200(sp)
    sd x26, 208(sp)
    sd x27, 216(sp)
    sd x28, 224(sp)
    sd x29, 232(sp)
    sd x30, 240(sp)
    sd x31, 248(sp)
    ld t0, 0(sp)
    bnez t0, .Lfail
    ld t0, 16(sp)
    bne t0, sp, .Lfail
    ld t0, 24(sp)
    ld t1, 264(sp)
    bne t0, t1, .Lfail
    ld t0, 32(sp)
    ld t1, 272(sp)
    bne t0, t1, .Lfail
    ld t0, 8(sp)
    li t1, 257
    bne t0, t1, .Lfail
    ld t0, 40(sp)
    li t1, 261
    bne t0, t1, .Lfail
    ld t0, 48(sp)
    li t1, 262
    bne t0, t1, .Lfail
    ld t0, 56(sp)
    li t1, 263
    bne t0, t1, .Lfail
    ld t0, 64(sp)
    li t1, 264
    bne t0, t1, .Lfail
    ld t0, 72(sp)
    li t1, 265
    bne t0, t1, .Lfail
    ld t0, 80(sp)
    li t1, 266
    bne t0, t1, .Lfail
    ld t0, 88(sp)
    li t1, 267
    bne t0, t1, .Lfail
    ld t0, 96(sp)
    li t1, 268
    bne t0, t1, .Lfail
    ld t0, 104(sp)
    li t1, 269
    bne t0, t1, .Lfail
    ld t0, 112(sp)
    li t1, 270
    bne t0, t1, .Lfail
    ld t0, 120(sp)
    li t1, 271
    bne t0, t1, .Lfail
    ld t0, 128(sp)
    li t1, 272
    bne t0, t1, .Lfail
    ld t0, 136(sp)
    li t1, 273
    bne t0, t1, .Lfail
    ld t0, 144(sp)
    li t1, 274
    bne t0, t1, .Lfail
    ld t0, 152(sp)
    li t1, 275
    bne t0, t1, .Lfail
    ld t0, 160(sp)
    li t1, 276
    bne t0, t1, .Lfail
    ld t0, 168(sp)
    li t1, 277
    bne t0, t1, .Lfail
    ld t0, 176(sp)
    li t1, 278
    bne t0, t1, .Lfail
    ld t0, 184(sp)
    li t1, 279
    bne t0, t1, .Lfail
    ld t0, 192(sp)
    li t1, 280
    bne t0, t1, .Lfail
    ld t0, 200(sp)
    li t1, 281
    bne t0, t1, .Lfail
    ld t0, 208(sp)
    li t1, 282
    bne t0, t1, .Lfail
    ld t0, 216(sp)
    li t1, 283
    bne t0, t1, .Lfail
    ld t0, 224(sp)
    li t1, 284
    bne t0, t1, .Lfail
    ld t0, 232(sp)
    li t1, 285
    bne t0, t1, .Lfail
    ld t0, 240(sp)
    li t1, 286
    bne t0, t1, .Lfail
    ld t0, 248(sp)
    li t1, 287
    bne t0, t1, .Lfail
    li a0, 0
    j .Lout
.Lfail:
    li a0, 1
.Lout:
    ld ra, 256(sp)
    ld x8, 280(sp)
    ld x9, 288(sp)
    ld x18, 296(sp)
    ld x19, 304(sp)
    ld x20, 312(sp)
    ld x21, 320(sp)
    ld x22, 328(sp)
    ld x23, 336(sp)
    ld x24, 344(sp)
    ld x25, 352(sp)
    ld x26, 360(sp)
    ld x27, 368(sp)
    addi sp, sp, 384
    ret
