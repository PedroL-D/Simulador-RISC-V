#include "cache_instrucoes.h"
#include <stdexcept>

CacheInstrucoes::Posicao CacheInstrucoes::mapear(std::uint32_t endereco) {
    // Por exemplo, 0x00 e 0x80 disputam a linha 0, com tags 0 e 1.
    const auto bloco = endereco / bytes_por_linha;
    return {bloco % numero_linhas, bloco / numero_linhas, endereco % bytes_por_linha};
}

std::uint32_t CacheInstrucoes::buscar(const Memoria& ram, std::uint32_t pc) {
    // Uma instrução alinhada ocupa quatro bytes dentro de um único bloco.
    if (pc % 4 != 0) throw std::invalid_argument("Endereco desalinhado");
    if (!ram.contem(pc, 4)) throw std::out_of_range("Acesso fora da RAM");

    const auto pos = mapear(pc);
    auto& linha = linhas[pos.indice];
    if (linha.valida && linha.tag == pos.tag) {
        ++contadores.hits;
    } else {
        ++contadores.misses;
        // Miss: substituir somente a linha indicada pelo mapeamento direto.
        // Se a RAM terminar no meio do bloco, copiar apenas os bytes existentes.
        const auto inicio = pc - pos.deslocamento;
        for (unsigned i = 0; i < bytes_por_linha && ram.contem(inicio + i, 1); ++i)
            linha.dados[i] = static_cast<std::uint8_t>(ram.load_byte_unsigned(inicio + i));
        linha.tag = pos.tag;
        linha.valida = true;
    }

    // Little-endian: o byte no endereço mais baixo ocupa os bits 7..0.
    std::uint32_t palavra = 0;
    for (unsigned i = 0; i < 4; ++i)
        palavra |= static_cast<std::uint32_t>(linha.dados[pos.deslocamento + i]) << (8 * i);
    return palavra;
}

void CacheInstrucoes::escrever(Memoria& ram, std::uint32_t endereco, unsigned bytes, std::uint32_t valor) {
    if (bytes != 1 && bytes != 2 && bytes != 4)
        throw std::invalid_argument("Largura de escrita invalida");
    if (endereco % bytes != 0) throw std::invalid_argument("Endereco desalinhado");
    if (!ram.contem(endereco, bytes)) throw std::out_of_range("Acesso fora da RAM");

    // Write-through: a RAM recebe a escrita imediatamente, em hit e em miss.
    switch (bytes) {
        case 1: ram.store_byte(endereco, static_cast<std::uint8_t>(valor)); break;
        case 2: ram.store_half(endereco, static_cast<std::uint16_t>(valor)); break;
        case 4: ram.store_word(endereco, valor); break;
    }

    const auto pos = mapear(endereco);
    auto& linha = linhas[pos.indice];
    // Write-around: um store miss não busca o bloco nem remove outro.
    if (!linha.valida || linha.tag != pos.tag) return;

    // Store hit: alterar só os bytes escritos. Alinhamento garante que caibam
    // nesta linha. Escritas do carregador também seguem esse caminho, sem
    // contar como buscas de instrução ou aquecer uma cache vazia.
    for (unsigned i = 0; i < bytes; ++i)
        linha.dados[pos.deslocamento + i] = static_cast<std::uint8_t>(valor >> (8 * i));
}
