/*
CPU monociclo do simulador RV32I

Cada objeto guarda o PC, os 32 registradores e a RAM de instrucoes e dados.
O PC comeca em zero. Uma chamada a passo() executa uma instrucao completa.
Os comentarios das etapas em cpu.cpp acompanham o caminho dos valores desde a busca da palavra na RAM ate a escrita em rd e a atualizacao do PC.

- Se PC = 0, a proxima busca comeca no byte 0
- O construtor permite que ao escrever CPU cpu(256); crie uma CPU com 256 bytes de RAM
- A funcao carregar_instrucao permite colocar uma instrucao de 32 bits em determinado endereco
    - como o programa entra na RAM antes da execução
- A funcao passo() é o metodo central
    - cada chamada executa a instruçao indicada pelo PC e, se ela terminar corretamente, atualiza o estado da CPU
        - Buscar instrução no endereço PC
        - Decoder separa os campos
        - Controle gera os sinais
        - Selecionar valores dos operandos
        - Executar a ULA
        - Determinar o próximo PC
        - Escrever resultado em rd, se habilitado
        - Atualizar o PC

*/


#ifndef CPU_H
#define CPU_H

#include "cache_dados.h"
#include "cache_instrucoes.h"
#include "registrador.h"


class CPU {
private:
    std::uint32_t pc = 0;   // contador de programa com 32bits e valor inicial igual a zero - o valor é um endereco em bytes
    Registrador registradores;  // cria banco de registradores dentro da CPU - contem os 32 registradores
    Memoria memoria;    // cria a RAM da CPU - instrucoes e dados ocupam a mesma memoria

    CacheInstrucoes cache_instrucoes;

    CacheDados cache_dados;

public:
    explicit CPU(std::size_t tamanho_memoria);  // declaracao do construtor

    void carregar_instrucao(std::uint32_t endereco, std::uint32_t instrucao);

    // Instrucao invalida ou endereco desalinhado: invalid_argument.
    // Acesso fora da RAM: out_of_range. A falha preserva PC, registradores e RAM; uma busca concluida conta na cache.

    void passo();   // acoes da CPU: busca-decodificacao-execucao-escrita

    ContadoresCache contadores_instrucoes() const { return cache_instrucoes.ler_contadores(); }

    ContadoresCache contadores_dados() const { return cache_dados.ler_contadores(); }

    // devolve o endereco atual do PC
    std::uint32_t ler_pc() const { return pc; }
    // consulta o banco de registradores
    std::uint32_t ler_registrador(unsigned indice) const { return registradores.ler(indice); }
};

#endif
