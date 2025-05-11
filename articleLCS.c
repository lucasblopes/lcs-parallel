
int LCS_Parallel_debug(mtype **scoreMatrix, int sizeA, int sizeB, char *seqA, char *seqB) {
    printf("\nA: %s (%d)\nB: %s (%d)\n", seqA, sizeA, seqB, sizeB);
    for (int i = 0, j = 0; j <= sizeA && i <= sizeB; j++) {
        int n = min(j, (int)sizeB - i);  // Size of the anti-diagonal

        printf("\nanti-diagonal i=%d, j=%d, n=%d\n", i, j, n);

#pragma omp parallel for
        for (int k = 0; k <= n; ++k) {
            int a = i + k;
            int b = j - k;

            printf("Thread %d [a,b] = [%d,%d]\n", omp_get_thread_num(), a, b);

            if (a > 0 && a <= sizeB && b > 0 && b <= sizeA) {
                if (seqB[a - 1] == seqA[b - 1]) {
                    scoreMatrix[a][b] = scoreMatrix[a - 1][b - 1] + 1;
                } else {
                    scoreMatrix[a][b] = max(scoreMatrix[a - 1][b], scoreMatrix[a][b - 1]);
                }
            } else {
                printf("Thread %d skipping out-of-bound a=%d, b=%d\n", omp_get_thread_num(), a, b);
            }
        }

        if (j >= sizeA) {
            j = sizeA - 1;
            i++;
        }
    }

    printf("Final result: scoreMatrix[%d][%d] = %d\n", sizeB, sizeA, scoreMatrix[sizeB][sizeA]);

    return scoreMatrix[sizeB][sizeA];
}
