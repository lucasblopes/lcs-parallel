#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

int min(int a, int b) {
    if (a < b) return a;
    return b;
}

typedef unsigned short mtype;

/* Read sequence from a file to a char vector.
 Filename is passed as parameter */

char *read_seq(char *fname) {
    // file pointer
    FILE *fseq = NULL;
    // sequence size
    long size = 0;
    // sequence pointer
    char *seq = NULL;
    // sequence index
    int i = 0;

    // open file
    fseq = fopen(fname, "rt");
    if (fseq == NULL) {
        printf("Error reading file %s\n", fname);
        exit(1);
    }

    // find out sequence size to allocate memory afterwards
    fseek(fseq, 0L, SEEK_END);
    size = ftell(fseq);
    rewind(fseq);

    // allocate memory (sequence)
    seq = (char *)calloc(size + 1, sizeof(char));
    if (seq == NULL) {
        printf("Erro allocating memory for sequence %s.\n", fname);
        exit(1);
    }

    // read sequence from file
    while (!feof(fseq)) {
        seq[i] = fgetc(fseq);
        if ((seq[i] != '\n') && (seq[i] != EOF)) i++;
    }
    // insert string terminator
    seq[i] = '\0';

    // close file
    fclose(fseq);

    // return sequence pointer
    return seq;
}

mtype **allocateScoreMatrix(int sizeA, int sizeB) {
    int i;
    // Allocate memory for LCS score matrix
    mtype **scoreMatrix = (mtype **)malloc((sizeB + 1) * sizeof(mtype *));
    for (i = 0; i < (sizeB + 1); i++) scoreMatrix[i] = (mtype *)malloc((sizeA + 1) * sizeof(mtype));
    return scoreMatrix;
}

void initScoreMatrix(mtype **scoreMatrix, int sizeA, int sizeB) {
    int i, j;
    // Fill first line of LCS score matrix with zeroes
    for (j = 0; j < (sizeA + 1); j++) scoreMatrix[0][j] = 0;

    // Do the same for the first collumn
    for (i = 1; i < (sizeB + 1); i++) scoreMatrix[i][0] = 0;
}

int LCS_Parallel_debug(mtype **scoreMatrix, int sizeA, int sizeB, char *seqA, char *seqB) {
    /* sizeA++, sizeB++; */
    printf("LCS_Parallel chamada com sizeA = %d, sizeB = %d\n", sizeA, sizeB);
    for (int i = 0, j = 0; j <= sizeB && i <= sizeA; j++) {
        int n = min(j, (int)sizeA - i);  // Size of the anti-diagonal
        printf("Iteração externa: i = %d, j = %d, n = %d\n", i, j, n);
#pragma omp parallel for
        for (int k = 0; k <= n; ++k)  // parallel for loop for each anti-diagonal element
        {
            int a = i + k, b = j - k;  // for index of the diagonal elements
            printf("  Iteração paralela (thread %d): k = %d, a = %d, b = %d\n",
                   omp_get_thread_num(), k, a, b);
            if (a == 0 || b == 0) {
                scoreMatrix[a][b] = 0;
                printf("    Caso base: scoreMatrix[%d][%d] = 0\n", a, b);
            } else if (a > 0 && a <= sizeA && b > 0 && b <= sizeB) {
                printf("    Acessando seqA[%d] ('%c'), seqB[%d] ('%c')\n", a - 1, seqA[a - 1],
                       b - 1, seqB[b - 1]);
                if (seqA[a - 1] == seqB[b - 1]) {
                    scoreMatrix[a][b] = scoreMatrix[a - 1][b - 1] + 1;
                    printf("    Match: scoreMatrix[%d][%d] = scoreMatrix[%d][%d] + 1 = %d\n", a, b,
                           a - 1, b - 1, scoreMatrix[a][b]);
                } else {
                    scoreMatrix[a][b] = max(scoreMatrix[a - 1][b], scoreMatrix[a][b - 1]);
                    printf(
                        "    No Match: scoreMatrix[%d][%d] = max(scoreMatrix[%d][%d], "
                        "scoreMatrix[%d][%d]) = %d\n",
                        a, b, a - 1, b, a, b - 1, scoreMatrix[a][b]);
                }
            } else {
                printf("    Índices fora dos limites: a = %d, b = %d, sizeA = %d, sizeB = %d\n", a,
                       b, sizeA, sizeB);
            }
        }
        if (j >= sizeB) {
            j = sizeB - 1, i++;
        }
    }
    return scoreMatrix[sizeA][sizeB];
}

