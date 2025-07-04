#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define DEBUGMATRIX
#define BLOCK_SIZE 192  // ~192x192x2 = 73KB, leaves space for other variables

typedef unsigned short matrix_element_type;

// ========================================
// MATRIX MANAGEMENT FUNCTIONS
// ========================================

/**
 * Allocates memory for a 2D matrix
 * @param sequence_a_length Length of sequence A
 * @param sequence_b_length Length of sequence B
 * @return Pointer to allocated matrix
 */
matrix_element_type **allocate_lcs_matrix(int sequence_a_length, int sequence_b_length) {
    matrix_element_type **matrix =
        (matrix_element_type **)malloc((sequence_b_length + 1) * sizeof(matrix_element_type *));
    for (int i = 0; i < (sequence_b_length + 1); i++) {
        matrix[i] =
            (matrix_element_type *)malloc((sequence_a_length + 1) * sizeof(matrix_element_type));
    }
    return matrix;
}

/**
 * Initializes the LCS matrix with zeros in first row and column
 * @param matrix The matrix to initialize
 * @param sequence_a_length Length of sequence A
 * @param sequence_b_length Length of sequence B
 */
void initialize_lcs_matrix(matrix_element_type **matrix, int sequence_a_length,
                           int sequence_b_length) {
    // Initialize first row (sequence A base case)
    for (int i = 0; i < (sequence_a_length + 1); i++) {
        matrix[0][i] = 0;
    }
    // Initialize first column (sequence B base case)
    for (int i = 1; i < (sequence_b_length + 1); i++) {
        matrix[i][0] = 0;
    }
}

/**
 * Prints the complete LCS matrix for debugging purposes
 * @param sequence_a First sequence
 * @param sequence_b Second sequence
 * @param matrix The LCS matrix
 * @param sequence_a_length Length of sequence A
 * @param sequence_b_length Length of sequence B
 */
