# Simulador RISC-V (RV32I) - interface grafica acessivel pelo navegador (noVNC)
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# g++ e GLFW/OpenGL para compilar; Xvfb + x11vnc + noVNC para mostrar a janela
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ libglfw3-dev libgl1-mesa-dev libgl1-mesa-dri \
        xvfb x11vnc novnc websockify \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY include include
COPY src src
COPY gui gui
COPY imgui imgui
COPY exemplos exemplos

RUN g++ -std=c++17 -O2 -Iinclude -Iimgui -Iimgui/backends \
        src/*.cpp gui/*.cpp \
        imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp \
        imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp \
        -lglfw -lGL -o simulador_gui

COPY docker/iniciar.sh /iniciar.sh
# Remove finais de linha do Windows (CRLF) caso o Git os tenha colocado no script
RUN sed -i 's/\r$//' /iniciar.sh && chmod +x /iniciar.sh

EXPOSE 6080
CMD ["/iniciar.sh"]