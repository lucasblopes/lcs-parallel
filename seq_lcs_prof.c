#include <math.h>  // Necessário para sqrt() e pow()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define NUM_RUNS 20  // Número de vezes que a computação será executada

typedef unsigned short mtype;

/* Estrutura para armazenar os dados brutos de todas as execuções */
typedef struct {
    double file_io_time;  // I/O é feito apenas uma vez
    double memory_alloc_times[NUM_RUNS];
    double matrix_init_times[NUM_RUNS];
    double lcs_computation_times[NUM_RUNS];
    double total_times[NUM_RUNS];
} profiling_raw_data;

/* Estrutura para armazenar as estatísticas calculadas (média e desvio padrão) */
typedef struct {
    double file_io_time;
    double alloc_mean, alloc_stddev;
    double init_mean, init_stddev;
    double compute_mean, compute_stddev;
    double total_mean, total_stddev;
} profiling_stats;

/* Função de timer de alta precisão */
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* As funções de LCS (read_seq, allocate, init, LCS, etc.) permanecem as mesmas,
   mas agora retornam o tempo em vez de somá-lo a um ponteiro. */

char *read_seq(char *fname) {
    // ... (código da função sem alterações)
    FILE *fseq = NULL;
    long size = 0;
    char *seq = NULL;
    int i = 0;
    fseq = fopen(fname, "rt");
    if (fseq == NULL) {
        printf("Error reading file %s\n", fname);
        exit(1);
    }
    fseek(fseq, 0L, SEEK_END);
    size = ftell(fseq);
    rewind(fseq);
    seq = (char *)calloc(size + 1, sizeof(char));
    if (seq == NULL) {
        printf("Erro allocating memory for sequence %s.\n", fname);
        exit(1);
    }
    while (!feof(fseq)) {
        seq[i] = fgetc(fseq);
        if ((seq[i] != '\n') && (seq[i] != EOF)) i++;
    }
    seq[i] = '\0';
    fclose(fseq);
    return seq;
}

mtype **allocateScoreMatrix(int sizeA, int sizeB) {
    int i;
    mtype **scoreMatrix = (mtype **)malloc((sizeB + 1) * sizeof(mtype *));
    for (i = 0; i < (sizeB + 1); i++) {
        scoreMatrix[i] = (mtype *)malloc((sizeA + 1) * sizeof(mtype));
    }
    return scoreMatrix;
}

void initScoreMatrix(mtype **scoreMatrix, int sizeA, int sizeB) {
    int i, j;
    for (j = 0; j < (sizeA + 1); j++) {
        scoreMatrix[0][j] = 0;
    }
    for (i = 1; i < (sizeB + 1); i++) {
        scoreMatrix[i][0] = 0;
    }
}

int LCS(mtype **scoreMatrix, int sizeA, int sizeB, char *seqA, char *seqB) {
    int i, j;
    for (i = 1; i < sizeB + 1; i++) {
        for (j = 1; j < sizeA + 1; j++) {
            if (seqA[j - 1] == seqB[i - 1]) {
                scoreMatrix[i][j] = scoreMatrix[i - 1][j - 1] + 1;
            } else {
                scoreMatrix[i][j] = max(scoreMatrix[i - 1][j], scoreMatrix[i][j - 1]);
            }
        }
    }
    return scoreMatrix[sizeB][sizeA];
}

void freeScoreMatrix(mtype **scoreMatrix, int sizeB) {
    int i;
    for (i = 0; i < (sizeB + 1); i++) free(scoreMatrix[i]);
    free(scoreMatrix);
}

/* Calcula a média e o desvio padrão dos tempos coletados */
void calculate_statistics(profiling_raw_data *raw, profiling_stats *stats) {
    double sum_alloc = 0, sum_init = 0, sum_compute = 0, sum_total = 0;
    double sum_sq_diff_alloc = 0, sum_sq_diff_init = 0, sum_sq_diff_compute = 0,
           sum_sq_diff_total = 0;

    // Calcular a média
    for (int i = 0; i < NUM_RUNS; i++) {
        sum_alloc += raw->memory_alloc_times[i];
        sum_init += raw->matrix_init_times[i];
        sum_compute += raw->lcs_computation_times[i];
        sum_total += raw->total_times[i];
    }
    stats->alloc_mean = sum_alloc / NUM_RUNS;
    stats->init_mean = sum_init / NUM_RUNS;
    stats->compute_mean = sum_compute / NUM_RUNS;
    stats->total_mean = sum_total / NUM_RUNS;
    stats->file_io_time = raw->file_io_time;

    // Calcular o desvio padrão
    for (int i = 0; i < NUM_RUNS; i++) {
        sum_sq_diff_alloc += pow(raw->memory_alloc_times[i] - stats->alloc_mean, 2);
        sum_sq_diff_init += pow(raw->matrix_init_times[i] - stats->init_mean, 2);
        sum_sq_diff_compute += pow(raw->lcs_computation_times[i] - stats->compute_mean, 2);
        sum_sq_diff_total += pow(raw->total_times[i] - stats->total_mean, 2);
    }
    stats->alloc_stddev = sqrt(sum_sq_diff_alloc / NUM_RUNS);
    stats->init_stddev = sqrt(sum_sq_diff_init / NUM_RUNS);
    stats->compute_stddev = sqrt(sum_sq_diff_compute / NUM_RUNS);
    stats->total_stddev = sqrt(sum_sq_diff_total / NUM_RUNS);
}

