#ifndef MEMORIA_H
#define MEMORIA_H
 
#include <cstddef>
#include <cstdint>
#include <vector>
 
// Memoria byte a byte, com acesso em palavras de 32 bits (little-endian).
class Memoria {
private:
    std::vector<std::uint8_t> mem;
 
public:
    // 'explicit' evita conversao implicita de um numero para Memoria.
    explicit Memoria(std::size_t tamanho);
 
    std::uint32_t ler_palavra(std::uint32_t endereco) const;
    void escrever_palavra(std::uint32_t endereco, std::uint32_t valor);
};
 
#endif