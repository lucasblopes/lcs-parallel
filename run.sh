#!/bin/bash

gcc -fopenmp -O3 seq_lcs.c -o lcs_seq

gcc -fopenmp -O3 par_lcs.c -o lcs_par

if [ $? -ne 0 ]; then
    echo "Compilation error."
    exit 1
fi

# threads_list=(2 4 8 16 32)
threads_list=(2 4 8 12 16 32)
# threads_list=(2)

string_sizes=(10 50 100 1000 10000)
# string_sizes=(1)

for threads in "${threads_list[@]}"; do
    echo "========== Threads: $threads =========="

    for size in "${string_sizes[@]}"; do
        eval "printf '%.0sheagawghee' {1..$size}" >A.in
        eval "printf '%.0spawheae' {1..$size}" >B.in
        echo "--- STRING SIZE: $((size * 10)) ---"
        export OMP_NUM_THREADS=1
        ./lcs_seq
        export OMP_NUM_THREADS=$threads
        ./lcs_par
        echo
    done
done
