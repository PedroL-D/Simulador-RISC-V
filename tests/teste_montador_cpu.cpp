#include "cpu.h"
#include "montador.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

// Mesmo caminho da interface: montar o arquivo e carregar as palavras na CPU do programa.
static void carregar(CPU& cpu, const std::string& caminho, std::size_t quantidade_esperada) {
    const auto palavras = montar_arquivo(caminho);
    assert(palavras.size() == quantidade_esperada);
    std::uint32_t endereco = 0;
    for (auto palavra : palavras) {
        cpu.carregar_instrucao(endereco, palavra);
        endereco += 4;
    }
}

static void testar_laco() {
    CPU cpu(256);
    carregar(cpu, "exemplos/laco_memoria.txt", 6);
    // O branch volta ao endereço 4 duas vezes. Na terceira passagem, segue para o store.
    for (unsigned proximo_pc : {4u, 8u, 12u, 4u, 8u, 12u, 4u, 8u, 12u, 16u, 20u, 24u}) {
        const auto passo = cpu.passo_com_detalhes();
        assert(cpu.ler_pc() == proximo_pc && passo.pc_depois == proximo_pc);
        switch (passo.pc) {
            case 8: // ADDI t0,t0,-1: o operando A conserva o valor anterior à escrita.
                assert(passo.operando_b == 0xffffffffu && passo.operando_a == passo.valor_rd + 1);
                break;
            case 12:
                assert(passo.tomou_desvio == (proximo_pc == 4));
                break;
            case 16: // SW: endereço e dado têm origens diferentes.
                assert(passo.sinais.escrever_memoria && passo.resultado_ula == 48 && passo.valor_rs2 == 6);
                break;
            case 20: // LW: o valor devolvido pela memória vai para a0.
                assert(passo.sinais.ler_memoria && passo.resultado_ula == 48);
                assert(passo.campos.rd == 10 && passo.valor_rd == 6);
                break;
        }
    }
    assert(cpu.ler_registrador(5) == 0 && cpu.ler_registrador(6) == 6);
    assert(cpu.ler_registrador(10) == 6); // a0 recebeu a soma lida no endereço 48.
    assert(cpu.contadores_instrucoes().hits == 10 && cpu.contadores_instrucoes().misses == 2);
    assert(cpu.contadores_dados().hits == 0 && cpu.contadores_dados().misses == 2);
}

static void testar_conflito() {
    CPU cpu(256);
    carregar(cpu, "exemplos/conflito_cache.txt", 9);
    // Variação esperada na cache de dados a cada instrução, começando pelo ADDI.
    const unsigned hits_acumulados[]   = {0, 0, 0, 1, 2, 2, 3, 3, 3};
    const unsigned misses_acumulados[] = {0, 1, 2, 2, 2, 3, 3, 4, 5};
    for (unsigned passo = 0; passo < 9; ++passo) {
        cpu.passo();
        const auto dados = cpu.contadores_dados();
        assert(dados.hits == hits_acumulados[passo] && dados.misses == misses_acumulados[passo]);
    }
    assert(cpu.ler_pc() == 36);
    assert(cpu.ler_registrador(10) == 0xFFFFFFFFu && cpu.ler_registrador(11) == 0xFF);
    assert(cpu.ler_registrador(12) == 0xFFFF && cpu.ler_registrador(13) == 0xFFFFFFFFu);
    assert(cpu.ler_registrador(14) == 0xFFFF);
    assert(cpu.contadores_instrucoes().hits == 6 && cpu.contadores_instrucoes().misses == 3);
}

static void testar_erro_de_montagem() {
    try {
        montar_arquivo("tests/entradas/rotulo_ausente.txt");
    } catch (const std::runtime_error& erro) {
        const std::string mensagem = erro.what();
        assert(mensagem.find("linha 2:") != std::string::npos);
        assert(mensagem.find("rotulo desconhecido: destino") != std::string::npos);
        return;
    }
    assert(false && "O montador deveria rejeitar o rotulo ausente");
}

int main() {
    testar_laco();
    testar_conflito();
    testar_erro_de_montagem();
    std::cout << "Montador e CPU: dois exemplos e diagnóstico de rótulo passaram\n";
}