void print_lcs_matrix(char *sequence_a, char *sequence_b, matrix_element_type **matrix,
                      int sequence_a_length, int sequence_b_length) {
    printf("Score Matrix:\n");
    printf("========================================\n");
    printf("    ");
    printf("%5c   ", ' ');
    for (int i = 0; i < sequence_a_length; i++) {
        printf("%5c   ", sequence_a[i]);
    }
    printf("\n");
    for (int i = 0; i < sequence_b_length + 1; i++) {
        if (i == 0) {
            printf("    ");
        } else {
            printf("%c   ", sequence_b[i - 1]);
        }
        for (int j = 0; j < sequence_a_length + 1; j++) {
            printf("%5d   ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("========================================\n");
}

/**
 * Frees the allocated memory for the LCS matrix
 * @param matrix The matrix to free
 * @param sequence_b_length Length of sequence B
 */
void free_lcs_matrix(matrix_element_type **matrix, int sequence_b_length) {
    for (int i = 0; i < (sequence_b_length + 1); i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// ========================================
// FILE I/O FUNCTIONS
// ========================================

/**
 * Reads a sequence from a file
 * @param filename Name of the file to read
 * @return Pointer to the sequence string
 */
char *read_sequence_from_file(char *filename) {
    FILE *file_pointer = NULL;
    long file_size = 0;
    char *sequence = NULL;
    int character_index = 0;

    file_pointer = fopen(filename, "rt");
    if (file_pointer == NULL) {
        printf("Error reading file %s\n", filename);
        exit(1);
    }

    // Get file size
    fseek(file_pointer, 0L, SEEK_END);
    file_size = ftell(file_pointer);
    rewind(file_pointer);

    // Allocate memory for sequence
    sequence = (char *)calloc(file_size + 1, sizeof(char));
    if (sequence == NULL) {
        printf("Error allocating memory for sequence %s.\n", filename);
        exit(1);
    }

    // Read sequence character by character, ignoring newlines
    while (!feof(file_pointer)) {
        sequence[character_index] = fgetc(file_pointer);
        if ((sequence[character_index] != '\n') && (sequence[character_index] != EOF)) {
            character_index++;
        }
    }
    sequence[character_index] = '\0';
    fclose(file_pointer);

    return sequence;
}

// ========================================
// LCS COMPUTATION FUNCTIONS
// ========================================

/**
 * Computes LCS for a specific block in the matrix using dynamic programming
 * @param matrix The LCS matrix
 * @param sequence_a First sequence
 * @param sequence_b Second sequence
 * @param block_row_index Row index of the block
 * @param block_col_index Column index of the block
 * @param sequence_a_length Length of sequence A
 * @param sequence_b_length Length of sequence B
 */
void compute_lcs_block(matrix_element_type **matrix, char *sequence_a, char *sequence_b,
                       int block_row_index, int block_col_index, int sequence_a_length,
                       int sequence_b_length) {
    // Calculate block boundaries
    int row_start = block_row_index * BLOCK_SIZE + 1;
    int row_end = min((block_row_index + 1) * BLOCK_SIZE, sequence_b_length);
    int col_start = block_col_index * BLOCK_SIZE + 1;
    int col_end = min((block_col_index + 1) * BLOCK_SIZE, sequence_a_length);

    // Apply LCS dynamic programming algorithm to the block
    for (int i = row_start; i <= row_end; i++) {
        for (int j = col_start; j <= col_end; j++) {
            if (sequence_a[j - 1] == sequence_b[i - 1]) {
                // Characters match: take diagonal value + 1
                matrix[i][j] = matrix[i - 1][j - 1] + 1;
            } else {
                // Characters don't match: take maximum of left and top
                matrix[i][j] = max(matrix[i - 1][j], matrix[i][j - 1]);
            }
        }
    }
}

// ========================================
// MPI COMMUNICATION FUNCTIONS
// ========================================

/**
 * Receives horizontal dependency data from the block above
 * @param matrix The LCS matrix
 * @param block_row_index Row index of current block
 * @param block_col_index Column index of current block
 * @param sequence_a_length Length of sequence A
 * @param total_col_blocks Total number of column blocks
 * @param world_size Number of MPI processes
 */
void receive_horizontal_dependency(matrix_element_type **matrix, int block_row_index,
                                   int block_col_index, int sequence_a_length, int total_col_blocks,
                                   int world_size) {
    if (block_row_index > 0) {
        int source_rank = ((block_row_index - 1) * total_col_blocks + block_col_index) % world_size;
        int row_to_receive =
            block_row_index * BLOCK_SIZE;  // halo cell -> row to receive form other process
        int col_start = block_col_index * BLOCK_SIZE + 1;
        int col_end = min((block_col_index + 1) * BLOCK_SIZE, sequence_a_length);
        int elements_count = col_end - col_start + 1;

        MPI_Recv(&matrix[row_to_receive][col_start - 1], elements_count + 1, MPI_UNSIGNED_SHORT,
                 source_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
}

/**
 * Receives vertical dependency data from the block to the left
 * @param matrix The LCS matrix
 * @param block_row_index Row index of current block
 * @param block_col_index Column index of current block
 * @param sequence_b_length Length of sequence B
 * @param total_col_blocks Total number of column blocks
 * @param world_size Number of MPI processes
 */
void receive_vertical_dependency(matrix_element_type **matrix, int block_row_index,
                                 int block_col_index, int sequence_b_length, int total_col_blocks,
                                 int world_size) {
    if (block_col_index > 0) {
        int source_rank = (block_row_index * total_col_blocks + (block_col_index - 1)) % world_size;
        int row_start = block_row_index * BLOCK_SIZE + 1;
        int row_end = min((block_row_index + 1) * BLOCK_SIZE, sequence_b_length);
        int col_to_receive = block_col_index * BLOCK_SIZE;
        int elements_count = row_end - row_start + 1;

        matrix_element_type *temp_column =
            (matrix_element_type *)malloc(elements_count * sizeof(matrix_element_type));
        MPI_Recv(temp_column, elements_count, MPI_UNSIGNED_SHORT, source_rank, 1, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        // Copy received data to matrix
        for (int k = 0; k < elements_count; ++k) {
            matrix[row_start + k][col_to_receive] = temp_column[k];
        }
        free(temp_column);
    }
}

/**
 * Sends horizontal data to the block below
 * @param matrix The LCS matrix
 * @param block_row_index Row index of current block
 * @param block_col_index Column index of current block
 * @param sequence_a_length Length of sequence A
 * @param total_row_blocks Total number of row blocks
 * @param total_col_blocks Total number of column blocks
 * @param world_size Number of MPI processes
 */
void send_horizontal_data(matrix_element_type **matrix, int block_row_index, int block_col_index,
                          int sequence_a_length, int total_row_blocks, int total_col_blocks,
                          int world_size) {
    if (block_row_index < total_row_blocks - 1) {
        int dest_rank = ((block_row_index + 1) * total_col_blocks + block_col_index) % world_size;
        int row_to_send = (block_row_index + 1) * BLOCK_SIZE;
        int col_start = block_col_index * BLOCK_SIZE + 1;
        int col_end = min((block_col_index + 1) * BLOCK_SIZE, sequence_a_length);
        int elements_count = col_end - col_start + 1;

        // -1 to add the diagonal element
        MPI_Send(&matrix[row_to_send][col_start - 1], elements_count + 1, MPI_UNSIGNED_SHORT,
                 dest_rank, 0, MPI_COMM_WORLD);
    }
}

/**
 * Sends vertical data to the block to the right
 * @param matrix The LCS matrix
 * @param block_row_index Row index of current block
 * @param block_col_index Column index of current block
 * @param sequence_b_length Length of sequence B
 * @param total_col_blocks Total number of column blocks
 * @param world_size Number of MPI processes
 */
void send_vertical_data(matrix_element_type **matrix, int block_row_index, int block_col_index,
                        int sequence_b_length, int total_col_blocks, int world_size) {
    if (block_col_index < total_col_blocks - 1) {
        int dest_rank = (block_row_index * total_col_blocks + (block_col_index + 1)) % world_size;
        int row_start = block_row_index * BLOCK_SIZE + 1;
        int row_end = min((block_row_index + 1) * BLOCK_SIZE, sequence_b_length);
        int col_to_send = (block_col_index + 1) * BLOCK_SIZE;
        int elements_count = row_end - row_start + 1;

        matrix_element_type *temp_column =
            (matrix_element_type *)malloc(elements_count * sizeof(matrix_element_type));

        // Copy data to temporary column
        for (int k = 0; k < elements_count; ++k) {
            temp_column[k] = matrix[row_start + k][col_to_send];
        }

        MPI_Send(temp_column, elements_count, MPI_UNSIGNED_SHORT, dest_rank, 1, MPI_COMM_WORLD);
        free(temp_column);
    }
}

/**
 * Processes a single block in the wavefront algorithm
 * @param matrix The LCS matrix
 * @param sequence_a First sequence
 * @param sequence_b Second sequence
 * @param block_row_index Row index of the block
 * @param block_col_index Column index of the block
 * @param sequence_a_length Length of sequence A
 * @param sequence_b_length Length of sequence B
 * @param total_row_blocks Total number of row blocks
 * @param total_col_blocks Total number of column blocks
 * @param world_size Number of MPI processes
 */
void process_wavefront_block(matrix_element_type **matrix, char *sequence_a, char *sequence_b,
                             int block_row_index, int block_col_index, int sequence_a_length,
                             int sequence_b_length, int total_row_blocks, int total_col_blocks,
                             int world_size) {
    // Step 1: Receive dependencies from neighboring blocks
    receive_horizontal_dependency(matrix, block_row_index, block_col_index, sequence_a_length,
                                  total_col_blocks, world_size);
    receive_vertical_dependency(matrix, block_row_index, block_col_index, sequence_b_length,
                                total_col_blocks, world_size);

    // Step 2: Compute the LCS for this block
    compute_lcs_block(matrix, sequence_a, sequence_b, block_row_index, block_col_index,
                      sequence_a_length, sequence_b_length);

    // Step 3: Send computed data to dependent blocks
    send_horizontal_data(matrix, block_row_index, block_col_index, sequence_a_length,
                         total_row_blocks, total_col_blocks, world_size);
    send_vertical_data(matrix, block_row_index, block_col_index, sequence_b_length,
                       total_col_blocks, world_size);
}

// ========================================
// MATRIX RECONSTRUCTION FUNCTIONS
// ========================================

/**
 * Reconstructs the complete matrix at rank 0 for debugging
 * @param matrix The LCS matrix
 * @param total_row_blocks Total number of row blocks
 * @param total_col_blocks Total number of column blocks
 * @param sequence_a_length Length of sequence A
 * @param sequence_b_length Length of sequence B
 * @param world_size Number of MPI processes
 * @param current_rank Current MPI rank
 */
void reconstruct_matrix_at_root(matrix_element_type **matrix, int total_row_blocks,
                                int total_col_blocks, int sequence_a_length, int sequence_b_length,
                                int world_size, int current_rank) {
#ifdef DEBUGMATRIX
    const int RECONSTRUCTION_TAG = 200;  // Tag for reconstruction communication

    if (current_rank == 0) {
        // Root process receives all blocks from other processes
        for (int block_row = 0; block_row < total_row_blocks; block_row++) {
            for (int block_col = 0; block_col < total_col_blocks; block_col++) {
                int owner_rank = (block_row * total_col_blocks + block_col) % world_size;

                if (owner_rank != 0) {
                    // Calculate exact block boundaries
                    int row_start = block_row * BLOCK_SIZE + 1;
                    int row_end = min((block_row + 1) * BLOCK_SIZE, sequence_b_length);
                    int col_start = block_col * BLOCK_SIZE + 1;
                    int col_end = min((block_col + 1) * BLOCK_SIZE, sequence_a_length);

                    // Receive block data row by row
                    for (int i = row_start; i <= row_end; i++) {
                        MPI_Recv(&matrix[i][col_start], (col_end - col_start + 1),
                                 MPI_UNSIGNED_SHORT, owner_rank, RECONSTRUCTION_TAG + i,
                                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                }
            }
        }
    } else {
        // Other processes send their owned blocks to root
        for (int block_row = 0; block_row < total_row_blocks; block_row++) {
            for (int block_col = 0; block_col < total_col_blocks; block_col++) {
                int owner_rank = (block_row * total_col_blocks + block_col) % world_size;

                if (owner_rank == current_rank) {
                    // Calculate exact block boundaries
                    int row_start = block_row * BLOCK_SIZE + 1;
                    int row_end = min((block_row + 1) * BLOCK_SIZE, sequence_b_length);
                    int col_start = block_col * BLOCK_SIZE + 1;
                    int col_end = min((block_col + 1) * BLOCK_SIZE, sequence_a_length);

                    // Send block data row by row
                    for (int i = row_start; i <= row_end; i++) {
                        MPI_Send(&matrix[i][col_start], (col_end - col_start + 1),
                                 MPI_UNSIGNED_SHORT, 0, RECONSTRUCTION_TAG + i, MPI_COMM_WORLD);
                    }
                }
            }
        }
    }
#endif
}

// ========================================
// MAIN FUNCTION
// ========================================

int main(int argc, char **argv) {
    int current_rank, world_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &current_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    char *sequence_a, *sequence_b;
    int sequence_a_length = 0, sequence_b_length = 0;

    // Root process reads input sequences
    if (current_rank == 0) {
        if (argc < 3) {
            printf("Usage: mpirun -np <num_procs> %s <fileA.in> <fileB.in>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        sequence_a = read_sequence_from_file(argv[1]);
        sequence_b = read_sequence_from_file(argv[2]);
        sequence_a_length = strlen(sequence_a);
        sequence_b_length = strlen(sequence_b);
    }

    // Broadcast sequence lengths and sequences to all processes
    MPI_Bcast(&sequence_a_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&sequence_b_length, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (current_rank != 0) {
        sequence_a = (char *)malloc((sequence_a_length + 1) * sizeof(char));
        sequence_b = (char *)malloc((sequence_b_length + 1) * sizeof(char));
    }

    MPI_Bcast(sequence_a, sequence_a_length + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(sequence_b, sequence_b_length + 1, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Initialize LCS matrix
    matrix_element_type **lcs_matrix = allocate_lcs_matrix(sequence_a_length, sequence_b_length);
    initialize_lcs_matrix(lcs_matrix, sequence_a_length, sequence_b_length);

    // Calculate block dimensions
    // teto para calcular 1 bloco a mais caso divisao nao exata
    int total_row_blocks = (sequence_b_length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int total_col_blocks = (sequence_a_length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Start timing
    double start_time, end_time;
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    // *** WAVEFRONT ALGORITHM EXECUTION ***
    // Process blocks diagonally to respect dependencies
    for (int diagonal = 0; diagonal < total_row_blocks + total_col_blocks - 1; ++diagonal) {
        // Calculate which blocks belong to this diagonal
        int min_block_row = max(0, diagonal - total_col_blocks + 1);
        int max_block_row = min(diagonal, total_row_blocks - 1);

        for (int block_row = min_block_row; block_row <= max_block_row; ++block_row) {
            int block_col = diagonal - block_row;
            // Distributes the blocks cyclically with the mod operator: 0, 1, 2, 3, 0, 1 ...
            int owner_rank = (block_row * total_col_blocks + block_col) % world_size;

            // Only the owner process works on this block
            if (current_rank == owner_rank) {
                process_wavefront_block(lcs_matrix, sequence_a, sequence_b, block_row, block_col,
                                        sequence_a_length, sequence_b_length, total_row_blocks,
                                        total_col_blocks, world_size);
            }
        }
    }

    // End timing
    MPI_Barrier(MPI_COMM_WORLD);
    end_time = MPI_Wtime();

    // Reconstruct complete matrix at root for debugging
    reconstruct_matrix_at_root(lcs_matrix, total_row_blocks, total_col_blocks, sequence_a_length,
                               sequence_b_length, world_size, current_rank);

    // Collect and print results
    if (current_rank == 0) {
        matrix_element_type final_lcs_score = lcs_matrix[sequence_b_length][sequence_a_length];

        // Uncomment the line below to print the complete matrix for debugging
        // print_lcs_matrix(sequence_a, sequence_b, lcs_matrix, sequence_a_length,
        // sequence_b_length);

        printf("\nScore: %d\n", final_lcs_score);
        printf("PARALLEL: %fs\n", end_time - start_time);
    }

    // Cleanup
    free_lcs_matrix(lcs_matrix, sequence_b_length);
    free(sequence_a);
    free(sequence_b);
    MPI_Finalize();

    return EXIT_SUCCESS;
}
