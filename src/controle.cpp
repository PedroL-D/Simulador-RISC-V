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

#include "controle.h"
#include "controle_ula.h"

SinaisControle gerar_controle(const Decoded& instrucao) {
    // Valida opcode/funct3/funct7 antes de devolver qualquer sinal.
    // Reutiliza a interpretacao existente, sem duplicar a tabela de operacoes.

    // Identificar a operacao da ULA e verificar se o código pertence ao conjunto suportado
    const OperacaoULA operacao = selecionar_operacao(instrucao);

    return {
        operacao,
        OrigemA::RS1,
        instrucao.opcode == 0x13 ? OrigemB::IMEDIATO : OrigemB::RS2,
        true,  // Escrita em rd habilitada; o banco continua protegendo x0.
        false, // Nenhuma leitura de dados.
        false, // Nenhuma escrita de dados.
        OrigemResultado::ULA,
        ProximoPC::SEQUENCIAL
    };
}
