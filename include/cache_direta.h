/*
Cache de mapeamento direto usada pela cache de instruções e pela cache de dados.

Cada cache tem suas próprias 8 linhas de 16 bytes, com capacidade de 128 bytes.
Um bloco da RAM só pode ocupar uma linha em cada cache.

Para localizar um byte a partir de seu endereço, usamos divisão inteira e resto.
    bloco        = endereço / 16
    índice       = bloco % 8
    tag          = bloco / 8
    deslocamento = endereço % 16

        Ex: o endereço 132 pertence ao bloco 8. Ele usa a linha 0, tem tag 1 e está no deslocamento 4. O endereço 4 usa a mesma linha e o mesmo deslocamento, mas tem tag 0. Para reconhecer um hit, a linha precisa ser válida e a tag deve coincidir com a do endereço pedido.

Na leitura, um hit entrega os bytes da linha. Um miss traz o bloco da RAM.

Na escrita, a RAM recebe o valor no mesmo passo, sem buffer (write-through).
Um hit atualiza também os bytes da linha.
Um miss preserva o bloco que já estava ali (write-around).
*/


#ifndef CACHE_DIRETA_H
#define CACHE_DIRETA_H

#include "memoria.h"
#include <array>
#include <cstdint>

struct ContadoresCache {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
};

// Cada instância tem suas próprias oito linhas, inicialmente inválidas.
class CacheDireta {
public:
    // Na cache de instruções, contam as buscas. Na de dados, contam os loads e stores.
    ContadoresCache ler_contadores() const { return contadores; }
    // Repercute uma escrita já feita na RAM. Preserva os contadores desta cache.
    void sincronizar_escrita(std::uint32_t endereco, unsigned bytes, std::uint32_t valor);

protected:
    // Monta o valor com os bytes da linha. A CPU aplica a extensão de sinal em LB e LH.
    std::uint32_t ler_bytes(const Memoria& ram, std::uint32_t endereco, unsigned bytes);
    // Devolve true se o bloco escrito já estava na linha, para permitir a contagem do store.
    bool escrever_bytes(Memoria& ram, std::uint32_t endereco, unsigned bytes, std::uint32_t valor);
    void contar_acesso(bool hit);

private:
    static constexpr unsigned bytes_por_linha = 16;
    static constexpr unsigned numero_linhas = 8;
    struct Linha {
        bool valida = false;  // Uma linha vazia começa inválida, mesmo quando a tag pedida é zero.
        std::uint32_t tag = 0;  // Identifica qual dos blocos que disputam a linha está guardado nela.
        std::array<std::uint8_t, bytes_por_linha> dados{};  // Cópia dos bytes originais da RAM.
    };
    struct Posicao {
        unsigned indice;        // Qual das oito linhas consultar.
        std::uint32_t tag;      // Valor esperado ao comparar com a tag da linha.
        unsigned deslocamento;  // Posição do primeiro byte do acesso dentro da linha (0 a 15).
    };
    static Posicao mapear(std::uint32_t endereco);
    bool atualizar_linha(std::uint32_t endereco, unsigned bytes, std::uint32_t valor);
    std::array<Linha, numero_linhas> linhas{};
    ContadoresCache contadores;
};
#endif
