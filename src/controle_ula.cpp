/*
 Variaveis locais:
 - 'instrucao.opcode' acessa o campo opcode da estrutura.
 - 'funct3' e 'funct7' são sópias dos campos correspondentes

 Operacoes entre registradores (OP): opcode 51 = 0x33 = 0110011
    Ex: add x3, x1, x2
 Operacoes com imediato (OP-IMM): opcode 19 = 0x13 = 0010011
    Ex: addi x3, x1, 10
    Os bits de funct7 fazem parte do imediato, exceto nos shifts,  - caso de imagiato negativo - nao obriga a ser zero
    As instrucoes reutilizam operacoes que a ULA ja possui
    O controle principal que será responsael pela origem.
    SLLI é um caso especial:
        - para desloc imediatos a interpretacao muda
            bits 31 a 25: identificam/validam a operação
            bits 24 a 20: quantidade de deslocamento, de 0 a 31
        - para SLLI os 7 bits superiores são zero e retorna SLL, senao, break e segue ate o erro

Separar campos:
    Decoded campos = decode(palavra);
Interpretar operandos:
    OperacaoULA op = selecionar_operacao(campos);
Executar o cálculo:
    std::uint32_t resultado = ULA::executar(op, a, b);

 */

#include "controle_ula.h"
#include <stdexcept>

OperacaoULA selecionar_operacao(const Decoded& instrucao) {
    const auto opcode = instrucao.opcode;
    const auto funct3 = instrucao.funct3;
    const auto funct7 = instrucao.funct7;

    // Operacoes entre registradores (formato R)
    if (opcode == 0x33) {

        // operacoes com funct7 igual a zero
        if (funct7 == 0x00) {

            // usa o funct3 para esclolher entre as 8 operacoes
            switch (funct3) {
                case 0x0: return OperacaoULA::ADD;
                case 0x1: return OperacaoULA::SLL;
                case 0x2: return OperacaoULA::SLT;
                case 0x3: return OperacaoULA::SLTU;
                case 0x4: return OperacaoULA::XOR;
                case 0x5: return OperacaoULA::SRL;
                case 0x6: return OperacaoULA::OR;
                case 0x7: return OperacaoULA::AND;
            }

        } else if (funct7 == 0x20) {
            if (funct3 == 0x0) return OperacaoULA::SUB;
            if (funct3 == 0x5) return OperacaoULA::SRA;
        }

        // ainda nao executa, soh escolhe e devolve a operacao

        // Operacoes com imediato (formato I)
    } else if (opcode == 0x13) {
        switch (funct3) {
            case 0x0: return OperacaoULA::ADD;  // ADDI
            case 0x2: return OperacaoULA::SLT;  // SLTI
            case 0x3: return OperacaoULA::SLTU; // SLTIU
            case 0x4: return OperacaoULA::XOR;  // XORI
            case 0x6: return OperacaoULA::OR;   // ORI
            case 0x7: return OperacaoULA::AND;  // ANDI
            case 0x1:
                if (funct7 == 0x00) return OperacaoULA::SLL; // SLLI
                break;
            case 0x5:
                if (funct7 == 0x00) return OperacaoULA::SRL; // SRLI - desloca preenchendo com zeros
                if (funct7 == 0x20) return OperacaoULA::SRA; // SRAI - preserva sinal
                break;
        }
    }

    throw std::invalid_argument(
        "Instrucao invalida ou nao suportada pelo controle da ULA RV32I");
}
