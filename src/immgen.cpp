#include "immgen.h"

// Função para estender o sinal de um valor de 'bits' bits para 32 bits
std::int32_t signExt(std::uint32_t valor, int bits) {
    int shift = 32 - bits;          //o segundo deslocamento é aritmetico, pois o valor sofre um cast para int32_t
    return static_cast<std::int32_t>(valor << shift) >> shift;
}

std::int32_t immgen(std::uint32_t instruction) {
    std::uint32_t opcode = instruction & 0x7F;

    switch (opcode) {
        case 3:    // load
        case 19:   // op-imm
        case 103:  // jalr
                           //deslocamento logico, preenche com 0 a esquerda
            return signExt(instruction >> 20, 12);                    // I

        case 35:   // store
            return signExt(((instruction >> 25) << 5) |
                           ((instruction >> 7) & 0x1F), 12);          // S

        case 99:   // branch
            return signExt((((instruction >> 31) & 0x1)  << 12) |
                           (((instruction >> 7)  & 0x1)  << 11) |
                           (((instruction >> 25) & 0x3F) << 5)  |
                           (((instruction >> 8)  & 0xF)  << 1), 13);  // B

        case 23:   // auipc
        case 55:   // lui
            return static_cast<std::int32_t>(instruction & 0xFFFFF000); // U

        case 111:  // jal
            return signExt((((instruction >> 31) & 0x1)   << 20) |
                           (((instruction >> 12) & 0xFF)  << 12) |
                           (((instruction >> 20) & 0x1)   << 11) |
                           (((instruction >> 21) & 0x3FF) << 1), 21); // J

        default:
            return 0;
    }
}