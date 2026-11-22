# As 37 instruções do simulador

Cada linha é um exemplo independente na sintaxe do montador. `t0` e `t1` são registradores temporários, `a0` é o destino em vários exemplos e `zero` sempre vale 0. Para experimentar os desvios, declare o rótulo `alvo:` em uma linha própria do programa. [Como carregar um programa](../README.md#do-assembly-à-execução) · [Nomes dos registradores](guia-de-estudo.md#nomes-e-usos-dos-registradores)

## Cálculo entre registradores

| Exemplo | Efeito |
|---|---|
| `add a0, t0, t1` | Soma os valores de `t0` e `t1` e grava os 32 bits do resultado em `a0`. |
| `sub a0, t0, t1` | Grava em `a0` o resultado de `t0 − t1`, limitado a 32 bits. |
| `and a0, t0, t1` | Faz E bit a bit. Cada bit de `a0` vale 1 se os dois bits correspondentes valem 1. |
| `or a0, t0, t1` | Faz OU bit a bit. Cada bit de `a0` vale 1 se pelo menos um dos dois bits vale 1. |
| `xor a0, t0, t1` | Faz OU exclusivo. Cada bit de `a0` vale 1 quando os dois bits diferem. |
| `sll a0, t0, t1` | Desloca `t0` para a esquerda e completa com zeros à direita. |
| `srl a0, t0, t1` | Desloca `t0` para a direita e completa com zeros à esquerda. |
| `sra a0, t0, t1` | Desloca `t0` para a direita e replica o bit de sinal à esquerda. |
| `slt a0, t0, t1` | Compara com sinal. Grava 1 se `t0 < t1` ou 0 caso contrário. |
| `sltu a0, t0, t1` | Faz a mesma comparação interpretando os dois valores sem sinal. |

Nos três deslocamentos acima, os cinco bits inferiores de `t1` escolhem a quantidade de posições (0 a 31). O resultado vai para `a0`.

## Cálculo com imediato

| Exemplo | Efeito |
|---|---|
| `addi t0, t0, -1` | Soma −1 ao valor anterior de `t0`. É o decremento do laço da demonstração. |
| `andi a0, t0, 15` | Faz E com a máscara 15 e conserva em `a0` apenas os quatro bits inferiores de `t0`. |
| `ori a0, t0, 1` | Faz OU com 1. Copia `t0` para `a0` com o bit menos significativo ligado. |
| `xori a0, t0, -1` | Faz OU exclusivo com 32 bits iguais a 1, invertendo todos os bits de `t0`. |
| `slli a0, t0, 2` | Desloca dois bits à esquerda, preenchendo com zeros. |
| `srli a0, t0, 2` | Desloca dois bits à direita, preenchendo com zeros. |
| `srai a0, t0, 2` | Desloca dois bits à direita, repetindo o bit de sinal. |
| `slti a0, t0, -1` | Grava 1 se o valor com sinal de `t0` for menor que −1. Nos demais casos, grava 0. |
| `sltiu a0, t0, -1` | Primeiro estende o sinal do imediato. Depois compara sem sinal: grava 1 se `t0` for menor que `0xFFFFFFFF`, ou 0 se for igual. |
| `lui a0, 1` | Coloca o imediato nos 20 bits superiores, com zeros nos 12 inferiores. Aqui, `a0` recebe `0x00001000` (4096). |
| `auipc a0, 1` | Soma esse mesmo imediato superior ao endereço da instrução. Com PC igual a 16, `a0` recebe 4112 e a execução segue em 20. |

## Leitura e escrita de dados

Em `48(zero)`, a ULA calcula o endereço em bytes somando o deslocamento 48 ao valor do registrador-base. Cada exemplo abaixo usa esse mesmo endereço. Uma meia-palavra ocupa dois bytes e uma palavra ocupa quatro.

| Exemplo | Efeito |
|---|---|
| `lb a0, 48(zero)` | Lê um byte e estende o sinal até 32 bits. O byte `0xFF` vira `0xFFFFFFFF`. |
| `lbu a0, 48(zero)` | Lê um byte e completa com zeros. O mesmo `0xFF` vira 255. |
| `lh a0, 48(zero)` | Lê uma meia-palavra e estende o sinal. O valor `0x8000` vira `0xFFFF8000`. |
| `lhu a0, 48(zero)` | Lê uma meia-palavra e completa com zeros. O mesmo `0x8000` vira 32768. |
| `lw a0, 48(zero)` | Lê os quatro bytes a partir de 48 e grava a palavra em `a0`. |
| `sb t1, 48(zero)` | Grava os 8 bits inferiores de `t1` no byte de endereço 48. |
| `sh t1, 48(zero)` | Grava os 16 bits inferiores de `t1` nos endereços 48 e 49. |
| `sw t1, 48(zero)` | Grava os 32 bits de `t1` nos endereços de 48 a 51, com o byte menos significativo em 48. |

## Desvios e saltos

Nos seis branches, o PC recebe o endereço de `alvo` quando a condição vale. Caso contrário, avança quatro bytes. Nos saltos, `ra` recebe o endereço de retorno (`PC + 4`) nos exemplos abaixo.

| Exemplo | Efeito |
|---|---|
| `beq t0, t1, alvo` | Desvia se os dois valores forem iguais. |
| `bne t0, t1, alvo` | Desvia se os dois valores forem diferentes. |
| `blt t0, t1, alvo` | Desvia se `t0 < t1` na comparação com sinal. |
| `bge t0, t1, alvo` | Desvia se `t0 ≥ t1` na comparação com sinal. |
| `bltu t0, t1, alvo` | Desvia se `t0 < t1` na comparação sem sinal. |
| `bgeu t0, t1, alvo` | Desvia se `t0 ≥ t1` na comparação sem sinal. |
| `jal ra, alvo` | Salta para o rótulo. O montador codifica a distância em bytes a partir do PC da própria instrução. |
| `jalr ra, t0, 0` | Salta para o valor de `t0` somado ao imediato 0, com o bit menos significativo do destino zerado. A CPU do projeto exige destino múltiplo de quatro. |

A seleção e a sintaxe dos exemplos seguem a tabela do projeto. Para conferir o comportamento de cada operação, consulte o controle e a especificação RV32I. [Montador](../src/montador.cpp) · [Controle](../src/controle.cpp) · [Especificação RV32I](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html)
