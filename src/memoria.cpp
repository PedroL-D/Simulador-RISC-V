#include <vector>
#include <cstdint>

class Memoria {
private:
    std::vector<uint8_t> mem; 

public:
    Memoria(size_t tamanho) : mem(tamanho, 0) {} //construtor;

    uint32_t ler_palavra(uint32_t endereco) const {
        if (endereco + 3 < mem.size()) {
            return static_cast<uint32_t>(mem[endereco]) | //byte 1;
                  (static_cast<uint32_t>(mem[endereco + 1]) << 8) | //byte 2;
                  (static_cast<uint32_t>(mem[endereco + 2]) << 16) | //byte 3;
                  (static_cast<uint32_t>(mem[endereco + 3]) << 24); //byte 4;
        }
        return 0;
    }

    void escrever_palavra(uint32_t endereco, uint32_t valor) {
        if (endereco + 3 < mem.size()) {
            mem[endereco]     = static_cast<uint8_t>(valor); //byte 1;
            mem[endereco + 1] = static_cast<uint8_t>(valor >> 8); //byte 2;
            mem[endereco + 2] = static_cast<uint8_t>(valor >> 16); //byte 3;
            mem[endereco + 3] = static_cast<uint8_t>(valor >> 24); //byte 4;
        }
    }
};