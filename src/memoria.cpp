#include "memoria.h"

//nameespace anonimo apenas para nao dar conflito com o signExt em immgen.cpp
namespace {
    std::uint32_t signExt(std::uint32_t valor, int bits) {
        const int shift = 32 - bits;
        return static_cast<std::uint32_t>(
            static_cast<std::int32_t>(valor << shift) >> shift);
    }
}

Memoria::Memoria(std::size_t tamanho) : mem(tamanho, 0) {}

//Funções de leitura

//Leitura de uma palavra (4 bytes) da memória, a partir do endereço fornecido.
std::uint32_t Memoria::load_word(std::uint32_t endereco) const {
    if (endereco + 3 < mem.size()) {
        return static_cast<std::uint32_t>(mem[endereco]) |                // byte 1
              (static_cast<std::uint32_t>(mem[endereco + 1]) << 8)  |     // byte 2
              (static_cast<std::uint32_t>(mem[endereco + 2]) << 16) |     // byte 3
              (static_cast<std::uint32_t>(mem[endereco + 3]) << 24);      // byte 4
    }
    return 0;
}

//leitura de meia palavra (2 bytes) da memória, a partir do endereço fornecido.
std::uint32_t Memoria::load_half(std::uint32_t endereco) const
{
    if (endereco + 1 < mem.size()) {
        return static_cast<std::uint32_t>(signExt(load_half_unsigned(endereco), 16));
    }
    return 0;
}

//leitura de 1 byte da memória, a partir do endereço fornecido.
std::uint32_t Memoria::load_byte(std::uint32_t endereco) const
{
    if (endereco < mem.size()) {
        return static_cast<std::uint32_t>(signExt(load_byte_unsigned(endereco), 8));
    }
    return 0;
}

//leitura de 1 byte da memória, a partir do endereço fornecido, sem sinal.
std::uint32_t Memoria::load_byte_unsigned(std::uint32_t endereco) const
{
    if (endereco < mem.size()) {
        return static_cast<std::uint32_t>(mem[endereco]);
    }
    return 0;
}

//leitura de meia palavra (2 bytes) da memória, a partir do endereço fornecido, sem sinal.
std::uint32_t Memoria::load_half_unsigned(std::uint32_t endereco) const
{
    if (endereco + 1 < mem.size()) {
        return static_cast<std::uint32_t>(mem[endereco]) | (static_cast<std::uint32_t>(mem[endereco + 1]) << 8);
    }
    return 0;
}

//Funções de escrita

//Escreve 1 byte na memória, a partir do endereço fornecido.
void Memoria::store_byte(std::uint32_t endereco, std::uint8_t valor) {
    if (endereco < mem.size()) {
        mem[endereco] = valor;
    }
}

//Escreve meia palavra (2 bytes) na memória, a partir do endereço fornecido.
void Memoria::store_half(std::uint32_t endereco, std::uint16_t valor) {
    if (endereco + 1 < mem.size()) {
        mem[endereco]     = static_cast<std::uint8_t>(valor);        // byte 1
        mem[endereco + 1] = static_cast<std::uint8_t>(valor >> 8);   // byte 2
    }
}

//Escreve uma palavra (4 bytes) na memória, a partir do endereço fornecido.
void Memoria::store_word(std::uint32_t endereco, std::uint32_t valor) {
    if (endereco + 3 < mem.size()) {
        mem[endereco]     = static_cast<std::uint8_t>(valor);        // byte 1
        mem[endereco + 1] = static_cast<std::uint8_t>(valor >> 8);   // byte 2
        mem[endereco + 2] = static_cast<std::uint8_t>(valor >> 16);  // byte 3
        mem[endereco + 3] = static_cast<std::uint8_t>(valor >> 24);  // byte 4
    }
}