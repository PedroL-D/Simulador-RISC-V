# Simulador RISC-V

Projeto de Arquitetura de Computadores escrito em C++17. O simulador executa 37 instruções da base inteira de 32 bits do RISC-V (RV32I). Cada chamada a `CPU::passo()` executa uma instrução completa e permite observar a mudança no contador de programa (PC) e nos registradores.

## Como compilar e testar

É necessário um compilador com suporte a C++17. Na raiz do repositório, compile e execute o programa de teste. O arquivo de teste contém seu próprio `main()`.

```sh
c++ -std=c++17 -Iinclude src/*.cpp tests/teste_cpu.cpp -o /tmp/teste_riscv_cpu
/tmp/teste_riscv_cpu
```

O [teste da CPU](tests/teste_cpu.cpp) executa programas em código de máquina e confere registradores, PC, acessos à memória, desvios, saltos e falhas de acesso.

## Do Assembly à execução

O [montador](src/montador.cpp) traduz as 37 instruções da tabela abaixo para palavras de 32 bits. Escreva uma instrução por linha, sem comentários. Ele aceita nomes de registradores como `x1` ou `ra` e resolve rótulos de branches e de `jal` quando declarados em uma linha própria, como `laco:`.

Salve este programa como `programa.s` na raiz do repositório. Ele coloca 10 em `x1` e soma 5 para obter 15 em `x2`.

```asm
addi x1, x0, 10
addi x2, x1, 5
```

Em C++, `montar_arquivo` devolve as palavras na ordem do programa. Carregue cada palavra na CPU com intervalo de quatro bytes e execute um passo para cada uma das duas instruções deste exemplo.

```cpp
#include "cpu.h"
#include "montador.h"
#include <iostream>

int main() {
    CPU cpu(256);                              // 256 bytes de RAM, PC inicial 0
    std::uint32_t endereco = 0;
    for (auto palavra : montar_arquivo("programa.s")) {
        cpu.carregar_instrucao(endereco, palavra);
        endereco += 4;
    }

    cpu.passo();
    cpu.passo();
    std::cout << "x2 = " << cpu.ler_registrador(2)
              << ", PC = " << cpu.ler_pc() << '\n';
}
```

Salve o exemplo como `exemplo.cpp` na raiz e execute:

```sh
c++ -std=c++17 -Iinclude src/*.cpp exemplo.cpp -o /tmp/exemplo_riscv
/tmp/exemplo_riscv
```

Saída esperada: `x2 = 15, PC = 8`. Os endereços são contados em bytes e cada instrução ocupa quatro bytes.

## Instruções implementadas

| Grupo | Instruções |
|---|---|
| Operações entre registradores | ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU |
| Operações com imediato | ADDI, ANDI, ORI, XORI, SLLI, SRLI, SRAI, SLTI, SLTIU |
| Imediatos superiores | LUI, AUIPC |
| Leitura de memória | LB, LBU, LH, LHU, LW |
| Escrita em memória | SB, SH, SW |
| Desvios condicionais | BEQ, BNE, BLT, BGE, BLTU, BGEU |
| Saltos | JAL, JALR |

## Como a CPU executa uma instrução

A [CPU](src/cpu.cpp) guarda o PC, o [banco de 32 registradores](src/registrador.cpp) e a [RAM](src/memoria.cpp), compartilhada entre instruções e dados. O registrador `x0` sempre vale zero.

1. **Busca.** A CPU lê a palavra de 32 bits no endereço indicado pelo PC.
2. **Decodificação.** O [decoder](src/decoder.cpp) separa os campos da instrução. O [gerador de imediatos](src/immgen.cpp) reconstrói o valor codificado nos formatos I, S, B, U ou J.
3. **Leitura e execução.** A [unidade de controle](src/controle.cpp) e o [controle da ULA](src/controle_ula.cpp) selecionam operandos e operação. A [ULA](src/ula.cpp) calcula o resultado, compara valores ou calcula um endereço.
4. **Acesso a dados.** Em `lw x3, 8(x1)`, a CPU lê uma palavra no endereço `x1 + 8` para gravar em `x3`. Em `sw x3, 8(x1)`, escreve o conteúdo de `x3` nesse mesmo endereço. A RAM armazena o byte menos significativo no menor endereço (*little-endian*).
5. **Escrita e próximo PC.** A CPU grava em `rd` quando a instrução habilita a escrita e atualiza o PC. O caminho sequencial avança quatro bytes. Branches tomados e saltos selecionam o endereço de destino.

A CPU exige alinhamento natural: meia-palavra em múltiplo de dois e palavra em múltiplo de quatro. Instruções inválidas e endereços desalinhados geram `std::invalid_argument`. Acessos fora da RAM geram `std::out_of_range`.

## Escopo do projeto

O montador e a CPU já permitem executar programas em Assembly passo a passo pela interface C++. As próximas etapas previstas são as caches de instruções e de dados, a interface gráfica e o empacotamento do ambiente de execução.

Referência técnica: [especificação oficial RV32I](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html). Distribuído sob a [licença MIT](LICENSE).
