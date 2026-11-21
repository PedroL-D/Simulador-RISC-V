/*
Cache de instruções de mapeamento direto: 8 linhas de 16 bytes (128 bytes).
Cada bloco da RAM só pode ocupar uma linha. Para um endereço em bytes:
    bloco = endereço / 16
    índice = bloco % 8
    tag   = bloco / 8
    deslocamento = endereço % 16
O índice escolhe a linha. A tag identifica qual dos blocos que disputam essa linha está nela. A linha começa inválida, inclusive quando a tag é zero.

Busca com hit: entrega a instrução da linha. Com miss: copia o bloco da RAM.

A RAM é compartilhada com os dados. Para manter o código atualizado, SB, SH, SW e a carga do programa usam write-through com write-around:
- escrita com hit: atualiza os bytes correspondentes na linha e na RAM;
- escrita com miss: grava na RAM e preserva a linha existente.
Assim a RAM já tem o valor novo após cada store, sem bit de linha modificada.

Cada posição guarda um byte original: 0xFF continua sendo 0xFF. A extensão para 0xFFFFFFFF (LB) ou 0x000000FF (LBU) acontece na leitura de dados.
Essas leituras ainda usam a RAM. Os contadores abaixo medem buscas de instrução.
*/


#ifndef CACHE_INSTRUCOES_H
#define CACHE_INSTRUCOES_H

#include "memoria.h"
#include <array>
#include <cstdint>

struct ContadoresCache {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
};

class CacheInstrucoes {
public:
    std::uint32_t buscar(const Memoria& ram, std::uint32_t pc);
    void escrever(Memoria& ram, std::uint32_t endereco, unsigned bytes, std::uint32_t valor);
    ContadoresCache ler_contadores() const { return contadores; }

private:
    // Quatro instruções por bloco permitem observar hits em buscas sequenciais.
    static constexpr unsigned bytes_por_linha = 16;
    // Com oito linhas, um programa pequeno já demonstra conflitos entre 0 e 128.
    static constexpr unsigned numero_linhas = 8;
    struct Linha {
        bool valida = false;
        std::uint32_t tag = 0;
        std::array<std::uint8_t, bytes_por_linha> dados{};
    };
    struct Posicao {
        unsigned indice;
        std::uint32_t tag;
        unsigned deslocamento;
    };
    static Posicao mapear(std::uint32_t endereco);
    std::array<Linha, numero_linhas> linhas{};
    ContadoresCache contadores;
};
#endif
