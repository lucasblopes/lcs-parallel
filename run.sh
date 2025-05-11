#!/bin/bash

# CPU1 (AMD)
# gcc -O3 -march=znver1 -mavx2 -funroll-loops -ffast-math -flto -fopenmp seq_lcs.c -o lcs_seq
# gcc -O3 -march=znver1 -mavx2 -funroll-loops -ffast-math -flto -fopenmp par_lcs.c -o lcs_par
gcc -O3 -march=znver1 -mtune=znver1 -funroll-loops -flto -fopenmp seq_lcs.c -o lcs_seq
gcc -O3 -march=znver1 -mtune=znver1 -funroll-loops -flto -fopenmp par_lcs.c -o lcs_par
# gcc -O3 -march=native -mtune=native -flto -ffast-math -fopenmp seq_lcs.c -o lcs_seq
# gcc -O3 -march=native -mtune=native -flto -ffast-math -fopenmp seq_par.c -o lcs_par

if [ $? -ne 0 ]; then
    echo "Compilation error."
    exit 1
fi

threads_list=(2 4 8 12 16 32)
# threads_list=(4)

string_sizes=(10 100 1000 10000 11000 12000 13000)
# string_sizes=(100)

for size in "${string_sizes[@]}"; do
    eval "printf '%.0sheagawghee' {1..$size}" >A.in
    eval "printf '%.0spawheae' {1..$size}" >B.in
    # eval "printf '%.0sgxtxayb' {1..$size}" >B.in
    # eval "printf '%.0saggtab' {1..$size}" >A.in
    echo "========== STRING SIZE: $((size * 10)) =========="

    for threads in "${threads_list[@]}"; do
        echo "--- Threads: $threads ---"
        export OMP_NUM_THREADS=1
        ./lcs_seq
        export OMP_NUM_THREADS=$threads
        ./lcs_par
        echo
    done
done

rm lcs_seq
rm lcs_par
