/*
Unidade de Controle

Coordenar a execução da instrução
Suporta as 19 instruçoes aritméticas e lógicas do controle_ula

- Decidir de onde vem o primeiro operando: rs1 ou PC
- Determinar o segundo operando: registrador rs2 ou imediato
- Decisão de escrita ou não em um registrador
  - Ex: ADD escreve, SW não escreve
- Decisão de deve ler ou escrever na memória
  - Ex: LW leitura, SW escrita
- Determinar qual resultado vai para o rd
  - Resultado da ULA, dado da memória ou PC+4
- Como selecionar o próximo PC
  - Sequencial, desvio ou salto

Definir os sinais e implementar à UC
*/

#ifndef CONTROLE_H
#define CONTROLE_H

#include "decoder.h"
#include "ula.h"

// Origens dos valores selecionados pelos multiplexadores da futura CPU.
enum class OrigemA { RS1 };
enum class OrigemB { RS2, IMEDIATO };
enum class OrigemResultado { ULA };
enum class ProximoPC { SEQUENCIAL }; // PC + 4

// Sinais combinacionais: descrevem o que a CPU deve fazer nesta instrucao.
// Os enums serao ampliados quando houver suporte a memoria e saltos.
struct SinaisControle {
    OperacaoULA operacao_ula;   // Calculo a ser executado
    OrigemA origem_a;           // Origem do primeiro operando
    OrigemB origem_b;           // Origem do segundo operando
    bool escrever_registrador;  // Determina se a escrita em rd está habilitada
    bool ler_memoria;           // Determina se deve ler um dado na memória. Acesso a dados; nao inclui a busca da instrucao.
    bool escrever_memoria;      // Determina se deve escrever um dado na memória
    OrigemResultado origem_resultado;   // Qual resultado encaminhar para rd
    ProximoPC proximo_pc;       // Escolha do próximo PC
};

// Suporta as 19 instrucoes OP e OP-IMM do controle da ULA.
// Nao executa a instrucao nem altera registradores, memoria ou PC.
// Lanca std::invalid_argument se a codificacao for invalida ou nao suportada.
SinaisControle gerar_controle(const Decoded& instrucao);

#endif
