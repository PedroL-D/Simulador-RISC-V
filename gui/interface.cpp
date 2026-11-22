// Interface grafica do simulador RV32I (Dear ImGui + GLFW + OpenGL3).
//
// Janelas: Controle, Programa, Registradores, Ultimo passo, Caches e Datapath.
// Os textos da interface ficam SEM ACENTO de proposito: a fonte padrao do
// ImGui so tem os caracteres ASCII e mostraria "?" no lugar de "ç" ou "ã".
//
// Como saber se um passo deu hit ou miss: a CPU so expoe os contadores das
// caches, entao comparamos os contadores ANTES e DEPOIS de cpu.passo().
// Cada busca de instrucao conta 1 acesso, e cada load/store tambem.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "controle.h"
#include "cpu.h"
#include "decoder.h"
#include "immgen.h"
#include "montador.h"

namespace {

constexpr std::size_t TAM_RAM = 4096; // bytes de RAM da CPU
// Com a fonte padrão, a tabela precisa de 596 pontos (cabeçalho + 32 linhas + moldura).
constexpr int ALTURA_REGISTRADORES = 596;
constexpr int ALTURA_DATAPATH = 288;
constexpr int MARGEM = 10;
constexpr int ALTURA_INICIAL = ALTURA_REGISTRADORES + ALTURA_DATAPATH + 3 * MARGEM;

// Nomes ABI dos registradores x0..x31
const char* const NOMES_ABI[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

// Textos dos enums de controle.h / ula.h (a ordem segue a dos enums)
const char* const NOMES_ULA[] = {"ADD", "SUB", "AND", "OR", "XOR",
                                 "SLL", "SRL", "SRA", "SLT", "SLTU"};
const char* const NOMES_ORIGEM_A[] = {"rs1", "zero", "PC"};
const char* const NOMES_ORIGEM_B[] = {"rs2", "imediato"};
const char* const NOMES_RESULTADO[] = {"ULA", "memoria", "PC+4"};
const char* const NOMES_PROXIMO_PC[] = {"sequencial", "desvio condicional", "JAL", "JALR"};

template <class Enum>
int idx(Enum e) { return static_cast<int>(e); }

const ImVec4 VERDE(0.30f, 0.85f, 0.40f, 1.0f);
const ImVec4 VERMELHO(0.95f, 0.35f, 0.35f, 1.0f);

// Tudo o que a tela mostra sobre o ULTIMO passo executado com sucesso.
struct UltimoPasso {
    bool valido = false;
    PassoCPU passo{};
    std::string texto;
    bool busca_hit = false;       // busca de instrucao: hit ou miss
    bool acessou_dados = false;   // houve load/store neste passo?
    bool dados_hit = false;
    int rd_escrito = -1;          // registrador alterado (-1 = nenhum)
};

std::string tira_espacos(const std::string& s) {
    const char* esp = " \t\r\n";
    const std::size_t ini = s.find_first_not_of(esp);
    if (ini == std::string::npos) return "";
    return s.substr(ini, s.find_last_not_of(esp) - ini + 1);
}

// Le o texto do arquivo Assembly com o mesmo filtro da 1a passagem do montador
// (ignora linhas em branco e rotulos), para alinhar cada linha com seu endereco.
std::vector<std::string> ler_fonte(const std::string& caminho) {
    std::vector<std::string> linhas;
    std::ifstream arq(caminho);
    for (std::string bruta; std::getline(arq, bruta);) {
        const std::string linha = tira_espacos(bruta);
        if (linha.empty() || linha.back() == ':') continue;
        linhas.push_back(linha);
    }
    return linhas;
}

const char* sim_nao(bool v) { return v ? "sim" : "nao"; }

void texto_hit_miss(bool hit) {
    ImGui::TextColored(hit ? VERDE : VERMELHO, hit ? "HIT" : "MISS");
}

// Coordenadas locais mantêm os componentes na mesma posição em todas as instruções.
// Desenhamos primeiro os fios, depois os blocos e, por último, os valores do passo.
void desenhar_datapath(const UltimoPasso& ultimo) {
    const auto& p = ultimo.passo;
    const auto& s = p.sinais;
    const bool ativo = ultimo.valido;
    const bool carga = ativo && s.ler_memoria;
    const bool escrita = ativo && s.escrever_memoria;
    const bool memoria = carga || escrita;
    const bool escreve_rd = ultimo.rd_escrito >= 0;
    const bool usa_a_reg = ativo && s.origem_a == OrigemA::RS1;
    const bool usa_b_reg = ativo && s.origem_b == OrigemB::RS2;
    const bool usa_pc = ativo && s.origem_a == OrigemA::PC;
    const bool usa_imm = ativo && s.origem_b == OrigemB::IMEDIATO;
    const bool muda_fluxo = ativo && s.proximo_pc != ProximoPC::SEQUENCIAL;
    const bool link = escreve_rd && s.origem_resultado == OrigemResultado::PC_MAIS_4;
    const bool busca_ram = ativo && !ultimo.busca_hit;
    const bool carga_ram = carga && !ultimo.dados_hit;

    if (ativo)
        ImGui::Text("PC %u   %s", p.pc, ultimo.texto.c_str());
    else
        ImGui::TextDisabled("Use Passo para acompanhar uma instrucao.");
    ImGui::Separator();

    const auto origem = ImGui::GetCursorScreenPos();
    const float escala = std::clamp(ImGui::GetContentRegionAvail().x / 804.f, 0.95f, 1.5f);
    const float fonte = 13.f * escala;
    auto* d = ImGui::GetWindowDrawList();
    const ImU32 cinza = IM_COL32(75, 83, 94, 255);
    const ImU32 acesso = IM_COL32(100, 215, 202, 255);
    const ImU32 hit = IM_COL32(103, 223, 139, 255);
    const ImU32 miss = IM_COL32(255, 154, 119, 255);
    const ImU32 texto = IM_COL32(235, 240, 245, 255);
    const ImU32 painel = ImGui::GetColorU32(ImGuiCol_WindowBg);

    auto ponto = [&](float x, float y) {
        return ImVec2(origem.x + x * escala, origem.y + y * escala);
    };
    auto seta = [&](std::initializer_list<ImVec2> pontos, bool ligada) {
        const ImU32 cor = ligada ? acesso : cinza;
        ImVec2 inicio = *pontos.begin();
        for (auto it = pontos.begin() + 1; it != pontos.end(); ++it) {
            const auto fim = *it;
            d->AddLine(ponto(inicio.x, inicio.y), ponto(fim.x, fim.y), cor, ligada ? 2.f : 1.f);
            if (it + 1 == pontos.end()) {
                const float dx = fim.x - inicio.x, dy = fim.y - inicio.y;
                const float comprimento = std::hypot(dx, dy);
                const float ux = dx / comprimento, uy = dy / comprimento;
                d->AddTriangleFilled(ponto(fim.x, fim.y),
                    ponto(fim.x - 6 * ux - 3 * uy, fim.y - 6 * uy + 3 * ux),
                    ponto(fim.x - 6 * ux + 3 * uy, fim.y - 6 * uy - 3 * ux), cor);
            }
            inicio = fim;
        }
    };
    auto legenda = [&](float x, float y, const std::string& valor, ImU32 cor) {
        const ImVec2 pos = ponto(x, y);
        const float largura = ImGui::CalcTextSize(valor.c_str()).x * fonte / ImGui::GetFontSize();
        d->AddRectFilled(pos, ImVec2(pos.x + largura + 4, pos.y + fonte), painel);
        d->AddText(ImGui::GetFont(), fonte, ImVec2(pos.x + 2, pos.y), cor, valor.c_str());
    };
    auto bloco = [&](float x, float y, float largura, const char* nome,
                     const std::string& detalhe, ImU32 cor) {
        const auto ini = ponto(x, y), fim = ponto(x + largura, y + 38);
        d->AddRectFilled(ini, fim, IM_COL32(35, 43, 54, 255), 4);
        d->AddRect(ini, fim, cor, 4);
        d->AddText(ImGui::GetFont(), fonte, ponto(x + 7, y + (detalhe.empty() ? 12 : 4)), texto, nome);
        if (!detalhe.empty())
            d->AddText(ImGui::GetFont(), fonte, ponto(x + 7, y + 21), cor, detalhe.c_str());
    };

    // Busca, decodificação e entradas da ULA.
    seta({{89, 47}, {127, 47}}, ativo);
    seta({{208, 47}, {244, 47}}, ativo);
    seta({{286, 66}, {286, 110}}, usa_a_reg || usa_b_reg || escrita);
    seta({{370, 47}, {507, 47}, {507, 110}}, usa_imm);
    seta({{76, 66}, {76, 84}, {545, 84}, {545, 110}}, usa_pc);
    seta({{367, 130}, {472, 130}}, usa_a_reg || usa_b_reg);

    // A ULA calcula o endereço; o dado de um store vem diretamente de rs2.
    seta({{563, 128}, {681, 128}}, memoria);
    seta({{342, 148}, {342, 173}, {708, 173}, {708, 148}}, escrita);
    seta({{752, 110}, {752, 66}}, escrita);
    seta({{721, 66}, {721, 110}}, carga_ram);
    seta({{784, 28}, {784, 8}, {165, 8}, {165, 28}}, busca_ram);

    // Retornos ao banco: resultado da ULA, dado do load ou PC+4 de um salto.
    seta({{489, 148}, {489, 195}, {311, 195}, {311, 148}}, escreve_rd && s.origem_resultado == OrigemResultado::ULA);
    seta({{765, 148}, {765, 219}, {256, 219}, {256, 148}}, escreve_rd && carga);
    seta({{165, 178}, {191, 178}, {191, 141}, {235, 141}}, link);

    // O seletor recebe a decisão do branch ou o alvo do salto e realimenta o PC.
    seta({{524, 148}, {524, 202}, {165, 202}}, muda_fluxo);
    seta({{40, 170}, {40, 66}}, ativo);

    std::string decisao;
    if (ativo) {
        switch (s.proximo_pc) {
            case ProximoPC::SEQUENCIAL: decisao = "PC + 4"; break;
            case ProximoPC::DESVIO_CONDICIONAL: decisao = p.tomou_desvio ? "Tomado" : "Nao tomado"; break;
            case ProximoPC::JAL: decisao = "JAL"; break;
            case ProximoPC::JALR: decisao = "JALR"; break;
        }
        decisao += (s.proximo_pc == ProximoPC::SEQUENCIAL ? " = " : " -> ");
        decisao += std::to_string(p.pc_depois);
    }
    bloco(10, 28, 79, "PC", ativo ? std::to_string(p.pc) : "", ativo ? acesso : cinza);
    bloco(127, 28, 81, "Cache I", ativo ? (ultimo.busca_hit ? "HIT" : "MISS") : "",
          ativo ? (ultimo.busca_hit ? hit : miss) : cinza);
    bloco(244, 28, 126, "Decodificacao", "e controle", ativo ? acesso : cinza);
    bloco(235, 110, 132, "Registradores", "", usa_a_reg || usa_b_reg || escreve_rd ? acesso : cinza);
    bloco(472, 110, 91, "ULA", ativo ? NOMES_ULA[idx(s.operacao_ula)] : "", ativo ? acesso : cinza);
    bloco(681, 110, 113, "Cache D", memoria ? (ultimo.dados_hit ? "HIT" : "MISS") : "",
          memoria ? (ultimo.dados_hit ? hit : miss) : cinza);
    bloco(681, 28, 113, "RAM", escrita && busca_ram ? "Le / grava" :
          escrita ? "Gravacao" : (busca_ram || carga_ram ? "Leitura" : ""),
          busca_ram || carga_ram || escrita ? acesso : cinza);
    bloco(10, 170, 155, "Proximo PC", decisao, ativo ? acesso : cinza);

    // Valores capturados na execução, antes de rd poder sobrescrever um dos operandos.
    auto reg = [](unsigned numero, std::uint32_t valor) {
        return std::string(NOMES_ABI[numero]) + "=" + std::to_string(static_cast<std::int32_t>(valor));
    };
    if (usa_a_reg) legenda(369, 110, reg(p.campos.rs1, p.operando_a), acesso);
    if (usa_b_reg) legenda(369, 134, reg(p.campos.rs2, p.operando_b), acesso);
    if (usa_pc) legenda(392, 77, "PC=" + std::to_string(p.operando_a), acesso);
    if (ativo && s.origem_a == OrigemA::ZERO) legenda(521, 91, "A=0", acesso);
    if (usa_imm) legenda(377, 31, "imm=" + std::to_string(static_cast<std::int32_t>(p.imediato)), acesso);
    if (memoria) legenda(565, 111, "end=" + std::to_string(p.resultado_ula), acesso);
    if (escrita) {
        // SB e SH gravam apenas os bits baixos de rs2. SW usa a palavra inteira.
        const unsigned bits = 8 * static_cast<unsigned>(s.largura);
        const auto dado = p.valor_rs2 & (UINT32_MAX >> (32 - bits));
        legenda(397, 162, "dado " + std::string(NOMES_ABI[p.campos.rs2]) + "=" + std::to_string(dado), acesso);
    }
    if (escreve_rd) {
        const auto destino = reg(p.campos.rd, p.valor_rd);
        if (link) legenda(61, 151, "PC+4: " + destino, acesso);
        else legenda(322, carga ? 206 : 183, destino, acesso);
    }
    ImGui::Dummy(ImVec2(804 * escala, 229 * escala));
}

} // namespace

int main() {
    // 1. GLFW + contexto OpenGL da plataforma
    if (!glfwInit()) {
        std::fprintf(stderr, "Erro ao inicializar GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
#ifdef __APPLE__
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Dimensiona a área de conteúdo antes de mostrar a janela (a barra de título fica fora).
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1280, ALTURA_INICIAL, "Simulador RISC-V (RV32I)", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    if (auto* monitor = glfwGetPrimaryMonitor()) {
        int topo_area = 0, altura_area = 0, borda_superior = 0, borda_inferior = 0;
        glfwGetMonitorWorkarea(monitor, nullptr, &topo_area, nullptr, &altura_area);
        glfwGetWindowFrameSize(window, nullptr, &borda_superior, nullptr, &borda_inferior);
        const int disponivel = altura_area - borda_superior - borda_inferior;
        if (disponivel > 0) {
            const int altura = std::min(ALTURA_INICIAL, disponivel);
            glfwSetWindowSize(window, 1280, altura);
            int x = 0, y = 0;
            glfwGetWindowPos(window, &x, &y);
            const int topo = topo_area + borda_superior;
            glfwSetWindowPos(window, x, std::clamp(y, topo, topo + disponivel - altura));
        }
    }
    glfwShowWindow(window);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync

    // 2. Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // nao cria imgui.ini; o layout abaixo vale sempre
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 3. Estado do simulador e da tela
    // A CPU tem a sua propria RAM e caches; nao existe outra Memoria para criar aqui.
    CPU cpu(TAM_RAM);
    std::vector<std::uint32_t> programa;   // palavras de 32 bits do programa carregado
    std::vector<std::string> fonte;        // linhas de Assembly (mesmo tamanho de 'programa')
    UltimoPasso ultimo;

    char caminho[256] = "exemplos/laco_memoria.txt";
    std::string mensagem_erro;
    bool executando = false;
    float passos_por_segundo = 5.0f;
    double acumulado = 0.0;
    double t_anterior = glfwGetTime();

    // O programa termina quando o PC sai da area das instrucoes carregadas.
    auto fim_do_programa = [&]() { return cpu.ler_pc() >= programa.size() * 4; };

    // Nova CPU (PC = 0, registradores e caches zerados) com o programa recarregado.
    auto reiniciar = [&]() {
        cpu = CPU(TAM_RAM);
        for (std::size_t i = 0; i < programa.size(); ++i)
            cpu.carregar_instrucao(static_cast<std::uint32_t>(i * 4), programa[i]);
        ultimo = UltimoPasso{};
        executando = false;
        mensagem_erro.clear();
    };

    auto carregar = [&](const std::string& arquivo) {
        try {
            auto palavras = montar_arquivo(arquivo);
            if (palavras.size() * 4 > TAM_RAM) throw std::runtime_error("programa maior que a RAM");
            programa = std::move(palavras);
            fonte = ler_fonte(arquivo);
            if (fonte.size() != programa.size()) fonte.assign(programa.size(), "");
            reiniciar();
        } catch (const std::exception& e) {
            mensagem_erro = e.what(); // ex.: "linha 2: rotulo desconhecido: destino"
        }
    };

    // Executa UMA instrucao e guarda o que aconteceu em 'ultimo'.
    auto executar_passo = [&]() {
        if (fim_do_programa()) {
            executando = false;
            return;
        }
        UltimoPasso u;
        const auto inst_antes = cpu.contadores_instrucoes();
        const auto dados_antes = cpu.contadores_dados();

        try {
            u.passo = cpu.passo_com_detalhes();
        } catch (const std::exception& e) {
            mensagem_erro = e.what(); // a CPU preserva PC, registradores e RAM na falha
            executando = false;
            return;
        }

        const auto inst_depois = cpu.contadores_instrucoes();
        const auto dados_depois = cpu.contadores_dados();

        u.valido = true;
        const auto indice = u.passo.pc / 4;
        u.texto = u.passo.palavra == programa[indice] ? fonte[indice] : "Instrucao modificada na RAM";
        u.busca_hit = inst_depois.hits > inst_antes.hits;
        u.acessou_dados = (dados_depois.hits + dados_depois.misses) >
                          (dados_antes.hits + dados_antes.misses);
        u.dados_hit = dados_depois.hits > dados_antes.hits;
        if (u.passo.sinais.escrever_registrador && u.passo.campos.rd != 0) {
            u.rd_escrito = u.passo.campos.rd;
        }
        ultimo = u;
    };

    carregar(caminho); // tenta abrir o exemplo padrao ao iniciar

    // 4. Loop principal (1 iteracao = 1 frame)
    float altura_anterior = 0;
    while (!glfwWindowShouldClose(window)) {
        // Pausado: espera eventos por ate 50 ms para nao gastar 100% de CPU no Docker
        if (executando) glfwPollEvents();
        else glfwWaitEventsTimeout(0.05);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Modo "Executar": roda 'passos_por_segundo' instrucoes por segundo
        const double agora = glfwGetTime();
        const double dt = agora - t_anterior;
        t_anterior = agora;
        if (executando) {
            acumulado += dt * passos_por_segundo;
            int feitos = 0;
            while (acumulado >= 1.0 && executando && feitos < 100) {
                executar_passo();
                acumulado -= 1.0;
                ++feitos;
            }
            if (feitos == 100) acumulado = 0.0;
        } else {
            acumulado = 0.0;
        }

        // Distribui a altura disponível entre as duas fileiras. Reaplica o layout
        // ao redimensionar a janela principal e permite ajustes manuais enquanto o tamanho permanece estável.
        const float altura = ImGui::GetIO().DisplaySize.y;
        const auto aplicar_layout = altura != altura_anterior ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
        altura_anterior = altura;
        const float altura_datapath = std::min<float>(ALTURA_DATAPATH, std::floor(altura * 0.42f));
        const float altura_banco = std::max(100.f, altura - altura_datapath - 3 * MARGEM);
        const float altura_caches = 109.f;
        const float altura_ultimo = std::max(60.f, altura_banco - altura_caches - MARGEM);

        // --- JANELA 1: CONTROLE ---
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 215), ImGuiCond_FirstUseEver);
        ImGui::Begin("Controle da CPU");
        ImGui::Text("PC atual: 0x%08X (%u)", cpu.ler_pc(), cpu.ler_pc());

        ImGui::SetNextItemWidth(-80.0f);
        ImGui::InputText("##caminho", caminho, sizeof(caminho));
        ImGui::SameLine();
        if (ImGui::Button("Carregar")) carregar(caminho);
        ImGui::Separator();

        if (ImGui::Button("Passo")) {
            mensagem_erro.clear();
            executar_passo();
        }
        ImGui::SameLine();
        if (ImGui::Button(executando ? "Pausar" : "Executar")) {
            mensagem_erro.clear();
            executando = !executando;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) reiniciar(); // mantem o programa, zera CPU e caches

        ImGui::SliderFloat("Passos/s", &passos_por_segundo, 1.0f, 120.0f, "%.0f");

        if (!programa.empty() && fim_do_programa()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.3f, 1.0f));
            ImGui::TextWrapped("Programa terminado (PC fora do codigo). Use Reset para repetir.");
            ImGui::PopStyleColor();
        }
        if (!mensagem_erro.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Erro: %s", mensagem_erro.c_str());
        }
        ImGui::End();

