#include <cstdint>

struct Decoded {
    uint8_t  opcode, rd, rs1, rs2, funct3, funct7;
};

Decoded decode(uint32_t i) {
    struct Decoded decodificada;
    return {
        decodificada.opcode = uint8_t(i & 0x7F),
        decodificada.rd = uint8_t((i >> 7)  & 0x1F),
        decodificada.rs1 = uint8_t((i >> 15) & 0x1F),
        decodificada.rs2 = uint8_t((i >> 20) & 0x1F),
        decodificada.funct3 = uint8_t((i >> 12) & 0x07),
        decodificada.funct7 = uint8_t((i >> 25) & 0x7F),
    };
}