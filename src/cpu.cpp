#include "cpu.h"
#include "controle.h"
#include "immgen.h"
#include <stdexcept>

// construtor - banco de reg com inicialização propria no header
CPU::CPU(std::size_t tamanho_memoria) : memoria(tamanho_memoria) {}

// validar o endereco
void CPU::validar_acesso(std::uint32_t endereco, unsigned bytes) const {
    // neste simulador cada acesso deve estar alinhado ao seu tamanho
    // o endereco eh divisivel pelo tamanho da palavra
    // meia palabra precisa de edereco multiplo de 2
    if (endereco % bytes != 0) throw std::invalid_argument("Endereco desalinhado");
    if (!memoria.contem(endereco, bytes)) throw std::out_of_range("Acesso fora da RAM");
}

// carregar a instrucao na RAM
// recebe o endereco e a palavra que representa a instrucao em cod de maq
void CPU::carregar_instrucao(std::uint32_t endereco, std::uint32_t instrucao) {
    cache_instrucoes.escrever(memoria, endereco, 4, instrucao);
}

// inicio da execucao de uma instrucao
void CPU::passo() {
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

    // consulta do controle para escolher como a execucao deve prosseguir
    switch (sinais.proximo_pc) {
        case ProximoPC::SEQUENCIAL: break;
        case ProximoPC::DESVIO_CONDICIONAL: // entra no caso da branches
            if (desvio_tomado(campos.funct3, resultado)) proximo_pc = pc + imediato;
            break;
        case ProximoPC::JAL: proximo_pc = resultado; break; // ULA já calculou PC atual + imediato
        case ProximoPC::JALR: proximo_pc = resultado & ~1u; break;  // ULA já calculou rs1 + imediato
    }

    // Um salto desalinhado falha antes de gravar o endereco de retorno.
    if (proximo_pc % 4 != 0) throw std::invalid_argument("Destino de desvio desalinhado");

    // 5. MEMORIA DE DADOS: LOAD le para rd; STORE grava os bits baixos de rs2.
    // Ex.: em SW x3, 8(x1), a ULA calcula x1 + 8 e o dado vem de x3.
    std::uint32_t dado_lido = 0;    // guardará o calor devolvido pela memoria ate a etapa de escrita em registrador
    if (sinais.ler_memoria) { // LOAD de dados continua consultando a RAM.
        validar_acesso(resultado, static_cast<unsigned>(sinais.largura));
        switch (sinais.largura) {
            case LarguraMemoria::BYTE:
                dado_lido = sinais.estender_sinal ? memoria.load_byte(resultado) : memoria.load_byte_unsigned(resultado);
                break;
            case LarguraMemoria::MEIA_PALAVRA:
                dado_lido = sinais.estender_sinal ? memoria.load_half(resultado) : memoria.load_half_unsigned(resultado);
                break;
            case LarguraMemoria::PALAVRA: dado_lido = memoria.load_word(resultado); break;
        }
    } else if (sinais.escrever_memoria) { // STORE: SB, SH ou SW.
        // Atualiza a RAM e, caso o bloco já esteja na cache de instruções,
        // atualiza sua cópia para a próxima busca enxergar os bytes novos.
        cache_instrucoes.escrever(memoria, resultado, static_cast<unsigned>(sinais.largura), rs2);
    }

    // 6. ESCRITA E ATUALIZACAO DO PC: o controle seleciona o valor para rd.
    // Em JAL/JALR gravamos o endereco de retorno; em store a escrita em rd
    // esta desabilitada. O proprio banco preserva x0.
    if (sinais.escrever_registrador) {  // se a inst eh escrita no reg
        auto valor_rd = resultado;  // toma a saida da ULA como valor inicial para destino
        switch (sinais.origem_resultado) {  // consulta o controle
            case OrigemResultado::ULA: break;
            case OrigemResultado::MEMORIA: valor_rd = dado_lido; break;
            case OrigemResultado::PC_MAIS_4: valor_rd = sequencial; break;
        }
        registradores.escrever(campos.rd, valor_rd);
    }
    pc = proximo_pc;
}
