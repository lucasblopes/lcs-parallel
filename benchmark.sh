#!/bin/bash

# CPU1 (AMD)
gcc -O3 -march=znver1 -mtune=znver1 -funroll-loops -flto -fopenmp seq_lcs.c -o lcs_seq
gcc -O3 -march=znver1 -mtune=znver1 -funroll-loops -flto -fopenmp par_lcs.c -o lcs_par

if [ $? -ne 0 ]; then
    echo "Compilation error."
    exit 1
fi

threads_list=(2 4 8 12 16 32)
string_sizes=(10 100 1000 10000 11000 12000 13000)
num_runs=20

# Define a pasta raiz para os logs
LOG_ROOT="logs"

for size in "${string_sizes[@]}"; do
    # Criar strings de entrada A e B
    eval "printf '%.0sheagawghee' {1..$size}" >A.in
    eval "printf '%.0spawheae' {1..$size}" >B.in

    echo "========== STRING SIZE: $size =========="

    for threads in "${threads_list[@]}"; do
        # Criar a pasta de logs para o tamanho da string e número de threads
        LOG_DIR="$LOG_ROOT/$((size * 10))/$threads"
        mkdir -p "$LOG_DIR"

        echo "--- Threads: $threads ---"

        for run in $(seq 1 "$num_runs"); do
            # Executar a versão sequencial e redirecionar a saída
            export OMP_NUM_THREADS=1
            echo "Round $run (Sequential):"
            ./lcs_seq >"$LOG_DIR/sequential_$run.log"

            # Executar a versão paralela e redirecionar a saída
            export OMP_NUM_THREADS="$threads"
            echo "Round $run (Parallel):"
            ./lcs_par >"$LOG_DIR/parallel_$run.log" &

            echo
        done
    done
    echo
done

rm lcs_seq
rm lcs_par

echo "Execuções concluídas. Os logs estão em '$LOG_ROOT/'."
