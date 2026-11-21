/*
Cache de dados de mapeamento direto: 8 linhas de 16 bytes (128 bytes).
    Ex: os endereços 0 e 128 disputam a linha 0. Um load com miss traz o bloco da RAM. A linha guarda os bytes originais para que LB e LBU possam ler o mesmo byte e aplicar a extensão indicada pela instrução.

Store com hit: atualiza a RAM e os bytes atingidos na linha (write-through).
Store com miss: grava na RAM e preserva a linha atual (write-around).

A escrita termina no próprio passo da CPU, sem buffer.
Cada load ou store válido conta como um acesso de dados.
*/


#ifndef CACHE_DADOS_H
#define CACHE_DADOS_H

#include "cache_direta.h"

class CacheDados : public CacheDireta {
public:
    std::uint32_t ler(const Memoria& ram, std::uint32_t endereco, unsigned bytes);
    void escrever(Memoria& ram, std::uint32_t endereco, unsigned bytes, std::uint32_t valor);
};

#endif
