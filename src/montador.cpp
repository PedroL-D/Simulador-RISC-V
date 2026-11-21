/*
Montador RV32I: transforma assembly em instrucoes de 32 bits.

Entrada: arquivo texto, uma instrucao por linha, sem comentarios. Exemplos:
    addi x1, x0, 5
    lw x2, 4(x1)
    sw x2, 8(x1)
    beq x1, x2, 8
    jal x1, 16
    lui x5, 0x12345

- Registradores: x0..x31 (tambem aceita nomes ABI: zero, ra, sp, t0, a0...)
- Imediatos: decimal ou hexadecimal (0x...), podendo ser negativos.
- Desvios (beq, bne...) e jal: o imediato eh o deslocamento em BYTES a partir
  da propria instrucao. Tambem eh aceito um rotulo (linha "nome:"), opcional.
- lui/auipc: o imediato eh o valor dos 20 bits superiores (upimm).
*/

#include "montador.h"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {

enum class Tipo { R, I, I_SHIFT, LOAD, S, B, U, J };

struct Info {
    std::uint32_t opcode;
    std::uint32_t funct3;
    std::uint32_t funct7;
    Tipo tipo;
};

// Tabela com as instrucoes da Tabela B.1 (RV32I)
const std::map<std::string, Info>& tabela() {
    static const std::map<std::string, Info> t = {
        // Tipo R (opcode 0x33)
        {"add",  {0x33, 0x0, 0x00, Tipo::R}},
        {"sub",  {0x33, 0x0, 0x20, Tipo::R}},
        {"sll",  {0x33, 0x1, 0x00, Tipo::R}},
        {"slt",  {0x33, 0x2, 0x00, Tipo::R}},
        {"sltu", {0x33, 0x3, 0x00, Tipo::R}},
        {"xor",  {0x33, 0x4, 0x00, Tipo::R}},
        {"srl",  {0x33, 0x5, 0x00, Tipo::R}},
        {"sra",  {0x33, 0x5, 0x20, Tipo::R}},
        {"or",   {0x33, 0x6, 0x00, Tipo::R}},
        {"and",  {0x33, 0x7, 0x00, Tipo::R}},
        // Tipo I aritmetico (opcode 0x13)
        {"addi",  {0x13, 0x0, 0x00, Tipo::I}},
        {"slti",  {0x13, 0x2, 0x00, Tipo::I}},
        {"sltiu", {0x13, 0x3, 0x00, Tipo::I}},
        {"xori",  {0x13, 0x4, 0x00, Tipo::I}},
        {"ori",   {0x13, 0x6, 0x00, Tipo::I}},
        {"andi",  {0x13, 0x7, 0x00, Tipo::I}},
        {"slli",  {0x13, 0x1, 0x00, Tipo::I_SHIFT}},
        {"srli",  {0x13, 0x5, 0x00, Tipo::I_SHIFT}},
        {"srai",  {0x13, 0x5, 0x20, Tipo::I_SHIFT}},
        // Loads (opcode 0x03)
        {"lb",  {0x03, 0x0, 0x00, Tipo::LOAD}},
        {"lh",  {0x03, 0x1, 0x00, Tipo::LOAD}},
        {"lw",  {0x03, 0x2, 0x00, Tipo::LOAD}},
        {"lbu", {0x03, 0x4, 0x00, Tipo::LOAD}},
        {"lhu", {0x03, 0x5, 0x00, Tipo::LOAD}},
        // Stores (opcode 0x23)
        {"sb", {0x23, 0x0, 0x00, Tipo::S}},
        {"sh", {0x23, 0x1, 0x00, Tipo::S}},
        {"sw", {0x23, 0x2, 0x00, Tipo::S}},
        // Desvios (opcode 0x63)
        {"beq",  {0x63, 0x0, 0x00, Tipo::B}},
        {"bne",  {0x63, 0x1, 0x00, Tipo::B}},
        {"blt",  {0x63, 0x4, 0x00, Tipo::B}},
        {"bge",  {0x63, 0x5, 0x00, Tipo::B}},
        {"bltu", {0x63, 0x6, 0x00, Tipo::B}},
        {"bgeu", {0x63, 0x7, 0x00, Tipo::B}},
        // Saltos
        {"jalr", {0x67, 0x0, 0x00, Tipo::I}},
        {"jal",  {0x6F, 0x0, 0x00, Tipo::J}},
        // Tipo U
        {"lui",   {0x37, 0x0, 0x00, Tipo::U}},
        {"auipc", {0x17, 0x0, 0x00, Tipo::U}},
    };
    return t;
}

