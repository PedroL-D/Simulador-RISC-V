# Handoff do simulador RISC-V — 21/11/2026

**Ponto de retomada.** A CPU executa uma instrução por chamada a `passo()`, e o montador já traduz arquivos Assembly em palavras de 32 bits. A CPU busca código pela cache de instruções e executa loads e stores pela cache de dados.

No Mac do grupo, a cópia atual do repositório fica em **Sem Título/**. Para quem clonar do GitHub, os caminhos abaixo partem diretamente da raiz do clone.

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
| 21/11 | Cache de instruções | A busca pelo PC passou a consultar linhas de 16 bytes e a contar hits e misses. | `76ee6d9` |
| 21/11 | Cache de dados e exemplos de integração | Loads e stores usam uma segunda cache. Dois programas Assembly exercitam o caminho do montador até a CPU. | Nesta entrega |

## O que está funcionando

| Componente | Estado e ponto de entrada |
|---|---|
| CPU | Começa com PC igual a zero e 32 registradores. Carregue palavras com `carregar_instrucao(endereco, palavra)`, execute com `passo()` e consulte `ler_pc()` e `ler_registrador(indice)`. [Interface](include/cpu.h) · [Execução](src/cpu.cpp) |
| Controle e ULA | Cobrem as 37 instruções da tabela do projeto, incluindo loads/stores, seis branches, JAL/JALR, LUI e AUIPC. O controle escolhe os sinais e a CPU aplica seus efeitos. [Lista de instruções](README.md#instruções-implementadas) · [Controle](src/controle.cpp) |
| Memória | Instruções e dados compartilham a RAM little-endian. A CPU verifica limites e alinhamento natural antes do acesso. As leituras de byte e meia-palavra admitem extensão de sinal ou de zeros. [Memória](src/memoria.cpp) |
| Montador | Reconhece os 37 mnemônicos, registradores x0–x31 e nomes ABI. Na primeira passagem registra os endereços dos rótulos; na segunda emite as instruções. Use uma instrução por linha, sem comentários, e rótulos em linhas próprias. [Interface](include/montador.h) · [Implementação](src/montador.cpp) |

## Integração da interface

1. **Abrir o arquivo.** Monte o arquivo de texto com as instruções em Assembly com `montar_arquivo(caminho)`. Se houver erro, mostre a mensagem da exceção. O [arquivo de teste com rótulo ausente](tests/entradas/rotulo_ausente.txt) deve indicar a linha 2 e o nome `destino`.
2. **Iniciar a sessão.** Crie uma CPU para o novo programa, carregue as palavras nos endereços 0, 4, 8… com `carregar_instrucao()` e mantenha essa instância entre os cliques. Os dois exemplos usam 256 bytes de RAM. O [trecho de integração no README](README.md#do-assembly-à-execução) mostra a chamada.
3. **Executar um passo.** Chame `passo()` uma vez e leia `ler_pc()`, `ler_registrador(indice)`, `contadores_instrucoes()` e `contadores_dados()` para atualizar a tela. O PC retornado indica a próxima instrução. Nos exemplos, pare ao atingir `4 * palavras.size()`. Um laço pode executar mais passos que o número de palavras carregadas.
4. **Inspecionar memória e caches.** A API pública atual expõe PC, registradores e contadores. Para exibir bytes da RAM ou validade, tag e conteúdo de cada linha, falta uma consulta somente de leitura. Essa consulta deve preservar linhas e contadores a cada atualização da tela.

## Como conferir a entrega

Rode os [comandos de compilação e teste](README.md#como-compilar-e-testar) na raiz. Os quatro executáveis passaram nesta revisão. O teste da CPU cobre memória, fluxo (inclusive AUIPC) e falhas de acesso. Os dois testes das caches cobrem conflitos, políticas de escrita e atualização de código. O [teste do montador com a CPU](tests/teste_montador_cpu.cpp) abre os mesmos arquivos que a interface pode carregar e verifica também um erro de montagem.

| Arquivo para abrir | Conferência na interface |
|---|---|
| [laco_memoria.txt](exemplos/laco_memoria.txt) | Após o 4º e o 7º passos, o PC volta a 4. Após o 10º, segue para 16. No 12º, o PC chega a 24 e `a0` recebe 6. |
| [conflito_cache.txt](exemplos/conflito_cache.txt) | Após o 4º passo, `a0 = 0xFFFFFFFF` e `a1 = 255`. O 6º passo grava em 192 com miss. O 7º ainda lê 64 com hit. Os dois últimos loads dão miss por conflito. |

Os valores finais das duas caches estão na [tabela dos exemplos](README.md#exemplos-para-executar-por-passos).

## Pendências para a entrega

1. Integrar e conferir o carregamento dos exemplos, o botão de passo e a exibição de estado na interface gráfica.
2. Confirmar no enunciado o ambiente de entrega exigido (Docker, VirtualBox ou ambos) e testar o empacotamento a partir de uma cópia limpa.
