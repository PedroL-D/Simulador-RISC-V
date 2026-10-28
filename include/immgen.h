#ifndef IMMGEN_H
#define IMMGEN_H
 
#include <cstdint>
 
// Estende o sinal de um valor de 'bits' bits para 32 bits.
std::int32_t signExt(std::uint32_t valor, int bits);
 
// Extrai e monta o imediato de uma instrucao (formatos I, S, B, U e J).
std::int32_t immgen(std::uint32_t instruction);
 
#endif