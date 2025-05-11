#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUGMATRIX

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

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

// scoreMatrix is assumed to have (sizeB+1) rows of (sizeA+1) ints,
// all initialized to zero in row 0 and col 0 before calling this.
int LCS_Parallel(mtype **scoreMatrix, int sizeA, int sizeB, const char *restrict seqA,
                 const char *restrict seqB) {
#ifdef DEBUGMATRIX
    printf("\nA: %s (%d)\nB: %s (%d)\n\n", seqA, sizeA, seqB, sizeB);
#endif

    // Loop over all anti-diagonals, d = a+b.
    // d runs from 2 (a=1,b=1) up to sizeB+sizeA (a=sizeB,b=sizeA).
    for (int d = 2; d <= sizeB + sizeA; ++d) {
        // Compute legal range for 'a'
        int a_min = d > sizeA + 1 ? (d - sizeA) : 1;
        int a_max = (d - 1) < sizeB ? (d - 1) : sizeB;

#ifdef DEBUGMATRIX
        printf("anti-diagonal d=%d: a in [%d..%d] (n=%d)\n", d, a_min, a_max, a_max - a_min + 1);
#endif

/* #pragma omp parallel for schedule(static) */
#pragma omp parallel for schedule(static)
        for (int a = a_min; a <= a_max; ++a) {
            int b = d - a;  // automatically 1 <= b <= sizeA
#ifdef DEBUGMATRIX
            printf("  [Thread %d] (a=%d, b=%d)\n", omp_get_thread_num(), a, b);
#endif

            // compare seqB[a-1] with seqA[b-1]
            if (seqB[a - 1] == seqA[b - 1]) {
                scoreMatrix[a][b] = scoreMatrix[a - 1][b - 1] + 1;
            } else {
                // take the max of up (a-1,b) and left (a,b-1)
                int up = scoreMatrix[a - 1][b];
                int left = scoreMatrix[a][b - 1];
                scoreMatrix[a][b] = (up > left ? up : left);
            }
        }
    }

    return scoreMatrix[sizeB][sizeA];
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
    /* mtype score = LCS_Parallel_debug(scoreMatrix, sizeA, sizeB, seqA, seqB); */
    mtype score = LCS_Parallel(scoreMatrix, sizeA, sizeB, seqA, seqB);
    double end_time = omp_get_wtime();

    /* if you wish to see the entire score matrix,
     for debug purposes, define DEBUGMATRIX. */
#ifdef DEBUGMATRIX
    printMatrix(seqA, seqB, scoreMatrix, sizeA, sizeB);
#endif

    // print score
    printf("PARALLEL: %.6fs\n", end_time - start_time);
    printf("Score: %d\n", score);

    // free score matrix
    freeScoreMatrix(scoreMatrix, sizeB);

    // Free sequence memory
    free(seqA);
    free(seqB);

    return EXIT_SUCCESS;
}
