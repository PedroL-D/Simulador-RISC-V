#include "registrador.h"
 
std::uint32_t Registrador::ler(unsigned reg) const {
    if (reg >= 32) return 0; // protecao contra indice invalido
    return registradores[reg];
}
 
void Registrador::escrever(unsigned reg, std::uint32_t valor) {
    if (reg != 0 && reg < 32) { // nao pode escrever em x0, ele e sempre 0
        registradores[reg] = valor;
    }
}