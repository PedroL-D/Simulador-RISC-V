#include "cache_direta.h"
#include <stdexcept>

namespace {
void validar_formato(std::uint32_t endereco, unsigned bytes) {
    // O acesso pode ser de um byte, uma meia-palavra (2 bytes) ou uma palavra (4 bytes).
    if (bytes != 1 && bytes != 2 && bytes != 4)
        throw std::invalid_argument("Largura de acesso invalida");
    // Uma meia-palavra começa em endereço par. Uma palavra começa em múltiplo de quatro.
    if (endereco % bytes != 0) throw std::invalid_argument("Endereco desalinhado");
}
}

void CacheDireta::contar_acesso(bool hit) {
    if (hit) ++contadores.hits;
    else ++contadores.misses;
}

CacheDireta::Posicao CacheDireta::mapear(std::uint32_t endereco) {
    // Dividir por 16 dá o número do bloco. Por exemplo, os endereços de 128 a 143 dão bloco 8.
    const auto bloco = endereco / bytes_por_linha;
    // Para o endereço 132, os três campos de Posicao são índice 0, tag 1 e deslocamento 4.
    return {bloco % numero_linhas, bloco / numero_linhas, endereco % bytes_por_linha};
}

std::uint32_t CacheDireta::ler_bytes(const Memoria& ram, std::uint32_t endereco, unsigned bytes) {
    // Verifica o acesso inteiro antes de alterar a linha ou contabilizar a leitura.
    validar_formato(endereco, bytes);
    if (!ram.contem(endereco, bytes)) throw std::out_of_range("Acesso fora da RAM");

    const auto pos = mapear(endereco);
    auto& linha = linhas[pos.indice];
    // O índice aponta para uma única linha. A validade e a tag confirmam que ela contém o bloco.
    const bool hit = linha.valida && linha.tag == pos.tag;
    contar_acesso(hit);
    if (!hit) {
        // Miss de leitura. Para o endereço 132, voltamos 4 bytes e começamos a cópia em 128.
        const auto inicio = endereco - pos.deslocamento;
        // Copia até 16 bytes originais. O último bloco pode terminar antes do fim da linha.
        for (unsigned i = 0; i < bytes_por_linha && ram.contem(inicio + i, 1); ++i)
            linha.dados[i] = static_cast<std::uint8_t>(ram.load_byte_unsigned(inicio + i));
        // Depois da cópia, a linha passa a representar o bloco solicitado.
        linha.tag = pos.tag;
        linha.valida = true;
    }

    // Em um hit já podemos ler daqui. Em um miss a linha acabou de ser preenchida.
    // Em little-endian, os bytes 0x34 e 0x12, nessa ordem, formam a meia-palavra 0x1234.
    std::uint32_t palavra = 0;
    for (unsigned i = 0; i < bytes; ++i)
        palavra |= static_cast<std::uint32_t>(linha.dados[pos.deslocamento + i]) << (8 * i);
    return palavra;
}

bool CacheDireta::escrever_bytes(Memoria& ram, std::uint32_t endereco, unsigned bytes, std::uint32_t valor) {
    validar_formato(endereco, bytes);
    if (!ram.contem(endereco, bytes)) throw std::out_of_range("Acesso fora da RAM");

    // Write-through sem buffer. SB grava os 8 bits baixos, SH os 16 baixos e SW os 32.
    switch (bytes) {
        case 1: ram.store_byte(endereco, static_cast<std::uint8_t>(valor)); break;
        case 2: ram.store_half(endereco, static_cast<std::uint16_t>(valor)); break;
        case 4: ram.store_word(endereco, valor); break;
    }

    // A RAM já tem o valor novo. O retorno informa se havia uma cópia na cache.
    return atualizar_linha(endereco, bytes, valor);
}

void CacheDireta::sincronizar_escrita(std::uint32_t endereco, unsigned bytes, std::uint32_t valor) {
    // Por exemplo, se um store alterou código, a cache de instruções atualiza sua cópia.
    // A sincronização preserva os contadores, pois a CPU ainda não fez uma nova busca.
    validar_formato(endereco, bytes);
    atualizar_linha(endereco, bytes, valor);
}

bool CacheDireta::atualizar_linha(std::uint32_t endereco, unsigned bytes, std::uint32_t valor) {
    const auto pos = mapear(endereco);
    auto& linha = linhas[pos.indice];
    // Write-around: uma escrita com miss preserva o bloco que ocupa a linha.
    if (!linha.valida || linha.tag != pos.tag) return false;

    // Hit de escrita. A cada iteração, os próximos 8 bits de valor vão para o byte
    // correspondente da linha. Assim, SB altera só um byte e preserva seus vizinhos.
    for (unsigned i = 0; i < bytes; ++i)
        linha.dados[pos.deslocamento + i] = static_cast<std::uint8_t>(valor >> (8 * i));
    return true;
}
