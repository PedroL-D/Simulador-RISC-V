#include "cache_instrucoes.h"

std::uint32_t CacheInstrucoes::buscar(const Memoria& ram, std::uint32_t pc) {
    return ler_bytes(ram, pc, 4);
}

void CacheInstrucoes::escrever(Memoria& ram, std::uint32_t endereco, unsigned bytes, std::uint32_t valor) {
    // A carga do programa escreve na RAM sem contar como busca de instrução.
    escrever_bytes(ram, endereco, bytes, valor);
}