// Nomes ABI dos registradores
const std::map<std::string, unsigned>& nomes_abi() {
    static const std::map<std::string, unsigned> t = {
        {"zero", 0}, {"ra", 1}, {"sp", 2}, {"gp", 3}, {"tp", 4},
        {"t0", 5}, {"t1", 6}, {"t2", 7}, {"s0", 8}, {"fp", 8}, {"s1", 9},
        {"a0", 10}, {"a1", 11}, {"a2", 12}, {"a3", 13}, {"a4", 14},
        {"a5", 15}, {"a6", 16}, {"a7", 17},
        {"s2", 18}, {"s3", 19}, {"s4", 20}, {"s5", 21}, {"s6", 22},
        {"s7", 23}, {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27},
        {"t3", 28}, {"t4", 29}, {"t5", 30}, {"t6", 31},
    };
    return t;
}

using Rotulos = std::map<std::string, std::uint32_t>;

unsigned parse_registrador(const std::string& s) {
    if (s.size() >= 2 && s[0] == 'x') {
        try {
            size_t pos = 0;
            int n = std::stoi(s.substr(1), &pos);
            if (pos == s.size() - 1 && n >= 0 && n < 32) return n;
        } catch (...) {}
    }
    auto it = nomes_abi().find(s);
    if (it != nomes_abi().end()) return it->second;
    throw std::invalid_argument("registrador invalido: " + s);
}

long parse_numero(const std::string& s) {
    try {
        size_t pos = 0;
        long v = std::stol(s, &pos, 0);   // base 0: aceita decimal e 0x...
        if (pos != s.size()) throw std::invalid_argument("");
        return v;
    } catch (...) {
        throw std::invalid_argument("imediato invalido: " + s);
    }
}

void verifica_faixa(long v, long min, long max, const std::string& nome) {
    if (v < min || v > max)
        throw std::invalid_argument("imediato fora da faixa (" + nome + ")");
}

