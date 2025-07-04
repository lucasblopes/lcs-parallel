#!/bin/bash
MPI_SOURCE="mpi_lcs.c"
SEQ_SOURCE="seq_lcs.c"
BIN_DIR="bin"
JOBS_DIR="jobs"
INPUTS_DIR="inputs"
LOG_ROOT="logsmpi"
SEQ_BIN="./$BIN_DIR/lcs_seq.bin"
MPI_BIN="./$BIN_DIR/lcs_mpi.bin"
USER="lgbl"

# Configurações
initial_size=10000
num_runs=20
tasks_list=(2 4 6)
input_sizes=(10000 20000 30000 40000)

echo "Iniciando benchmark com escalonamento de tarefas: ${tasks_list[*]}"

mkdir -p "$BIN_DIR"
mkdir -p "$JOBS_DIR"
mkdir -p "$INPUTS_DIR"

mpicc -O3 -march=native -funroll-loops -flto $MPI_SOURCE -o $MPI_BIN -lm
if [ $? -ne 0 ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi
gcc -O3 -march=native -funroll-loops -flto -fopenmp $SEQ_SOURCE -o $SEQ_BIN
if [ $? -ne 0 ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi
#gcc -O3 -march=native -funroll-loops -flto -fopenmp seq_lcs_prof.c -o seqprof

if [ $? -ne 0 ]; then
    echo "Erro de compilação."
    exit 1
fi

for size in "${input_sizes[@]}"; do

    size_a_k=$((size / 1000))
    size_b_k=$((size / 1000))

    echo "Tamanhos: A=${size_a_k}k, B=${size_b_k}k"
    A_FILE="$INPUTS_DIR/A_${size_a_k}k.in"
    B_FILE="$INPUTS_DIR/B_${size_b_k}k.in"

    if [ ! -f "$A_FILE" ]; then
        </dev/urandom tr -dc 'ACGT' | head -c "$size" >"$A_FILE"
        </dev/urandom tr -dc 'ACGT' | head -c 100 >A100
    fi

    if [ ! -f "$B_FILE" ]; then
        </dev/urandom tr -dc 'ACGT' | head -c "$size" >"$B_FILE"
    fi

    for tasks in "${tasks_list[@]}"; do
        # Estimar uso total de memória com base nas alocações reais do código sequencial
        matrix_ptrs_bytes=$(((current_size_b + 1) * 8))
        matrix_data_bytes=$(((current_size_b + 1) * (current_size_a + 1) * 2))
        seq_a_bytes=$((current_size_a + 1))
        seq_b_bytes=$((current_size_b + 1))

        total_bytes=$((matrix_ptrs_bytes + matrix_data_bytes + seq_a_bytes + seq_b_bytes))

        # Adicionar margem de segurança de 50%
        total_bytes=$(((total_bytes * 3) / 2))

        # Converter para MB, depois para GB (mínimo de 1GB)
        mem_mb=$(((total_bytes + 1024 * 1024 - 1) / (1024 * 1024)))
        mem_gb=$(((mem_mb + 1023) / 1024))
        mem_gb=$((mem_gb < 1 ? 1 : mem_gb))
        mem="${mem_gb}GB"

        # if [ "$tasks" -le 3 ]; then
        if [ "$tasks" -eq 1 ]; then
            nodes=1
        else
            nodes=2
        fi

        time="00:30:00"
        LOG_DIR="$LOG_ROOT/${size_a_k}k_${size_b_k}k/${tasks}"
        mkdir -p "$LOG_DIR"

        echo "  Tasks: $tasks, Nodes: $nodes, Memória: $mem"

        for run in $(seq 1 "$num_runs"); do
            LOG="$LOG_DIR/$run.log"
            if [ -s "$LOG" ]; then
                continue
            fi

            JOB_FILE="$JOBS_DIR/${size_a_k}k_${size_b_k}k_${tasks}t.sbatch"

            if [ "$tasks" -eq 1 ]; then
                tasks_per_node=$tasks
            else
                per_node=$((($tasks + 1) / 2))
                tasks_per_node=$per_node
            fi

            if [ "$tasks" -eq 1 ]; then
                RUN_CODE="./$SEQ_BIN $A_FILE $B_FILE"
            else
                RUN_CODE="mpirun -np $tasks --bind-to core --map-by ppr:$tasks_per_node:node $MPI_BIN \"$A_FILE\" \"$B_FILE\""
            fi

            cat >"$JOB_FILE" <<EOF
#!/bin/bash
#SBATCH --job-name="${size_a_k}${size_b_k}${tasks}"
#SBATCH --nodes=$nodes
#SBATCH --ntasks=$tasks
#SBATCH --ntasks-per-node=$tasks_per_node
#SBATCH --mem=$mem
#SBATCH --time=$time
#SBATCH --output=$LOG
#SBATCH --cpu-freq=high:UserSpace

$RUN_CODE
EOF

            echo -n "    Submetendo $JOB_FILE..."

            while squeue -u "$USER" --noheader | grep -q .; do
                sleep 0.5
            done

            sbatch "$JOB_FILE"
        done
    done
done

echo "Todos os subjobs foram gerados e submetidos. Verifique os logs em '$LOG_ROOT' e os scripts em '$JOBS_DIR'."
