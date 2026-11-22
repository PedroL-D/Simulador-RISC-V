#include "cpu.h"
#include "controle.h"
#include "immgen.h"
#include <stdexcept>

// construtor - banco de reg com inicialização propria no header
CPU::CPU(std::size_t tamanho_memoria) : memoria(tamanho_memoria) {}

// carregar a instrucao na RAM
// recebe o endereco e a palavra que representa a instrucao em cod de maq
void CPU::carregar_instrucao(std::uint32_t endereco, std::uint32_t instrucao) {
    cache_instrucoes.escrever(memoria, endereco, 4, instrucao);
    // Uma palavra recarregada também pode estar em uma linha de dados.
    cache_dados.sincronizar_escrita(endereco, 4, instrucao);
}

// inicio da execucao de uma instrucao
void CPU::passo() {
    passo_com_detalhes();
}

PassoCPU CPU::passo_com_detalhes() {
    // 1. BUSCA: o PC aponta para o primeiro dos 4 bytes da instrucao.
    // Ate o fim do passo, pc continua sendo o endereco da instrucao atual.
    const auto palavra = cache_instrucoes.buscar(memoria, pc);

    // 2. DECODIFICACAO E CONTROLE: separar os campos, interpretar os sinais e reconstruir o imediato (inclusive deslocamentos negativos).
    const auto campos = decode(palavra);    // separa os campos da inst
    const auto sinais = gerar_controle(campos); // interpreta os campos e devolve as decisoes para executar a instrucao
    const auto imediato = static_cast<std::uint32_t>(immgen(palavra));  // reconstroi o imediato no formato correto

    // 3. LEITURA: colher os operandos antes da escrita em rd.
    // Ex: JALR pode usar o mesmo registrador como origem e destino.
    auto a = registradores.ler(campos.rs1); // usa inicialmente o registrador rs1 como operando a - declarada como auto pq a variavel pode mudar
    if (sinais.origem_a == OrigemA::PC) a = pc;       // AUIPC, JAL - se o controle chama pc, atualiza a pelo end da inst atual
    if (sinais.origem_a == OrigemA::ZERO) a = 0;      // LUI - se o controle pede zero, substitui a por zero
    const auto rs2 = registradores.ler(campos.rs2); // leitura do segundo registrador
    const auto b = sinais.origem_b == OrigemB::IMEDIATO ? imediato : rs2; // condicao para escolha do segundo comando da ULA

    // 4. EXECUCAO: a ULA faz a operacao escolhida pelo controle.
    // Em load/store a soma produz o endereco efetivo em bytes.
    const auto resultado = ULA::executar(sinais.operacao_ula, a, b);    // executa o calculo escolhido pelo controle
    const auto sequencial = pc + 4u;    // calcula o end da instrucao seguinte
    auto proximo_pc = sequencial;   // suponhe que seguira em sequencia

    bool tomou_desvio = false;
    // consulta do controle para escolher como a execucao deve prosseguir
    switch (sinais.proximo_pc) {
        case ProximoPC::SEQUENCIAL: break;
        case ProximoPC::DESVIO_CONDICIONAL: // entra no caso da branches
            tomou_desvio = desvio_tomado(campos.funct3, resultado);
            if (tomou_desvio) proximo_pc = pc + imediato;
            break;
        case ProximoPC::JAL: proximo_pc = resultado; break; // ULA já calculou PC atual + imediato
        case ProximoPC::JALR: proximo_pc = resultado & ~1u; break;  // ULA já calculou rs1 + imediato
    }

    // Um salto desalinhado falha antes de gravar o endereco de retorno.
    if (proximo_pc % 4 != 0) throw std::invalid_argument("Destino de desvio desalinhado");

    // 5. MEMORIA DE DADOS: LOAD le para rd; STORE grava os bits baixos de rs2.
    // Ex.: em SW x3, 8(x1), a ULA calcula x1 + 8 e o dado vem de x3.
    std::uint32_t dado_lido = 0;
    const auto bytes = static_cast<unsigned>(sinais.largura);
    if (sinais.ler_memoria) {
        dado_lido = cache_dados.ler(memoria, resultado, bytes);
        // LB e LH estendem o sinal depois da leitura. LBU e LHU já têm zeros nos bits altos.
        if (sinais.estender_sinal && bytes < 4)
            dado_lido = static_cast<std::uint32_t>(signExt(dado_lido, static_cast<int>(8 * bytes)));
    } else if (sinais.escrever_memoria) {
        cache_dados.escrever(memoria, resultado, bytes, rs2);
        // O store já gravou na RAM. Atualize a cópia de código caso esteja residente.
        cache_instrucoes.sincronizar_escrita(resultado, bytes, rs2);
    }

    // 6. ESCRITA E ATUALIZACAO DO PC: o controle seleciona o valor para rd.
    // Em JAL/JALR gravamos o endereco de retorno; em store a escrita em rd
    // esta desabilitada. O proprio banco preserva x0.
    auto valor_rd = resultado;
    if (sinais.escrever_registrador) {  // se a inst eh escrita no reg
        switch (sinais.origem_resultado) {  // consulta o controle
            case OrigemResultado::ULA: break;
            case OrigemResultado::MEMORIA: valor_rd = dado_lido; break;
            case OrigemResultado::PC_MAIS_4: valor_rd = sequencial; break;
        }
        registradores.escrever(campos.rd, valor_rd);
    }
    const auto pc_antes = pc;
    pc = proximo_pc;
    return {pc_antes, pc, palavra, campos, sinais, imediato, a, b, resultado, rs2, valor_rd, tomou_desvio};
}
