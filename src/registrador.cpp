#include <iostream>
#include <vector>
#include <cstdint>

class Registrador {
private:
    uint32_t registradores[32] = {0};

public:
    uint32_t ler(int reg) {
        if (reg >= 32) return 0; // apenas um teste para caso tente acessar um registradir maior que o 32
        return registradores[reg];
    }

    void escrever(int reg, uint32_t value) {
        if (reg != 0 && reg < 32) {
            registradores[reg] = value; // não pode escrever no registrador 0 pq ele e sempre 0;
        }
    }
};