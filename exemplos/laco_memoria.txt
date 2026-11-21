addi t0, zero, 3
laco:
add t1, t1, t0
addi t0, t0, -1
bne t0, zero, laco
sw t1, 48(zero)
lw a0, 48(zero)