/* Imprime os resultados do profiling e a análise da Lei de Amdahl usando as médias */
void print_profiling_results(profiling_stats *stats, int sizeA, int sizeB) {
    // Calcula o tempo sequencial total como a soma das médias das partes, mais o I/O
    double total_sequential_time =
        stats->file_io_time + stats->alloc_mean + stats->init_mean + stats->compute_mean;

    printf("\n======================================================\n");
    printf("PROFILING RESULTS (Statistics over %d runs)\n", NUM_RUNS);
    printf("======================================================\n");
    printf("Sequence A size: %d\n", sizeA);
    printf("Sequence B size: %d\n", sizeB);
    printf("------------------------------------------------------\n");
    printf("Component                    | Mean Time (s)  | Std Dev (s)  | Percentage\n");
    printf("------------------------------------------------------\n");
    printf("File I/O                     | %-14.6f | (one-time)   | %7.2f%%\n", stats->file_io_time,
           (stats->file_io_time / total_sequential_time) * 100);
    printf("Memory Allocation            | %-14.6f | %-12.6f | %7.2f%%\n", stats->alloc_mean,
           stats->alloc_stddev, (stats->alloc_mean / total_sequential_time) * 100);
    printf("Matrix Initialization        | %-14.6f | %-12.6f | %7.2f%%\n", stats->init_mean,
           stats->init_stddev, (stats->init_mean / total_sequential_time) * 100);
    printf("LCS Computation              | %-14.6f | %-12.6f | %7.2f%%\n", stats->compute_mean,
           stats->compute_stddev, (stats->compute_mean / total_sequential_time) * 100);
    printf("------------------------------------------------------\n");
    printf("TOTAL SEQUENTIAL TIME (Mean) | %-14.6f | %-12.6f |  100.00%%\n", total_sequential_time,
           stats->total_stddev);
    printf("======================================================\n");

    // Análise da Lei de Amdahl usando as médias
    double parallelizable_fraction = stats->compute_mean / total_sequential_time;
    double sequential_fraction = 1.0 - parallelizable_fraction;

    printf("\nAMDAHL'S LAW ANALYSIS (based on mean times):\n");
    printf("----------------------------------------\n");
    printf("Parallelizable portion (P): %.4f (%.2f%%)\n", parallelizable_fraction,
           parallelizable_fraction * 100);
    printf("Sequential portion (1-P):   %.4f (%.2f%%)\n", sequential_fraction,
           sequential_fraction * 100);
    printf("\nTheoretical speedup limits:\n");

    int processors[] = {2, 4, 8, 12, 16, 32};
    int num_procs = sizeof(processors) / sizeof(processors[0]);

    for (int i = 0; i < num_procs; i++) {
        double speedup = 1.0 / (sequential_fraction + (parallelizable_fraction / processors[i]));
        printf("  %3d processors: %.2fx speedup\n", processors[i], speedup);
    }
    double max_speedup = 1.0 / sequential_fraction;
    printf("  Inf processors: %.2fx speedup (theoretical maximum)\n", max_speedup);
    printf("========================================\n");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <sequence_file_A> <sequence_file_B>\n", argv[0]);
        return EXIT_FAILURE;
    }

    profiling_raw_data raw_data = {0};
    profiling_stats stats = {0};

    // --- LEITURA DE ARQUIVO (FEITA APENAS UMA VEZ) ---
    double start_time = get_time();
    char *seqA = read_seq(argv[1]);
    char *seqB = read_seq(argv[2]);
    raw_data.file_io_time = get_time() - start_time;

    int sizeA = strlen(seqA);
    int sizeB = strlen(seqB);
    mtype score = 0;

    printf("Starting %d profiling runs for sequences of size %d and %d...\n", NUM_RUNS, sizeA,
           sizeB);

    // --- LOOP DE PROFILING ---
    for (int i = 0; i < NUM_RUNS; i++) {
        double run_start_time = get_time();

        // Aloca a matriz
        double alloc_start_time = get_time();
        mtype **scoreMatrix = allocateScoreMatrix(sizeA, sizeB);
        raw_data.memory_alloc_times[i] = get_time() - alloc_start_time;

        // Inicializa a matriz
        double init_start_time = get_time();
        initScoreMatrix(scoreMatrix, sizeA, sizeB);
        raw_data.matrix_init_times[i] = get_time() - init_start_time;

        // Computa o LCS
        double lcs_start_time = get_time();
        score = LCS(scoreMatrix, sizeA, sizeB, seqA, seqB);
        raw_data.lcs_computation_times[i] = get_time() - lcs_start_time;

        // Libera a matriz (essencial dentro do loop)
        freeScoreMatrix(scoreMatrix, sizeB);

        raw_data.total_times[i] = get_time() - run_start_time;
    }

    printf("Profiling runs completed.\n");

    // --- CÁLCULO DAS ESTATÍSTICAS E RESULTADOS ---

    calculate_statistics(&raw_data, &stats);

    printf("\nLCS Score: %d\n", score);

    print_profiling_results(&stats, sizeA, sizeB);

    // Libera as sequências
    free(seqA);
    free(seqB);

    return EXIT_SUCCESS;
}