int LCS_Parallel(mtype **scoreMatrix, int sizeA, int sizeB, char *seqA, char *seqB) {
    for (int i = 0, j = 0; j <= sizeB && i <= sizeA; j++) {
        int n = min(j, (int)sizeA - i);  // Size of the anti-diagonal
#pragma omp parallel for
        for (int k = 0; k <= n; ++k)  // parallel for loop for each anti-diagonal element
        {
            int a = i + k, b = j - k;  // for index of the diagonal elements
                                       /* if (a == 0 || b == 0) { */
                                       /*     scoreMatrix[a][b] = 0; */
            if (a > 0 && a <= sizeA && b > 0 && b <= sizeB) {
                if (seqA[a - 1] == seqB[b - 1]) {
                    scoreMatrix[a][b] = scoreMatrix[a - 1][b - 1] + 1;
                } else {
                    scoreMatrix[a][b] = max(scoreMatrix[a - 1][b], scoreMatrix[a][b - 1]);
                }
            } else {
            }
        }
        if (j >= sizeB) {
            j = sizeB - 1, i++;
        }
    }
    return scoreMatrix[sizeA][sizeB];
}

void printMatrix(char *seqA, char *seqB, mtype **scoreMatrix, int sizeA, int sizeB) {
    int i, j;

    // print header
    printf("Score Matrix:\n");
    printf("========================================\n");

    // print LCS score matrix allong with sequences
    printf("    ");
    printf("%5c   ", ' ');

    for (j = 0; j < sizeA; j++) printf("%5c   ", seqA[j]);
    printf("\n");
    for (i = 0; i < sizeB + 1; i++) {
        if (i == 0)
            printf("    ");
        else
            printf("%c   ", seqB[i - 1]);
        for (j = 0; j < sizeA + 1; j++) {
            printf("%5d   ", scoreMatrix[i][j]);
        }
        printf("\n");
    }
    printf("========================================\n");
}

void freeScoreMatrix(mtype **scoreMatrix, int sizeB) {
    int i;
    for (i = 0; i < (sizeB + 1); i++) free(scoreMatrix[i]);
    free(scoreMatrix);
}

int main(int argc, char **argv) {
    // sequence pointers for both sequences
    char *seqA, *seqB;

    // sizes of both sequences
    int sizeA, sizeB;

    // read both sequences
    seqA = read_seq("A.in");
    seqB = read_seq("B.in");

    // find out sizes
    sizeA = strlen(seqA);
    sizeB = strlen(seqB);

    // allocate LCS score matrix
    mtype **scoreMatrix = allocateScoreMatrix(sizeA, sizeB);

    // initialize LCS score matrix
    initScoreMatrix(scoreMatrix, sizeA, sizeB);

    // Set the number of threads for OpenMP (optional)
    // Uncomment and adjust if you want to set a specific number of threads
    // omp_set_num_threads(4);

    // fill up the rest of the matrix and return final score (element locate at the last line and
    // collumn)
    double start_time = omp_get_wtime();
    mtype score = LCS_Parallel(scoreMatrix, sizeB, sizeA, seqB, seqA);
    double end_time = omp_get_wtime();

    /* if you wish to see the entire score matrix,
     for debug purposes, define DEBUGMATRIX. */
#ifdef DEBUGMATRIX
    printMatrix(seqA, seqB, scoreMatrix, sizeA, sizeB);
#endif

    // print score
    printf("PARALLEL: %.6fs\n", end_time - start_time);
    printf("Score: %d\n\n", score);

    // free score matrix
    freeScoreMatrix(scoreMatrix, sizeB);

    // Free sequence memory
    free(seqA);
    free(seqB);

    return EXIT_SUCCESS;
}
