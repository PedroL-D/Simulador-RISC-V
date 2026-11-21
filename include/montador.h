#ifndef MONTADOR_H
#define MONTADOR_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "memoria.h"

// Le o arquivo texto (uma instrucao por linha) e devolve as palavras de 32 bits.
// Lanca std::runtime_error se o arquivo nao abrir ou se houver erro em alguma linha.
std::vector<std::uint32_t> montar_arquivo(const std::string& caminho);

// Le o arquivo, monta cada instrucao e grava na memoria a partir de 'endereco_inicial'
// (4 bytes por instrucao). Retorna quantas instrucoes foram gravadas.
std::size_t carregar_programa(const std::string& caminho, Memoria& mem,
                              std::uint32_t endereco_inicial = 0);

#endif
