#include "cpu.h"
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <stdexcept>

static void carregar(CPU& cpu, std::initializer_list<std::uint32_t> programa) {
    std::uint32_t pc = 0;
    for (auto instrucao : programa) {
        cpu.carregar_instrucao(pc, instrucao);
        pc += 4;
    }
}

static void testar_memoria() {
    CPU cpu(128);
    carregar(cpu, {
        0x04000093, // addi x1,x0,64
        0xfff00113, // addi x2,x0,-1
        0xfe20ae23, // sw x2,-4(x1)
        0xffc0a183, // lw x3,-4(x1)
        0x08000113, // addi x2,x0,128
        0x00208023, // sb x2,0(x1)
        0x00008203, // lb x4,0(x1)
        0x0000c283, // lbu x5,0(x1)
        0x00008137, // lui x2,8  (x2 = 0x8000)
        0x00209123, // sh x2,2(x1)
        0x00209303, // lh x6,2(x1)
        0x0020d383, // lhu x7,2(x1)
        0x0000a403  // lw x8,0(x1)
    });
    for (int passo = 0; passo < 13; ++passo) cpu.passo();
    assert(cpu.ler_pc() == 52 && cpu.ler_registrador(0) == 0);
    assert(cpu.ler_registrador(3) == 0xffffffffu);
    assert(cpu.ler_registrador(4) == 0xffffff80u && cpu.ler_registrador(5) == 128);
    assert(cpu.ler_registrador(6) == 0xffff8000u && cpu.ler_registrador(7) == 0x8000);
    assert(cpu.ler_registrador(8) == 0x80000080u);
}

static void testar_fluxo() {
    CPU cpu(64);
    carregar(cpu, {
        0x00200093, // addi x1,x0,2
        0xfff08093, // addi x1,x1,-1
        0xfe009ee3, // bne x1,x0,-4
        0x00001117, // auipc x2,1
        0x008001ef, // jal x3,+8
        0x00000000, // deve ser pulada
        0x02100193, // addi x3,x0,33
        0x000181e7, // jalr x3,0(x3): destino 32, retorno 32
        0x00310233  // add x4,x2,x3
    });
    for (unsigned pc : {4u,8u,4u,8u,12u,16u,24u}) {
        cpu.passo();
        assert(cpu.ler_pc() == pc);
    }
    assert(cpu.ler_registrador(2) == 4108 && cpu.ler_registrador(3) == 20);
    for (int i = 0; i < 3; ++i) cpu.passo();
    assert(cpu.ler_pc() == 36 && cpu.ler_registrador(3) == 32);
    assert(cpu.ler_registrador(4) == 4140);
}

template<class Erro>
static void testar_falha(std::uint32_t instrucao) {
    CPU cpu(32);
    carregar(cpu, {0x00900093, instrucao}); // x1 = 9
    cpu.passo();
    try {
        cpu.passo();
        throw std::runtime_error("Instrucao deveria falhar");
    } catch (const Erro&) {
        assert(cpu.ler_pc() == 4 && cpu.ler_registrador(1) == 9);
    }
}

int main() {
    testar_memoria();
    testar_fluxo();
    testar_falha<std::invalid_argument>(0x002000efu); // JAL para PC+2
    testar_falha<std::invalid_argument>(0x00102083u); // LW desalinhado
    testar_falha<std::invalid_argument>(0x00003083u); // load com funct3 reservado
    testar_falha<std::invalid_argument>(0x00104023u); // store com funct3 reservado
    testar_falha<std::out_of_range>(0xffc02003u);    // LW fora da RAM, inclusive para x0
    testar_falha<std::out_of_range>(0x02102023u);    // SW x1,32(x0)
    std::cout << "CPU: memoria, fluxo e falhas passaram\n";
}
