#include "memoria.h"

Memoria::Memoria(std::size_t tamanho) : mem(tamanho, 0) {}

std::uint32_t Memoria::ler_palavra(std::uint32_t endereco) const {
    if (endereco + 3 < mem.size()) {
        return static_cast<std::uint32_t>(mem[endereco]) |                // byte 1
              (static_cast<std::uint32_t>(mem[endereco + 1]) << 8)  |     // byte 2
              (static_cast<std::uint32_t>(mem[endereco + 2]) << 16) |     // byte 3
              (static_cast<std::uint32_t>(mem[endereco + 3]) << 24);      // byte 4
    }
    return 0;
}

void Memoria::escrever_palavra(std::uint32_t endereco, std::uint32_t valor) {
    if (endereco + 3 < mem.size()) {
        mem[endereco]     = static_cast<std::uint8_t>(valor);        // byte 1
        mem[endereco + 1] = static_cast<std::uint8_t>(valor >> 8);   // byte 2
        mem[endereco + 2] = static_cast<std::uint8_t>(valor >> 16);  // byte 3
        mem[endereco + 3] = static_cast<std::uint8_t>(valor >> 24);  // byte 4
    }
}