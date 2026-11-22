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
    std::uint32_t pc = 0;         // endereco da instrucao executada
    std::uint32_t pc_depois = 0;  // PC apos o passo
    std::uint32_t palavra = 0;    // instrucao em codigo de maquina
    std::uint32_t imediato = 0;
    std::string texto;            // instrucao em Assembly
    Decoded campos{};
    SinaisControle sinais{};
    bool busca_hit = false;       // busca de instrucao: hit ou miss
    bool acessou_dados = false;   // houve load/store neste passo?
    bool dados_hit = false;
    int rd_escrito = -1;          // registrador alterado (-1 = nenhum)
    std::uint32_t valor_rd = 0;
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

} // namespace

int main() {
    // 1. GLFW + contexto OpenGL 3.0 (compativel com Mesa/Docker)
    if (!glfwInit()) {
        std::fprintf(stderr, "Erro ao inicializar GLFW\n");
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Simulador RISC-V (RV32I)", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
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
        const auto pc_antes = cpu.ler_pc();
        const auto inst_antes = cpu.contadores_instrucoes();
        const auto dados_antes = cpu.contadores_dados();

        try {
            cpu.passo();
        } catch (const std::exception& e) {
            mensagem_erro = e.what(); // a CPU preserva PC, registradores e RAM na falha
            executando = false;
            return;
        }

        const auto inst_depois = cpu.contadores_instrucoes();
        const auto dados_depois = cpu.contadores_dados();

        UltimoPasso u;
        u.valido = true;
        u.pc = pc_antes;
        u.pc_depois = cpu.ler_pc();
        u.palavra = programa[pc_antes / 4];
        u.texto = fonte[pc_antes / 4];
        u.campos = decode(u.palavra);
        u.sinais = gerar_controle(u.campos);
        u.imediato = static_cast<std::uint32_t>(immgen(u.palavra));
        u.busca_hit = inst_depois.hits > inst_antes.hits;
        u.acessou_dados = (dados_depois.hits + dados_depois.misses) >
                          (dados_antes.hits + dados_antes.misses);
        u.dados_hit = dados_depois.hits > dados_antes.hits;
        if (u.sinais.escrever_registrador && u.campos.rd != 0) {
            u.rd_escrito = u.campos.rd;
            u.valor_rd = cpu.ler_registrador(u.campos.rd);
        }
        ultimo = u;
    };

    carregar(caminho); // tenta abrir o exemplo padrao ao iniciar

    // 4. Loop principal (1 iteracao = 1 frame)
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
        ImGui::SetNextWindowPos(ImVec2(10, 235), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 475), ImGuiCond_FirstUseEver);
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
                const bool anterior = ultimo.valido && end == ultimo.pc;

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
        ImGui::SetNextWindowPos(ImVec2(440, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(330, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Banco de Registradores");
        if (ImGui::BeginTable("regs_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
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
        ImGui::SetNextWindowPos(ImVec2(780, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(490, 330), ImGuiCond_FirstUseEver);
        ImGui::Begin("Ultimo passo");
        if (!ultimo.valido) {
            ImGui::TextDisabled("Nenhum passo executado ainda.");
        } else {
            const auto& c = ultimo.campos;
            const auto& s = ultimo.sinais;
            ImGui::Text("Instrucao: %s", ultimo.texto.c_str());
            ImGui::Text("Endereco: %u   Palavra: 0x%08X", ultimo.pc, ultimo.palavra);
            // Campos brutos do decoder: cada formato usa so alguns (ex.: um branch nao tem rd).
            ImGui::Text("opcode=0x%02X rd=x%u rs1=x%u rs2=x%u funct3=%u funct7=0x%02X",
                        static_cast<unsigned>(c.opcode), static_cast<unsigned>(c.rd),
                        static_cast<unsigned>(c.rs1), static_cast<unsigned>(c.rs2),
                        static_cast<unsigned>(c.funct3), static_cast<unsigned>(c.funct7));
            ImGui::Text("Imediato: %d (0x%08X)", static_cast<std::int32_t>(ultimo.imediato), ultimo.imediato);

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
            ImGui::Text("PC: %u -> %u", ultimo.pc, ultimo.pc_depois);
            if (ultimo.rd_escrito >= 0)
                ImGui::Text("x%d (%s) = 0x%08X (%d)", ultimo.rd_escrito, NOMES_ABI[ultimo.rd_escrito],
                            ultimo.valor_rd, static_cast<std::int32_t>(ultimo.valor_rd));
            else
                ImGui::TextDisabled("Nenhum registrador foi escrito.");
        }
        ImGui::End();

        // --- JANELA 5: CACHES (contadores acumulados desde o ultimo Reset) ---
        ImGui::SetNextWindowPos(ImVec2(780, 350), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(490, 120), ImGuiCond_FirstUseEver);
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

        // --- JANELA 6: ESBOCO DA MICROARQUITETURA (ImDrawList) ---
        // Para acrescentar um bloco novo: chame bloco(x, "Nome", cor, "legenda")
        // e ligue-o ao vizinho com fio(x_inicio, x_fim, ativo).
        ImGui::SetNextWindowPos(ImVec2(780, 480), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(490, 230), ImGuiCond_FirstUseEver);
        ImGui::Begin("Microarquitetura (Datapath)");
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 o = ImGui::GetCursorScreenPos(); // canto superior esquerdo da janela

            const ImU32 cor_base = IM_COL32(50, 70, 100, 255);
            const ImU32 cor_hit = IM_COL32(40, 130, 70, 255);
            const ImU32 cor_miss = IM_COL32(150, 50, 50, 255);
            const ImU32 fio_ligado = IM_COL32(50, 220, 90, 255);
            const ImU32 fio_desligado = IM_COL32(120, 120, 120, 255);

            // Cache fica verde (hit) ou vermelha (miss) conforme o ultimo passo
            const ImU32 cor_cache_i = !ultimo.valido ? cor_base : (ultimo.busca_hit ? cor_hit : cor_miss);
            const ImU32 cor_cache_d = !ultimo.acessou_dados ? cor_base : (ultimo.dados_hit ? cor_hit : cor_miss);
            const char* legenda_i = !ultimo.valido ? "" : (ultimo.busca_hit ? "HIT" : "MISS");
            const char* legenda_d = !ultimo.acessou_dados ? "" : (ultimo.dados_hit ? "HIT" : "MISS");

            auto bloco = [&](float x, const char* nome, ImU32 fundo, const char* legenda) {
                const ImVec2 p0(o.x + x, o.y + 30);
                const ImVec2 p1(o.x + x + 75, o.y + 110);
                draw->AddRectFilled(p0, p1, fundo, 4.0f);
                draw->AddRect(p0, p1, IM_COL32(200, 200, 200, 255), 4.0f);
                draw->AddText(ImVec2(p0.x + 6, p0.y + 22), IM_COL32(255, 255, 255, 255), nome);
                draw->AddText(ImVec2(p0.x + 6, p0.y + 44), IM_COL32(255, 255, 255, 255), legenda);
            };
            auto fio = [&](float x0, float x1, bool ativo) {
                draw->AddLine(ImVec2(o.x + x0, o.y + 70), ImVec2(o.x + x1, o.y + 70),
                              ativo ? fio_ligado : fio_desligado, 3.0f);
            };

            bloco(15, "PC", cor_base, "");
            bloco(125, "Cache I", cor_cache_i, legenda_i);
            bloco(235, "ULA", cor_base, "");
            bloco(345, "Cache D", cor_cache_d, legenda_d);

            fio(90, 125, ultimo.valido);          // PC -> cache de instrucoes
            fio(200, 235, ultimo.valido);         // cache de instrucoes -> ULA (simplificado)
            fio(310, 345, ultimo.acessou_dados);  // ULA -> cache de dados (so em load/store)

            ImGui::Dummy(ImVec2(430, 125)); // reserva o espaco desenhado
        }
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
