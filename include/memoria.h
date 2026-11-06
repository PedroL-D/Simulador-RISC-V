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

    //funções de leitura
    std::uint32_t load_word(std::uint32_t endereco) const;
    std::uint32_t load_byte(std::uint32_t endereco) const; 
    std::uint32_t load_half(std::uint32_t endereco) const;
    std::uint32_t load_byte_unsigned(std::uint32_t endereco) const;
    std::uint32_t load_half_unsigned(std::uint32_t endereco) const;

    //funções de escrita
    void store_word(std::uint32_t endereco, std::uint32_t valor);
    void store_half(std::uint32_t endereco, std::uint16_t valor);
    void store_byte(std::uint32_t endereco, std::uint8_t valor);
};
 
#endif