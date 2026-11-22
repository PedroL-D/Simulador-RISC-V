#!/bin/bash
# O simulador procura a pasta exemplos/ a partir do diretório de execução.
cd -- "$(dirname -- "$0")" || exit 1

if [[ ! -x ./build/simulador_gui ]]; then
    echo 'Compile o simulador conforme a seção “Compilar e rodar no macOS” do README.'
    read -r -p 'Pressione Enter para fechar.'
    exit 1
fi

exec ./build/simulador_gui
