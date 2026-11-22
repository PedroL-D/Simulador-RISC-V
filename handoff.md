# Handoff do simulador RISC-V — 22/11/2026

**Ponto de retomada.** A revisão `844954b` inclui a interface Dear ImGui ligada à CPU e a imagem Docker com acesso pelo navegador. A versão de trabalho acrescenta o datapath do último passo e o layout inicial que acomoda os 32 registradores. Para a demonstração, acompanhe o exemplo do laço pelo [guia de estudo e apresentação](docs/guia-de-estudo.md). [Como iniciar o simulador](README.md#abrir-o-simulador-no-navegador)

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
| 21/11 | Cache de dados e exemplos de integração | Loads e stores usam uma segunda cache. Dois programas Assembly exercitam o caminho do montador até a CPU. | `d274496`, `6aefb4f` |
| 22/11 | Interface gráfica e container | Pedro integrou os controles e seis painéis à CPU, incluiu o Dear ImGui e adicionou o ambiente Docker e a verificação no GitHub Actions. | `0d80517` |
| 22/11 | Correção do build | O Dockerfile passou a compilar `gui/*.cpp`, incluindo o arquivo real `gui/interface.cpp`. | `844954b` |

## O que está funcionando

| Componente | Estado e ponto de entrada |
|---|---|
| CPU | Começa com PC igual a zero e 32 registradores. Carregue palavras com `carregar_instrucao(endereco, palavra)`, execute com `passo()` e consulte `ler_pc()` e `ler_registrador(indice)`. [Interface](include/cpu.h) · [Execução](src/cpu.cpp) |
| Controle e ULA | Cobrem as 37 instruções da tabela do projeto, incluindo loads/stores, seis branches, JAL/JALR, LUI e AUIPC. O controle escolhe os sinais e a CPU aplica seus efeitos. [Lista de instruções](README.md#instruções-implementadas) · [Controle](src/controle.cpp) |
| Memória | Instruções e dados compartilham a RAM little-endian. A CPU verifica limites e alinhamento natural antes do acesso. As leituras de byte e meia-palavra admitem extensão de sinal ou de zeros. [Memória](src/memoria.cpp) |
| Montador | Reconhece os 37 mnemônicos, registradores x0–x31 e nomes ABI. Na primeira passagem registra os endereços dos rótulos; na segunda emite as instruções. Use uma instrução por linha, sem comentários, e rótulos em linhas próprias. [Interface](include/montador.h) · [Implementação](src/montador.cpp) |

## Integração da interface

A GUI mantém uma CPU com 4096 bytes de RAM. **Carregar** monta o arquivo, verifica se o programa cabe na RAM e inicializa a CPU. **Reset** restaura o programa já montado. **Passo** e **Executar** usam a mesma rotina de execução. A GUI compara os contadores antes e depois da execução para distinguir hit, miss e ausência de acesso a dados. O diagrama combina os hits e misses com os operandos, a palavra buscada, o resultado da ULA e o próximo PC obtidos de `passo_com_detalhes()`. A leitura para desenhar a janela preserva os contadores. **Programa** lista o código carregado. Quando um store altera uma instrução, **Ultimo passo** e o datapath usam a palavra efetivamente buscada. [Implementação da interface](gui/interface.cpp)

O processo principal do container é `/app/simulador_gui`. O script prepara o display virtual `:99` em 1280 × 914, ativa o OpenGL por software, inicia o VNC na porta interna 5900 e serve o noVNC na 6080. A imagem inclui os dois exemplos em `/app/exemplos`. Os testes C++ são executados a partir do repositório. [Dockerfile](Dockerfile) · [Inicialização](docker/iniciar.sh) · [Arquivos excluídos do build](.dockerignore)

## Verificação de 22/11/2026

- **Núcleo.** Os quatro testes C++17 foram recompilados e passaram no Mac com os fontes da versão de trabalho, incluindo os dois exemplos Assembly e o diagnóstico de rótulo ausente. [Comandos dos testes](README.md#como-compilar-e-testar)
- **Container.** O workflow do commit `844954b` passou no Ubuntu 24.04. Ele construiu a imagem, iniciou o container, obteve resposta HTTP de `/vnc.html`, encontrou o processo da GUI e recebeu a assinatura RFB do servidor VNC. O workflow roda em pushes para `main`, em pull requests e por acionamento manual. [Execução aprovada](https://github.com/PedroL-D/Simulador-RISC-V/actions/runs/45030354900) · [Workflow](.github/workflows/docker.yml)
- **Conferência visual.** A [captura da interface nativa](docs/imagens/simulador-laco.png) registra o 12º passo do laço, com os 32 registradores e o datapath visíveis. A alteração do display virtual para 1280 × 914 ainda precisa de conferência visual pelo navegador. A execução do workflow citada acima corresponde ao commit anterior a esse ajuste.

| Arquivo para abrir | Conferência na interface |
|---|---|
| [laco_memoria.txt](exemplos/laco_memoria.txt) | Após o 4º e o 7º passos, o PC volta a 4. Após o 10º, segue para 16. No 12º, o PC chega a 24 e `a0` recebe 6. |
| [conflito_cache.txt](exemplos/conflito_cache.txt) | Após o 4º passo, `a0 = 0xFFFFFFFF` e `a1 = 255`. O 6º passo grava em 192 com miss. O 7º ainda lê 64 com hit. Os dois últimos loads dão miss por conflito. |

Os valores finais das duas caches estão na [tabela dos exemplos](README.md#exemplos-para-executar-por-passos).

## Sugestões para revisões futuras

1. **Inspecionar a RAM e as linhas de cache.** Uma consulta somente de leitura permitiria ver os bytes em 48 após o `sw` e a mudança de tag quando 64 e 192 disputam a linha 4. Abrir ou redesenhar o painel deve preservar os contadores.
2. **Executar os quatro testes C++ no GitHub Actions.** O workflow atual verifica a inicialização do container. Rodar também os testes do núcleo facilitaria detectar mudanças no PC, nos registradores e nos contadores antes de integrar uma alteração.
3. **Conferir a demonstração pelo noVNC.** Usar a tabela acima para repetir os dois exemplos no navegador, testar **Pausar** e **Reset** e conferir se o layout inicial exibe de `x0` a `x31`. Após o reset, PC e contadores devem voltar a zero.
