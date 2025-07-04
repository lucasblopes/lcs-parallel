#include <omp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* #define DEBUGMATRIX */
/* #define DEBUGSTEPS */

typedef unsigned short mtype;

// Macro to access the flattened score array
#define SCORE(i, j) scoreArray[(size_t)(i) * (sizeA + 1) + (j)]

// Read sequence from a file into a char vector
char *read_seq(const char *fname) {
    FILE *fseq = fopen(fname, "rt");
    if (!fseq) {
        fprintf(stderr, "Error reading file %s\n", fname);
        exit(EXIT_FAILURE);
    }
    fseek(fseq, 0L, SEEK_END);
    long size = ftell(fseq);
    rewind(fseq);

    char *seq = calloc(size + 1, sizeof(char));
    if (!seq) {
        fprintf(stderr, "Error allocating memory for sequence %s.\n", fname);
        exit(EXIT_FAILURE);
    }
    int i = 0;
    int c;
    while ((c = fgetc(fseq)) != EOF) {
        if (c == '\n') continue;
        seq[i++] = (char)c;
    }
    seq[i] = '\0';
    fclose(fseq);
    return seq;
}

// Allocate a flattened score array with SIMD-friendly alignment
mtype *allocateScoreArray(size_t sizeA, size_t sizeB) {
    mtype *scoreArray;
    size_t total = (size_t)(sizeA + 1) * (sizeB + 1) * sizeof(mtype);
    if (posix_memalign((void **)&scoreArray, 64, total) != 0) {
        fprintf(stderr, "Error in posix_memalign for scoreArray\n");
        exit(EXIT_FAILURE);
    }
    return scoreArray;
}

// Initialize the score array to zero
void initScoreArray(mtype *scoreArray, size_t sizeA, size_t sizeB) {
    memset(scoreArray, 0, (size_t)(sizeA + 1) * (sizeB + 1) * sizeof(mtype));
}

// Parallel LCS using anti-diagonal iteration
int LCS_Parallel(mtype *restrict scoreArray, size_t sizeA, size_t sizeB, const char *restrict seqA,
                 const char *restrict seqB) {
    double start_lcs = omp_get_wtime();
    double parallel_time = 0.0;

#ifdef DEBUGSTEPS
    printf("\nA: %s (%d)\nB: %s (%d)\n\n", seqA, sizeA, seqB, sizeB);
#endif

    // The outer loop iterates sequentially through each antidiagonal
    int num_diag = sizeA + sizeB;  // number of antidiaginals
    for (int d = 2; d <= num_diag; ++d) {
        int a_min = d > sizeA + 1 ? d - sizeA : 1;
        int a_max = (d - 1) < sizeB ? (d - 1) : sizeB;

#ifdef DEBUGSTEPS
        printf("anti-diagonal d=%d: a in [%d..%d] (n=%d)\n", d, a_min, a_max, a_max - a_min + 1);
#endif

        double start_parallel = omp_get_wtime();
#pragma omp parallel for schedule(static)
        // The inner loop iterates through each element of the current antidiagonal parallelly
        for (int a = a_min; a <= a_max; ++a) {
            int b = d - a;

#ifdef DEBUGSTEPS
            printf("  [Thread %d] (a=%d, b=%d)\n", omp_get_thread_num(), a, b);
#endif

            if (seqB[a - 1] == seqA[b - 1]) {
                SCORE(a, b) = SCORE(a - 1, b - 1) + 1;
            } else {
                mtype up = SCORE(a - 1, b);
                mtype left = SCORE(a, b - 1);
                SCORE(a, b) = up > left ? up : left;
            }
        }

        double end_parallel = omp_get_wtime();
        parallel_time += (end_parallel - start_parallel);
    }

    double end_lcs = omp_get_wtime();
    double total_lcs_time = end_lcs - start_lcs;
    double sequential_overhead = total_lcs_time - parallel_time;

    printf("Total time: %.6fs\n", total_lcs_time);
    printf("Parallel time: %.6fs\n", parallel_time);
    printf("Sequential time: %.6fs\n", sequential_overhead);

    return SCORE(sizeB, sizeA);
}

void printMatrix(const char *seqA, const char *seqB, mtype *scoreArray, size_t sizeA,
                 size_t sizeB) {
    int i, j;

    // print header
    printf("Score Matrix:\n");
    printf("========================================\n");

    // print top row (empty corner + seqA)
    printf("    ");
    printf("%5c   ", ' ');

    for (j = 0; j < sizeA; j++) {
        printf("%5c   ", seqA[j]);
    }
    printf("\n");

    for (i = 0; i <= sizeB; i++) {
        if (i == 0)
            printf("    ");
        else
            printf("%c   ", seqB[i - 1]);

        for (j = 0; j <= sizeA; j++) {
            printf("%5d   ", (int)scoreArray[i * (sizeA + 1) + j]);
        }
        printf("\n");
    }

    printf("========================================\n");
}

int main(int argc, char **argv) {
    char *seqA = read_seq("A.in");
    char *seqB = read_seq("B.in");
    size_t sizeA = strlen(seqA);
    size_t sizeB = strlen(seqB);

    mtype *scoreArray = allocateScoreArray(sizeA, sizeB);
    initScoreArray(scoreArray, sizeA, sizeB);

    mtype score = LCS_Parallel(scoreArray, sizeA, sizeB, seqA, seqB);

#ifdef DEBUGMATRIX
    printMatrix(seqA, seqB, scoreArray, sizeA, sizeB);
#endif

    printf("Score: %d\n", score);

    free(scoreArray);
    free(seqA);
    free(seqB);
    return EXIT_SUCCESS;
}
