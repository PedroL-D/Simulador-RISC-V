/*
Unidade de Controle

Coordenar a execução da instrução
Suporta as  19 instruçoes aritméticas e lógicas do controle_ula e as instruções de desvios e saltos (JAL, JALR, BRANCH) e LUI/AUIPC, alem de loads e stores.

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
#include <stdexcept>

SinaisControle gerar_controle(const Decoded& instrucao) {
    // LUI usa o imediato U pronto do immgen: 0 + imediato.
    // Nao possui rs1/funct3/funct7: esses bits pertencem ao imediato.
    if (instrucao.opcode == 0x37) {
        return {
            OperacaoULA::ADD,
            OrigemA::ZERO,
            OrigemB::IMEDIATO,
            true,
            false,
            false,
            OrigemResultado::ULA,
            ProximoPC::SEQUENCIAL
        };
    }

    // AUIPC soma o imediato U ao PC da propria instrucao, nao ao PC + 4.
    // Assim como em LUI, os bits de funct3/funct7 fazem parte do imediato.
    if (instrucao.opcode == 0x17) {
        return {
            OperacaoULA::ADD,
            OrigemA::PC,
            OrigemB::IMEDIATO,
            true,  // rd recebe PC atual + imediato.
            false,
            false,
            OrigemResultado::ULA,
            ProximoPC::SEQUENCIAL // AUIPC nao e um salto.
        };
    }

    // LOAD (tipo I) e STORE (tipo S) usam ADD para calcular rs1 + imediato.
    // funct3 escolhe a largura. Os bits superiores pertencem ao deslocamento.
    if (instrucao.opcode == 0x03 || instrucao.opcode == 0x23) {
        const bool load = instrucao.opcode == 0x03;
        LarguraMemoria largura;
        switch (instrucao.funct3) {
            case 0: largura = LarguraMemoria::BYTE; break;         // LB / SB
            case 1: largura = LarguraMemoria::MEIA_PALAVRA; break; // LH / SH
            case 2: largura = LarguraMemoria::PALAVRA; break;      // LW / SW
            case 4: // LBU
            case 5: // LHU
                if (!load) throw std::invalid_argument("funct3 invalido para store");
                if (instrucao.funct3 == 4) {
                    largura = LarguraMemoria::BYTE;         // LBU: 1 byte
                } else {
                    largura = LarguraMemoria::MEIA_PALAVRA; // LHU: 2 bytes
                }
                break;
            default: throw std::invalid_argument("funct3 invalido para acesso a memoria");
        }
        return {
            OperacaoULA::ADD, OrigemA::RS1, OrigemB::IMEDIATO,
            load, load, !load, // Load escreve em rd e le a RAM; store escreve na RAM.
            OrigemResultado::MEMORIA, ProximoPC::SEQUENCIAL,
            largura, load && instrucao.funct3 < 2
        };
    }

    // Valida opcode/funct3/funct7 antes de devolver qualquer sinal.
    // Reutiliza a interpretacao existente, sem duplicar a tabela de operacoes.

    // Identificar a operacao da ULA e verificar se o código pertence ao conjunto suportado
    const OperacaoULA operacao = selecionar_operacao(instrucao);

    switch (instrucao.opcode) {
        case 0x33: // OP
            return { operacao, OrigemA::RS1, OrigemB::RS2,
                     true, false, false,
                     OrigemResultado::ULA, ProximoPC::SEQUENCIAL };

        case 0x13: // OP-IMM
            return { operacao, OrigemA::RS1, OrigemB::IMEDIATO,
                     true, false, false,
                     OrigemResultado::ULA, ProximoPC::SEQUENCIAL };

        case 0x63: // BRANCH: ULA compara rs1 e rs2, nao escreve em rd
            return { operacao, OrigemA::RS1, OrigemB::RS2,
                     false, false, false,
                     OrigemResultado::ULA, ProximoPC::DESVIO_CONDICIONAL };

        case 0x6F: // JAL: ULA calcula PC + imm; rd recebe PC + 4
            return { operacao, OrigemA::PC, OrigemB::IMEDIATO,
                     true, false, false,
                     OrigemResultado::PC_MAIS_4, ProximoPC::JAL };

        case 0x67: // JALR: ULA calcula rs1 + imm; rd recebe PC + 4
            return { operacao, OrigemA::RS1, OrigemB::IMEDIATO,
                     true, false, false,
                     OrigemResultado::PC_MAIS_4, ProximoPC::JALR };
    }

    throw std::invalid_argument("Opcode nao suportado pela unidade de controle");

}

bool desvio_tomado(std::uint8_t funct3, std::uint32_t r) {
    switch (funct3) {
        case 0x0: return r == 0;  // beq:  rs1 - rs2 == 0
        case 0x1: return r != 0;  // bne
        case 0x4: return r == 1;  // blt:  SLT deu 1
        case 0x5: return r == 0;  // bge:  SLT deu 0
        case 0x6: return r == 1;  // bltu: SLTU deu 1
        case 0x7: return r == 0;  // bgeu: SLTU deu 0
    }
    return false;
}
