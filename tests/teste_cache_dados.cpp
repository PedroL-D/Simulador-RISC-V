#include "cpu.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

static void testar_mapeamento_e_escrita() {
    Memoria ram(133); // O último bloco tem apenas cinco bytes.
    CacheDados cache;
    cache.escrever(ram, 0, 4, 0x11223344); // miss de escrita, sem alocação
    assert(ram.load_word(0) == 0x11223344);
    assert(cache.ler(ram, 0, 4) == 0x11223344); // miss de leitura, traz o bloco
    assert(cache.ler_contadores().misses == 2 && cache.ler_contadores().hits == 0);

    cache.escrever(ram, 1, 1, 0xFF);
    cache.escrever(ram, 2, 2, 0x8000);
    assert(ram.load_word(0) == 0x8000FF44);
    cache.escrever(ram, 128, 4, 0xAABBCCDD); // write-around preserva a linha de 0
    assert(ram.load_word(128) == 0xAABBCCDD);
    assert(cache.ler(ram, 0, 4) == 0x8000FF44);
    assert(cache.ler_contadores().hits == 3 && cache.ler_contadores().misses == 3);

    assert(cache.ler(ram, 128, 4) == 0xAABBCCDD); // conflito substitui a linha
    assert(cache.ler(ram, 132, 1) == 0);          // último byte válido ainda é hit
    try { cache.escrever(ram, 132, 2, 0xFFFF); assert(false); } catch (const std::out_of_range&) {}
    try { cache.ler(ram, 129, 2); assert(false); } catch (const std::invalid_argument&) {}
    assert(ram.load_byte_unsigned(132) == 0);
    assert(cache.ler(ram, 0, 4) == 0x8000FF44);
    assert(cache.ler_contadores().hits == 4 && cache.ler_contadores().misses == 5);
}

static void testar_caches_na_cpu() {
    CPU cpu(32);
    cpu.carregar_instrucao(0, 0x00C02083);  // lw x1,12(x0)
    cpu.carregar_instrucao(4, 0x00C02103);  // lw x2,12(x0)
    cpu.carregar_instrucao(8, 0x00202623);  // sw x2,12(x0)
    cpu.carregar_instrucao(12, 0x00100193); // addi x3,x0,1
    cpu.passo(); // Busca e load no mesmo bloco: cada cache tem seu próprio miss.
    assert(cpu.ler_registrador(1) == 0x00100193);
    assert(cpu.contadores_dados().misses == 1 && cpu.contadores_dados().hits == 0);
    assert(cpu.contadores_instrucoes().misses == 1 && cpu.contadores_instrucoes().hits == 0);

    // A recarga deve atualizar as duas cópias residentes sem contar acesso.
    cpu.carregar_instrucao(12, 0x00900193); // addi x3,x0,9
    cpu.passo();
    assert(cpu.ler_registrador(2) == 0x00900193);
    cpu.carregar_instrucao(12, 0x00100193);
    cpu.passo(); // SW restaura a instrução que soma 9 nas duas caches e na RAM.
    cpu.passo();
    assert(cpu.ler_registrador(3) == 9);
    assert(cpu.contadores_dados().misses == 1 && cpu.contadores_dados().hits == 2);
    assert(cpu.contadores_instrucoes().misses == 1 && cpu.contadores_instrucoes().hits == 3);
}

int main() {
    testar_mapeamento_e_escrita();
    testar_caches_na_cpu();
    std::cout << "Cache de dados: mapeamento, escritas e integração com a CPU passaram\n";
}
