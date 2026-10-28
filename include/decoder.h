#ifndef DECODER_H
#define DECODER_H
 
#include <cstdint>
 
// Campos de uma instrucao RV32I ja separados.
// A ordem dos campos importa para a inicializacao com { ... }.
struct Decoded {
    std::uint8_t opcode, rd, rs1, rs2, funct3, funct7;
};
 
// Separa os campos de uma instrucao de 32 bits.
Decoded decode(std::uint32_t i);

#endif
 