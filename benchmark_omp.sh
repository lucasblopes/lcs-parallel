#!/bin/bash

# Função para verificar se um arquivo de log já existe e não está vazio
check_log() {
    if [ -s "$1" ]; then
        echo "Log '$1' already exist and is not empty. Skipping step."
        return 1
    fi
    return 0
}

gcc -O3 -march=native -funroll-loops -flto -fopenmp par_lcs.c -o lcs_par

if [ $? -ne 0 ]; then
    echo "Compilation error."
    exit 1
fi

threads_list=(1 2 4 8)
initial_size=10000
num_iterations=5
num_runs=1

# Define a pasta raiz para os logs
LOG_ROOT="logsomp"

current_size_a=$initial_size
current_size_b=$initial_size

echo "Iniciando o experimento com interpolação de tamanhos e strings concatenadas."

for iter in $(seq 1 "$num_iterations"); do
    echo "========== ITERAÇÃO: $iter =========="

    # Calcular o tamanho para a concatenação (n/10)
    size_a=$((current_size_a / 10))
    size_b=$((current_size_b / 10))

    # Gerar strings concatenadas de entrada A e B com os tamanhos atuais
    eval "printf '%.0sheagawghee' {1..$size_a}" >A.in
    eval "printf '%.0spawheae' {1..$size_b}" >B.in

    echo "Tamanho de A: $((current_size_a / 1000))k, Tamanho de B: $((current_size_b / 1000))k"

    for threads in "${threads_list[@]}"; do
        # Criar a pasta de logs para o tamanho das strings e número de threads
        SIZE_A_K=$((current_size_a / 1000))
        SIZE_B_K=$((current_size_b / 1000))
        LOG_DIR="$LOG_ROOT/${SIZE_A_K}k_${SIZE_B_K}k/$threads"
        mkdir -p "$LOG_DIR"

        echo "--- Threads: $threads ---"

        for run in $(seq 1 "$num_runs"); do
            export OMP_NUM_THREADS="$threads"
            echo "Round $run:"

            LOG="$LOG_DIR/$run.log"
            if check_log "$LOG"; then
                ./lcs_par >"$LOG"
            fi

            echo
        done
    done
    echo

    # Intercalar o aumento dos tamanhos
    if ((iter % 2 == 1)); then
        current_size_b=$((current_size_b * 2))
    else
        current_size_a=$((current_size_a * 2))
    fi
done

rm lcs_par

echo "Done!. Logs located in '$LOG_ROOT/'."
