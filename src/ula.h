/*
 As operacoes implementadas sao:

 ADD    Soma
 SUB    Subtracao
 AND    E logico - bit a bit
 OR     OU logico - bit a bit
 XOR    OU exclusivo - bit a bit
 SLL    Deslocamento logico para a esquerda
 SRL    Deslocamento logico para a direita
 SRA    Deslocamento aritmetico para a direita - preserva o sinal
 SLT    Retorna 1 se a < b interpretando os operandos COM sinal, 0
 SLTU   Retorna 1 se a < b interpretando os operandos SEM sinal, 0

 a e b sao os operandos

 Especificacoes RV32I:
 https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html

 Combinacional: não guarda registradores, PC nem estado entre chamadas (resultados anteriores) - isso fica com outros componentes.

 O resultado depende apenas da operação e dos dois valores recebidos naquela chamada.
*/

#ifndef ULA_H
#define ULA_H

#include <cstdint>

// declaracao das operacoes
enum class OperacaoULA {
    ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU
};

// definicao da classe ULA com a funcao de execucao
class ULA {
public:
    static std::uint32_t executar(
        OperacaoULA op,
        std::uint32_t a,
        std::uint32_t b
    );
};

#endif
