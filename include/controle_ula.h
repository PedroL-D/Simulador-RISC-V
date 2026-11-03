#ifndef CONTROLE_ULA_H
#define CONTROLE_ULA_H

#include "decoder.h"    // Recebe campos separados pelo decoder
#include "ula.h"        // Devolve a operação que a ULA deve executar

/*
Seleciona a operacao para OP (0x33) e OP-IMM (0x13) do RV32I.
Nao escolhe operandos, nao escreve registradores e nao atualiza o PC.
Lanca std::invalid_argument para codificacoes invalidas ou nao suportadas.
*/

OperacaoULA selecionar_operacao(const Decoded& instrucao);

#endif
