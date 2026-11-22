#!/bin/bash
# Sobe um display virtual, o servidor VNC e o noVNC (VNC pelo navegador),
# e depois abre a interface grafica do simulador dentro desse display.
set -e

export DISPLAY=:99
export LIBGL_ALWAYS_SOFTWARE=1   # renderizacao por software (o container nao tem GPU)

# Tela virtual com o mesmo tamanho da janela do simulador (1280x914)
Xvfb :99 -screen 0 1280x914x24 2>/dev/null &

# Espera o display ficar pronto
for _ in $(seq 1 50); do
    [ -e /tmp/.X11-unix/X99 ] && break
    sleep 0.1
done

# VNC sem senha: so e acessivel por quem alcanca a porta (veja o -p no docker run)
x11vnc -display :99 -forever -shared -nopw -rfbport 5900 -quiet &

# Ponte WebSocket -> VNC e pagina web do noVNC
websockify --web /usr/share/novnc 6080 localhost:5900 &

echo
echo "Simulador RISC-V pronto. Abra no navegador:"
echo "  http://localhost:6080/vnc.html?autoconnect=true&resize=scale"
echo

cd /app
exec ./simulador_gui
