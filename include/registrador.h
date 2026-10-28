#ifndef REGISTRADOR_H
#define REGISTRADOR_H
 
#include <cstdint>
 
// Banco com os 32 registradores do RV32I (x0 a x31).
class Registrador {
private:
    std::uint32_t registradores[32] = {0};
 
public:
    // Retorna o valor do registrador (0 se o indice for invalido).
    std::uint32_t ler(unsigned reg) const;
 
    // Escreve no registrador. x0 e sempre 0, entao escritas nele sao ignoradas.
    void escrever(unsigned reg, std::uint32_t valor);
};
 
#endif