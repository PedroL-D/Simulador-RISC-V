# Handoff do simulador RISC-V — 21/11/2026

**Ponto de retomada.** A CPU executa uma instrução por chamada a `passo()`, e o montador já traduz arquivos Assembly em palavras de 32 bits. A próxima frente discutida pelo grupo é a cache. Ela pode ser desenvolvida e testada com os programas existentes enquanto o montador ganha testes próprios.

A revisão local é a `main` no commit `d92832f`, que acrescentou o montador após a integração da CPU. No Mac usado pelo grupo, a cópia atual é o repositório dentro de **Sem Título/**. A pasta pai também contém fontes e um histórico Git mais antigo. Todos os caminhos e comandos abaixo partem da raiz da cópia atual.

## Histórico das alterações

| Data (2026) | Marco | O que passou a ser possível | Commits |
|---|---|---|---|
| 05/10 | Memória e registradores | Guardar palavras na RAM e valores nos 32 registradores, preservando o zero em `x0`. | `cf16c50` |
| 18/10 | Decoder e gerador de imediatos | Separar os campos de uma instrução de 32 bits e reconstruir o imediato de cada formato. | `cd461fb` |
| 24–28/10 | ULA e organização do código | Executar cálculos de 32 bits e integrar os componentes pelas interfaces em `include/`, com as implementações em `src/`. | `4af2b02`, `15a93fa` |
| 03/11 | Controle da ULA e controle principal | Traduzir os campos da instrução em uma operação e selecionar se o segundo operando vem de um registrador ou de um imediato. | `9d93440`, `b6bf59a` |
| 06/11 | Acessos à RAM | Ler e gravar bytes, meias-palavras e palavras. Os métodos de memória ficaram disponíveis aqui e a CPU passou a chamá-los em 20/11. | `1ed2a4f` |
| 19–20/11 | Imediatos superiores | O controle passou a distinguir `LUI`, que usa zero como base, de `AUIPC`, que usa o PC da instrução atual. | `1e2771b`, `d03396f` |
| 20/11 | Branches e saltos | O controle passou a decidir quando tomar um desvio e a selecionar o destino de `JAL` e `JALR`. A validação de `JALR` passou a exigir `funct3 = 000`. | `eea51de`, `4fcc96d` |
| 20/11 | Integração da CPU | `CPU::passo()` conectou busca, decodificação, ULA, acesso à RAM e escrita em registradores. Os testes passaram a executar pequenos programas em código de máquina. | `123295b` |
| 21/11 | Montador | O grupo passou a escrever programas em Assembly com rótulos e a gerar as palavras que a CPU já sabia executar. | `d92832f` |

## O que está funcionando

| Componente | Estado e ponto de entrada |
|---|---|
| CPU | Começa com PC igual a zero e 32 registradores. Carregue palavras com `carregar_instrucao(endereco, palavra)`, execute com `passo()` e consulte `ler_pc()` e `ler_registrador(indice)`. [Interface](include/cpu.h) · [Execução](src/cpu.cpp) |
| Controle e ULA | Cobrem as 37 instruções da tabela do projeto, incluindo loads/stores, seis branches, JAL/JALR, LUI e AUIPC. O controle escolhe os sinais e a CPU aplica seus efeitos. [Lista de instruções](README.md#instruções-implementadas) · [Controle](src/controle.cpp) |
| Memória | Instruções e dados compartilham a RAM little-endian. A CPU verifica limites e alinhamento natural antes do acesso. As leituras de byte e meia-palavra admitem extensão de sinal ou de zeros. [Memória](src/memoria.cpp) |
| Montador | Reconhece os 37 mnemônicos, registradores x0–x31 e nomes ABI. Na primeira passagem registra os endereços dos rótulos; na segunda emite as instruções. Use uma instrução por linha, sem comentários, e rótulos em linhas próprias. [Interface](include/montador.h) · [Implementação](src/montador.cpp) |

## Como ligar o montador à CPU

O [exemplo do README](README.md#do-assembly-à-execução) mostra os arquivos Assembly e C++, o comando de compilação e a saída esperada.

`montar_arquivo(caminho)` devolve as palavras em ordem. A memória da CPU é privada, por isso o caminho público para executar um arquivo é carregar esse vetor na **mesma CPU** que receberá as chamadas a `passo()`:

```cpp
CPU cpu(256);
const auto palavras = montar_arquivo("programa.s");
std::uint32_t endereco = 0;
for (auto palavra : palavras) {
    cpu.carregar_instrucao(endereco, palavra);
    endereco += 4;
}
```

O trecho usa os headers `cpu.h` e `montador.h`. A outra função do montador, `carregar_programa(caminho, mem, inicio)`, recebe uma instância de `Memoria`. Carregar em uma RAM separada não disponibiliza o programa à CPU. Com laços e saltos, a quantidade de instruções **executadas** pode diferir da quantidade de palavras **carregadas**.

## Verificação feita nesta revisão

Execute os comandos na raiz com um compilador C++17. Mantenha os `assert` habilitados para o teste da CPU.

```sh
c++ -std=c++17 -Iinclude src/*.cpp tests/teste_cpu.cpp -o /tmp/teste_riscv_cpu
/tmp/teste_riscv_cpu
```

- **CPU:** o teste versionado passou para memória, fluxo de controle e falhas. [Casos executados](tests/teste_cpu.cpp)
- **AUIPC:** o teste local passou nas 123 verificações. O arquivo `tests/teste_auipc.cpp` está presente nesta máquina, mas ainda não foi versionado. Para reproduzir localmente, use o comando acima trocando o fonte de teste e o nome do executável.
- **Assembly → CPU:** em um teste temporário, o montador gerou cinco palavras para o programa abaixo. Após sete chamadas a `passo()`, a CPU chegou a `PC = 20` com `a0 = 0`. O laço executa duas vezes. Este ensaio de integração ainda não faz parte da suíte versionada.

```asm
addi t0, zero, 2
laco:
addi t0, t0, -1
bne t0, zero, laco
sw t0, 48(zero)
lw a0, 48(zero)
```

## Próxima frente: caches de instruções e dados

**Estado:** a CPU ainda acessa a RAM diretamente. O desenho discutido é uma **proposta**: duas instâncias de uma classe de cache de mapeamento direto, cada uma com seus próprios blocos e contadores. A configuração sugerida para a primeira demonstração é de 8 linhas de 16 bytes por cache.

| Onde conectar | Primeiro critério de aceitação |
|---|---|
| Busca de instrução em `CPU::passo()` | Com a cache inicialmente vazia, buscar nos endereços 0, 4, 8 e 12 deve produzir **1 miss e 3 hits**. A carga inicial do programa ocorre antes da contagem. |
| Acesso a dados em loads/stores | Um `LW` consulta a cache de instruções na busca e a cache de dados na leitura. Preservar tamanho, ordem dos bytes e extensão de sinal da implementação atual. |
| Escrita de dados | Política sugerida: *write-through* e *no-write-allocate*. Um store atualiza a RAM. Em hit, atualiza também os bytes da linha; em miss, mantém a linha anterior. |

Antes de implementar, combinar como invalidar linhas ao recarregar um programa ou ao escrever em uma região já usada como código. Depois da integração, rodar o mesmo programa usado acima e conferir PC e registradores além dos contadores.

## Pendências para a entrega

1. Versionar um teste de integração do montador com a CPU e incluir casos de erro de Assembly.
2. Implementar e testar as duas caches com a política que o grupo escolher.
3. Integrar a interface gráfica ao carregamento do programa, à execução por passo e às estatísticas de cache.
4. Confirmar no enunciado qual entrega de ambiente é exigida (Docker, VirtualBox ou ambos) e testar o empacotamento a partir de uma cópia limpa.