// Imediato de desvio/jal: numero (deslocamento em bytes) ou rotulo.
long parse_destino(const std::string& s, const Rotulos& rotulos, std::uint32_t pc) {
    if (!s.empty() && (std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) {
        auto it = rotulos.find(s);
        if (it == rotulos.end()) throw std::invalid_argument("rotulo desconhecido: " + s);
        return static_cast<long>(it->second) - static_cast<long>(pc);
    }
    return parse_numero(s);
}

std::uint32_t montar(const std::string& linha, const Rotulos& rotulos, std::uint32_t pc) {
    // Troca virgulas e parenteses por espacos: "lw x1, 4(x2)" -> "lw x1 4 x2"
    std::string limpa = linha;
    for (char& c : limpa)
        if (c == ',' || c == '(' || c == ')') c = ' ';

    std::istringstream ss(limpa);
    std::vector<std::string> t;
    for (std::string p; ss >> p;) t.push_back(p);

    auto it = tabela().find(t[0]);
    if (it == tabela().end()) throw std::invalid_argument("instrucao desconhecida: " + t[0]);
    const Info& f = it->second;

    auto exige = [&](size_t n) {
        if (t.size() != n + 1)
            throw std::invalid_argument("numero de operandos incorreto em: " + t[0]);
    };

    switch (f.tipo) {
        case Tipo::R: {   // op rd, rs1, rs2
            exige(3);
            return (f.funct7 << 25) | (parse_registrador(t[3]) << 20) |
                   (parse_registrador(t[2]) << 15) | (f.funct3 << 12) |
                   (parse_registrador(t[1]) << 7) | f.opcode;
        }
        case Tipo::I: {   // op rd, rs1, imm   (addi, jalr...)
            exige(3);
            long imm = parse_numero(t[3]);
            verifica_faixa(imm, -2048, 2047, "12 bits");
            return ((static_cast<std::uint32_t>(imm) & 0xFFF) << 20) |
                   (parse_registrador(t[2]) << 15) | (f.funct3 << 12) |
                   (parse_registrador(t[1]) << 7) | f.opcode;
        }
        case Tipo::I_SHIFT: {   // op rd, rs1, uimm (0 a 31); funct7 vai nos bits 31:25
            exige(3);
            long sh = parse_numero(t[3]);
            verifica_faixa(sh, 0, 31, "shift 0 a 31");
            return (f.funct7 << 25) | (static_cast<std::uint32_t>(sh) << 20) |
                   (parse_registrador(t[2]) << 15) | (f.funct3 << 12) |
                   (parse_registrador(t[1]) << 7) | f.opcode;
        }
        case Tipo::LOAD: {   // op rd, imm(rs1)
            exige(3);
            long imm = parse_numero(t[2]);
            verifica_faixa(imm, -2048, 2047, "12 bits");
            return ((static_cast<std::uint32_t>(imm) & 0xFFF) << 20) |
                   (parse_registrador(t[3]) << 15) | (f.funct3 << 12) |
                   (parse_registrador(t[1]) << 7) | f.opcode;
        }
        case Tipo::S: {   // op rs2, imm(rs1)
            exige(3);
            long v = parse_numero(t[2]);
            verifica_faixa(v, -2048, 2047, "12 bits");
            std::uint32_t imm = static_cast<std::uint32_t>(v);
            return (((imm >> 5) & 0x7F) << 25) | (parse_registrador(t[1]) << 20) |
                   (parse_registrador(t[3]) << 15) | (f.funct3 << 12) |
                   ((imm & 0x1F) << 7) | f.opcode;
        }
        case Tipo::B: {   // op rs1, rs2, destino
            exige(3);
            long v = parse_destino(t[3], rotulos, pc);
            verifica_faixa(v, -4096, 4094, "desvio 13 bits");
            if (v % 2 != 0) throw std::invalid_argument("desvio deve ser multiplo de 2");
            std::uint32_t imm = static_cast<std::uint32_t>(v);
            return (((imm >> 12) & 0x1) << 31) | (((imm >> 5) & 0x3F) << 25) |
                   (parse_registrador(t[2]) << 20) | (parse_registrador(t[1]) << 15) |
                   (f.funct3 << 12) | (((imm >> 1) & 0xF) << 8) |
                   (((imm >> 11) & 0x1) << 7) | f.opcode;
        }
        case Tipo::U: {   // op rd, upimm (20 bits)
            exige(2);
            long v = parse_numero(t[2]);
            verifica_faixa(v, -524288, 1048575, "20 bits");
            return ((static_cast<std::uint32_t>(v) & 0xFFFFF) << 12) |
                   (parse_registrador(t[1]) << 7) | f.opcode;
        }
        case Tipo::J: {   // op rd, destino
            exige(2);
            long v = parse_destino(t[2], rotulos, pc);
            verifica_faixa(v, -1048576, 1048574, "salto 21 bits");
            if (v % 2 != 0) throw std::invalid_argument("salto deve ser multiplo de 2");
            std::uint32_t imm = static_cast<std::uint32_t>(v);
            return (((imm >> 20) & 0x1) << 31) | (((imm >> 1) & 0x3FF) << 21) |
                   (((imm >> 11) & 0x1) << 20) | (((imm >> 12) & 0xFF) << 12) |
                   (parse_registrador(t[1]) << 7) | f.opcode;
        }
    }
    throw std::invalid_argument("tipo de instrucao invalido");
}

std::string tira_espacos(const std::string& s) {
    const char* esp = " \t\r\n";
    size_t ini = s.find_first_not_of(esp);
    if (ini == std::string::npos) return "";
    return s.substr(ini, s.find_last_not_of(esp) - ini + 1);
}

} // namespace

std::vector<std::uint32_t> montar_arquivo(const std::string& caminho) {
    std::ifstream arq(caminho);
    if (!arq) throw std::runtime_error("nao foi possivel abrir o arquivo: " + caminho);

    // Passo 1: le as linhas e guarda a posicao dos rotulos (linhas "nome:")
    std::vector<std::string> linhas;
    std::vector<int> numeros;   // numero da linha no arquivo (para mensagens de erro)
    Rotulos rotulos;
    std::string bruta;
    int n = 0;
    while (std::getline(arq, bruta)) {
        ++n;
        std::string linha = tira_espacos(bruta);
        if (linha.empty()) continue;
        if (linha.back() == ':') {
            rotulos[linha.substr(0, linha.size() - 1)] = static_cast<std::uint32_t>(linhas.size() * 4);
            continue;
        }
        linhas.push_back(linha);
        numeros.push_back(n);
    }

    // Passo 2: monta cada instrucao
    std::vector<std::uint32_t> programa;
    for (size_t i = 0; i < linhas.size(); ++i) {
        try {
            programa.push_back(montar(linhas[i], rotulos, static_cast<std::uint32_t>(i * 4)));
        } catch (const std::exception& e) {
            throw std::runtime_error("linha " + std::to_string(numeros[i]) + ": " + e.what());
        }
    }
    return programa;
}

std::size_t carregar_programa(const std::string& caminho, Memoria& mem, std::uint32_t endereco_inicial) {
    std::vector<std::uint32_t> programa = montar_arquivo(caminho);
    for (size_t i = 0; i < programa.size(); ++i)
        mem.store_word(endereco_inicial + static_cast<std::uint32_t>(i * 4), programa[i]);
    return programa.size();
}
