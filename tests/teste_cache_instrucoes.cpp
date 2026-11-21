#include "cpu.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    // Programa sequencial: o carregador deixa a cache fria.
    CPU cpu(32);
    for (unsigned pc = 0; pc < 16; pc += 4)
        cpu.carregar_instrucao(pc, 0x00108093); // addi x1,x1,1
    for (unsigned i = 0; i < 4; ++i) cpu.passo();
    auto stats = cpu.contadores_instrucoes();
    assert(stats.misses == 1 && stats.hits == 3 && cpu.ler_registrador(1) == 4);

    Memoria ram(132); // último bloco contém só uma palavra
    CacheInstrucoes cache;
    cache.escrever(ram, 0, 4, 0x12345678);
    assert(cache.buscar(ram, 0) == 0x12345678);
    cache.escrever(ram, 1, 1, 0xFF);
    cache.escrever(ram, 2, 2, 0x89AB);
    assert(ram.load_word(0) == 0x89ABFF78 && cache.buscar(ram, 0) == 0x89ABFF78);
    // Mesmo índice, outra tag: a escrita em 128 não pode expulsar a linha 0.
    cache.escrever(ram, 128, 4, 0xABCDEF01);
    assert(ram.load_word(128) == 0xABCDEF01 && cache.buscar(ram, 0) == 0x89ABFF78);
    assert(cache.buscar(ram, 128) == 0xABCDEF01); // miss de conflito
    assert(cache.buscar(ram, 0) == 0x89ABFF78);   // miss ao voltar
    stats = cache.ler_contadores();
    assert(stats.hits == 2 && stats.misses == 3);
    try { cache.buscar(ram, 132); assert(false); } catch (const std::out_of_range&) {}
    assert(cache.ler_contadores().misses == 3 && cache.ler_contadores().hits == 2);

    // SB modifica o byte 2 da próxima instrução, já trazida na primeira busca.
    CPU codigo(16);
    codigo.carregar_instrucao(0, 0x07000093); // addi x1,x0,112 (0x70)
    codigo.carregar_instrucao(4, 0x00100523); // sb x1,10(x0)
    codigo.carregar_instrucao(8, 0x00100113); // addi x2,x0,1 -> addi x2,x0,7
    codigo.passo();
    codigo.passo();
    codigo.passo();
    assert(codigo.ler_registrador(2) == 7);
    codigo.carregar_instrucao(12, 0x00900113); // recarga de código residente
    codigo.passo();
    assert(codigo.ler_registrador(2) == 9);
    assert(codigo.contadores_instrucoes().hits == 3 && codigo.contadores_instrucoes().misses == 1);
    std::cout << "Cache de instrucoes: busca, conflitos e escritas passaram\n";
}