        // --- JANELA 2: PROGRAMA (a linha amarela e a proxima instrucao) ---
        ImGui::SetNextWindowPos(ImVec2(10, 235), aplicar_layout);
        ImGui::SetNextWindowSize(ImVec2(420, std::max(65.f, altura - 235 - MARGEM)), aplicar_layout);
        ImGui::Begin("Programa");
        if (programa.empty()) {
            ImGui::TextDisabled("Nenhum programa carregado.");
        } else if (ImGui::BeginTable("prog", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed, 28.0f);
            ImGui::TableSetupColumn("Endereco", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Palavra", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Assembly");
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < programa.size(); ++i) {
                const std::uint32_t end = static_cast<std::uint32_t>(i * 4);
                const bool proxima = (end == cpu.ler_pc());
                const bool anterior = ultimo.valido && end == ultimo.passo.pc;

                ImGui::TableNextRow();
                if (proxima) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(110, 90, 20, 255));
                else if (anterior) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(40, 75, 45, 255));

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(proxima ? ">" : "");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", end);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("0x%08X", programa[i]);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(fonte[i].c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();

        // --- JANELA 3: BANCO DE REGISTRADORES (o ultimo escrito fica verde) ---
        ImGui::SetNextWindowPos(ImVec2(440, MARGEM), aplicar_layout);
        ImGui::SetNextWindowSize(ImVec2(330, altura_banco), aplicar_layout);
        ImGui::Begin("Banco de Registradores");
        if (ImGui::BeginTable("regs_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Reg");
            ImGui::TableSetupColumn("ABI");
            ImGui::TableSetupColumn("Hex");
            ImGui::TableSetupColumn("Decimal");
            ImGui::TableHeadersRow();

            for (int i = 0; i < 32; ++i) {
                const std::uint32_t valor = cpu.ler_registrador(i);
                const bool alterado = (i == ultimo.rd_escrito);

                ImGui::TableNextRow();
                if (alterado) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(40, 75, 45, 255));
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("x%d", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(NOMES_ABI[i]);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("0x%08X", valor);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", static_cast<std::int32_t>(valor));
            }
            ImGui::EndTable();
        }
        ImGui::End();

        // --- JANELA 4: ULTIMO PASSO (campos, sinais de controle e resultado nas caches) ---
        ImGui::SetNextWindowPos(ImVec2(780, MARGEM), aplicar_layout);
        ImGui::SetNextWindowSize(ImVec2(490, altura_ultimo), aplicar_layout);
        ImGui::Begin("Ultimo passo");
        if (!ultimo.valido) {
            ImGui::TextDisabled("Nenhum passo executado ainda.");
        } else {
            const auto& c = ultimo.passo.campos;
            const auto& s = ultimo.passo.sinais;
            ImGui::Text("Instrucao: %s", ultimo.texto.c_str());
            ImGui::Text("Endereco: %u   Palavra: 0x%08X", ultimo.passo.pc, ultimo.passo.palavra);
            // Campos brutos do decoder: cada formato usa so alguns (ex.: um branch nao tem rd).
            ImGui::Text("opcode=0x%02X rd=x%u rs1=x%u rs2=x%u funct3=%u funct7=0x%02X",
                        static_cast<unsigned>(c.opcode), static_cast<unsigned>(c.rd),
                        static_cast<unsigned>(c.rs1), static_cast<unsigned>(c.rs2),
                        static_cast<unsigned>(c.funct3), static_cast<unsigned>(c.funct7));
            ImGui::Text("Imediato: %d (0x%08X)", static_cast<std::int32_t>(ultimo.passo.imediato), ultimo.passo.imediato);

            ImGui::Separator();
            ImGui::Text("Sinais de controle");
            ImGui::Text("  Operacao da ULA: %s", NOMES_ULA[idx(s.operacao_ula)]);
            ImGui::Text("  Operando A: %s   Operando B: %s", NOMES_ORIGEM_A[idx(s.origem_a)],
                        NOMES_ORIGEM_B[idx(s.origem_b)]);
            ImGui::Text("  Escreve registrador: %s   Le memoria: %s   Escreve memoria: %s",
                        sim_nao(s.escrever_registrador), sim_nao(s.ler_memoria), sim_nao(s.escrever_memoria));
            ImGui::Text("  Resultado vem de: %s   Proximo PC: %s", NOMES_RESULTADO[idx(s.origem_resultado)],
                        NOMES_PROXIMO_PC[idx(s.proximo_pc)]);

            ImGui::Separator();
            ImGui::Text("Busca da instrucao (cache I): ");
            ImGui::SameLine();
            texto_hit_miss(ultimo.busca_hit);
            ImGui::Text("Acesso a dados (cache D):     ");
            ImGui::SameLine();
            if (ultimo.acessou_dados) texto_hit_miss(ultimo.dados_hit);
            else ImGui::TextDisabled("nao houve");

            ImGui::Separator();
            ImGui::Text("PC: %u -> %u", ultimo.passo.pc, ultimo.passo.pc_depois);
            if (ultimo.rd_escrito >= 0)
                ImGui::Text("x%d (%s) = 0x%08X (%d)", ultimo.rd_escrito, NOMES_ABI[ultimo.rd_escrito],
                            ultimo.passo.valor_rd, static_cast<std::int32_t>(ultimo.passo.valor_rd));
            else
                ImGui::TextDisabled("Nenhum registrador foi escrito.");
        }
        ImGui::End();

        // --- JANELA 5: CACHES (contadores acumulados desde o ultimo Reset) ---
        ImGui::SetNextWindowPos(ImVec2(780, altura_ultimo + 2 * MARGEM), aplicar_layout);
        ImGui::SetNextWindowSize(ImVec2(490, altura_caches), aplicar_layout);
        ImGui::Begin("Caches");
        if (ImGui::BeginTable("caches_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Cache");
            ImGui::TableSetupColumn("Hits");
            ImGui::TableSetupColumn("Misses");
            ImGui::TableSetupColumn("Taxa");
            ImGui::TableSetupColumn("Ultimo passo");
            ImGui::TableHeadersRow();

            // resultado: -1 = nao houve acesso, 0 = miss, 1 = hit
            auto linha = [&](const char* nome, const ContadoresCache& c, int resultado) {
                const unsigned long long total = c.hits + c.misses;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(nome);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%llu", static_cast<unsigned long long>(c.hits));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%llu", static_cast<unsigned long long>(c.misses));
                ImGui::TableSetColumnIndex(3);
                if (total > 0) ImGui::Text("%.1f%%", 100.0 * static_cast<double>(c.hits) / static_cast<double>(total));
                else ImGui::TextDisabled("-");
                ImGui::TableSetColumnIndex(4);
                if (resultado < 0) ImGui::TextDisabled("-");
                else texto_hit_miss(resultado == 1);
            };
            linha("Instrucoes", cpu.contadores_instrucoes(),
                  ultimo.valido ? (ultimo.busca_hit ? 1 : 0) : -1);
            linha("Dados", cpu.contadores_dados(),
                  ultimo.acessou_dados ? (ultimo.dados_hit ? 1 : 0) : -1);
            ImGui::EndTable();
        }
        ImGui::End();

        // A largura das duas colunas permite ler os valores nas setas do último passo.
        ImGui::SetNextWindowPos(ImVec2(440, altura_banco + 2 * MARGEM), aplicar_layout);
        ImGui::SetNextWindowSize(ImVec2(830, altura_datapath), aplicar_layout);
        if (ImGui::Begin("Microarquitetura (Datapath)", nullptr, ImGuiWindowFlags_HorizontalScrollbar))
            desenhar_datapath(ultimo);
        ImGui::End();

        // 5. Renderiza o frame
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // 6. Libera recursos
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
