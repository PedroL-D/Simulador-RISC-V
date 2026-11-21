#include "cache_dados.h"

std::uint32_t CacheDados::ler(const Memoria& ram, std::uint32_t endereco, unsigned bytes) {
    // O bloco fornece 1, 2 ou 4 bytes em little-endian. A CPU interpreta o sinal.
    return ler_bytes(ram, endereco, bytes);
}

void CacheDados::escrever(Memoria& ram, std::uint32_t endereco, unsigned bytes, std::uint32_t valor) {
    // A RAM recebe o valor em ambos os casos. O retorno indica se a linha já existia.
    const bool hit = escrever_bytes(ram, endereco, bytes, valor);
    contar_acesso(hit);
}
