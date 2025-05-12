#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

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
mtype *allocateScoreArray(int sizeA, int sizeB) {
    mtype *scoreArray;
    size_t total = (size_t)(sizeA + 1) * (sizeB + 1) * sizeof(mtype);
    if (posix_memalign((void **)&scoreArray, 64, total) != 0) {
        fprintf(stderr, "Error in posix_memalign for scoreArray\n");
        exit(EXIT_FAILURE);
    }
    return scoreArray;
}

// Initialize the score array to zero
void initScoreArray(mtype *scoreArray, int sizeA, int sizeB) {
    memset(scoreArray, 0, (size_t)(sizeA + 1) * (sizeB + 1) * sizeof(mtype));
}

// Parallel LCS using anti-diagonal iteration
int LCS_Parallel(mtype *scoreArray, int sizeA, int sizeB, const char *restrict seqA,
                 const char *restrict seqB) {
    for (int d = 2; d <= sizeA + sizeB; ++d) {
        int a_min = d > sizeA + 1 ? d - sizeA : 1;
        int a_max = (d - 1) < sizeB ? (d - 1) : sizeB;

#pragma omp parallel for schedule(static)
        for (int a = a_min; a <= a_max; ++a) {
            int b = d - a;
            if (seqB[a - 1] == seqA[b - 1]) {
                SCORE(a, b) = SCORE(a - 1, b - 1) + 1;
            } else {
                mtype up = SCORE(a - 1, b);
                mtype left = SCORE(a, b - 1);
                SCORE(a, b) = up > left ? up : left;
            }
        }
    }
    return SCORE(sizeB, sizeA);
}

// Optional: print the LCS score matrix (for debugging)
void printMatrix(const char *seqA, const char *seqB, mtype *scoreArray, int sizeA, int sizeB) {
    printf("Score Matrix:\n");
    printf("========================================\n");
    printf("    ");
    for (int j = 0; j < sizeA; ++j) printf("%5c ", seqA[j]);
    printf("\n");
    for (int i = 0; i <= sizeB; ++i) {
        if (i == 0)
            printf("   ");
        else
            printf("%3c", seqB[i - 1]);
        for (int j = 0; j <= sizeA; ++j) {
            printf("%5d ", (int)SCORE(i, j));
        }
        printf("\n");
    }
    printf("========================================\n");
}

int main(int argc, char **argv) {
    char *seqA = read_seq("A.in");
    char *seqB = read_seq("B.in");
    int sizeA = strlen(seqA);
    int sizeB = strlen(seqB);

    mtype *scoreArray = allocateScoreArray(sizeA, sizeB);
    initScoreArray(scoreArray, sizeA, sizeB);

    // omp_set_num_threads(4); // adjust as needed
    double start = omp_get_wtime();
    mtype score = LCS_Parallel(scoreArray, sizeA, sizeB, seqA, seqB);
    double end = omp_get_wtime();

#ifdef DEBUGMATRIX
    printMatrix(seqA, seqB, scoreArray, sizeA, sizeB);
#endif

    printf("PARALLEL: %.6fs\n", end - start);
    printf("Score: %d\n", score);

    free(scoreArray);
    free(seqA);
    free(seqB);
    return EXIT_SUCCESS;
}

