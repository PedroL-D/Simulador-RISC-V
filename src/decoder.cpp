#include "decoder.h"

Decoded decode(uint32_t i) {
    return {
        uint8_t(i & 0x7F),          // opcode
        uint8_t((i >> 7)  & 0x1F),  // rd
        uint8_t((i >> 15) & 0x1F),  // rs1
        uint8_t((i >> 20) & 0x1F),  // rs2
        uint8_t((i >> 12) & 0x07),  // funct3
        uint8_t((i >> 25) & 0x7F)   // funct7
    };
}