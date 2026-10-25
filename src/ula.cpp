/*
Implementacao do metodo executar declarado na classe ULA:
- Recebe a operacao desejada e depois oeperandos de 32 bits
- Retorna o resultado da operacao (com 32 bits)

No RV32I apenas os 5 bits menos significativos determiam a quantidade de posicoes deslocadas (valores de 0 a 31).

Mascara 0x1Fu hexadecimal equivale a 00011111 em binario (%u indica unsigned int)

Operacoes de soma e subtracao com uint32_t mantem o resultado limitado a 32 bits.
- Um eventual overflow descarta os bits excedentes

Operacoes de deslocamento logico descartam os bits deslocados para fora e as posicoes liberadas a direita ou a esquerda recebem zeros.

Deslocamento aritmetico preserva o sinal do numero e preenche as posicoes a esquerda com copias do bit de sinal
- Se o deslocamento zero preserva o valor, evitando deslocar 32 bits na construcao da mascara de sinal
- A variável constante de sinal verifica se o bit mais significativo de a (bit 31) é 1 e a mascara 0x80000000 seleciona somente esse bit
    - Se for 1, o numero eh negativo na representacao de C2 e deve-se preencher os bits liberados a esquerda com 1
    - Se for 0, o numero nao eh negativo e o preenchimento sera feito com zeros
- O retorno primeiro desloca a para a direita, preenchendo com zeros e depois combina o resultado com a máscara de sinal, usando OR para inserir os bits 1 necessários à esquerda.
    - Essa implementação evita depender do comportamento do deslocamento à direita de números signed negativos.


STL e STLU

A interpretacao de operandos COM sinal retorna 1 se a for menor que b, considerando ambos como números com sinal em complemento de dois. Caso contrário, retorna 0.
- Como a e b são uint32_t, uma comparacao direta interpretaria os dois operandos como sem sinal.
- O XOR com 0x80000000 inverte o bit mais significativo dos dois valores, permitindo comparar a ordem signed por meio de uma comparação unsigned.

        Exemplo: -1 deve ser menor que 1.
        Em complemento de dois, -1 = 0xFFFFFFFF.
        Após inverter o bit de sinal:
        -1 -> 0x7FFFFFFF
        1 -> 0x80000001
        Portanto, a comparação retorna verdadeiro.


A interpretacao de operandos SEM sinal compara diretamente os operandos como uint32_t.
- Retorna 1 se a < b; caso contrário, retorna 0.
*/

#include "ula.h"
#include <stdexcept>    //para verificar a validade das operacoes

std::uint32_t ULA::executar(
    OperacaoULA op,
    std::uint32_t a,
    std::uint32_t b
) {
    const unsigned deslocamento = b & 0x1Fu;
    // deslocamento antes para determinar os 5 bits menos significativo

    // Selecao da operacao a ser executada pela ULA
    switch (op) {
        case OperacaoULA::ADD: return a + b;    // Soma dos operandos
        case OperacaoULA::SUB: return a - b;    // Subtrai b de a
        case OperacaoULA::AND: return a & b;
        case OperacaoULA::OR:  return a | b;
        case OperacaoULA::XOR: return a ^ b;
        case OperacaoULA::SLL: return a << deslocamento;    // Deslocamento logico para a esquerda
        case OperacaoULA::SRL: return a >> deslocamento;    // Deslocamento logico para a direita
        case OperacaoULA::SRA: {
            // Deslocamento aritmetico para a direita
            // Preenchimento explícito evita depender de shift de signed negativo.
            if (deslocamento == 0) return a;
            const std::uint32_t sinal = (a & 0x80000000u)
                ? (0xFFFFFFFFu << (32u - deslocamento)) : 0u;
            return (a >> deslocamento) | sinal;
        }
        case OperacaoULA::SLT:
            // Inverter o bit de sinal transforma a ordem signed em unsigned.
            return (a ^ 0x80000000u) < (b ^ 0x80000000u) ? 1u : 0u;
        case OperacaoULA::SLTU: return a < b ? 1u : 0u;
    }
    // Se nenhuma operação válida corresponder ao valor recebido, lança uma exceção para indicar erro na seleção da operação.
    throw std::invalid_argument("Operacao da ULA invalida");
}
