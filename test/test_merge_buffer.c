
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "test.h"
#include "../src/trusted/merge_buffer.h"
#include "../src/trusted/clause_flat.h"

#define DEFAULT_BUFFER_CAPACITY 4096
#define DEFAULT_MAX_NB_LITS 100
#define DEFAULT_NUM_CLAUSES 1000
#define FILE_PATH "merge_buffer_test_file"

// Maximum heap capacity in Byte
unsigned int BUFFER_CAPACITY;
unsigned int MAX_NB_LITS;
unsigned int NUM_CLAUSES;

clause_ptr* clauses;
size_t clauses_size;
struct merge_buffer* buffer;

// ----- UTIL -----

#define MAX(X,Y) (X > Y ? X : Y)

static void init_merge_buffer() {
    printf("   * create new merge_buffer\n");
    buffer = merge_buffer_init(BUFFER_CAPACITY, FILE_PATH);
    do_assert(buffer->file);
    trusted_utils_read_int(buffer->file);   // skip initial int
}

// ----- TEST -----

static void test_merge_buffer_fill() {
    init_merge_buffer();

    printf("   * fill buffer\n");
    merge_buffer_fill(buffer);

    printf("   * check buffer invariants\n");
    do_assert(buffer->size <= buffer->capacity);
    do_assert(buffer->start_idx == 0);
    size_t clauses_in_buffer = get_clause_id(buffer->data[buffer->end_idx - 1]);
    do_assert(buffer->end_idx == buffer->start_idx + clauses_in_buffer);
    do_assert((u64)ftell(buffer->file) == 4 + buffer->size);

    printf("   * empty some buffer content and refil\n");
    size_t deleted_clauses = MAX(1, (buffer->end_idx - buffer->start_idx) / 2);
    buffer->start_idx += deleted_clauses;
    merge_buffer_fill(buffer);

    printf("   * check buffer invariants\n");
    do_assert(buffer->size <= buffer->capacity);
    do_assert(buffer->start_idx == 0);
    clauses_in_buffer = trusted_utils_read_ul(buffer->file) - 1 - deleted_clauses;
    do_assert(buffer->end_idx == buffer->start_idx + clauses_in_buffer);

    merge_buffer_free(buffer);
}

static void test_merge_buffer_next_clause() {
    init_merge_buffer();

    printf("   * check read clauses\n");
    for (size_t i = 0; i < NUM_CLAUSES; i++) {
        clause_ptr c = merge_buffer_next_clause(buffer);
        do_assert(get_clause_id(c) == i + 1);
    }
    do_assert(merge_buffer_next_clause(buffer) == NULL);
    do_assert(fgetc(buffer->file) == EOF);

    merge_buffer_free(buffer);
}

static void test_merge_buffer_set_file_pointer() {
    buffer = merge_buffer_init(BUFFER_CAPACITY, FILE_PATH);

    printf("   * set file pointer various positions in file and check\n");
    merge_buffer_set_file_pointer(buffer, 0);
    do_assert(ftell(buffer->file) == 0);

    // file length for onl< unit clauses
    int min_file_length = ((8 * NUM_CLAUSES) + 4);
    for (size_t i = 0; i < 100; i++) {
        int pos = random() % min_file_length;
        merge_buffer_set_file_pointer(buffer, pos);
        do_assert(ftell(buffer->file) == pos);
    }

    merge_buffer_free(buffer);
}

// ----- INIT -----

static void generate_clauses() {
    printf("   * generate clauses\n");

    clauses_size = 0;
    clauses = (clause_ptr*)trusted_utils_calloc(NUM_CLAUSES, sizeof(clause_ptr));
    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        int nb_lits = ((random() % MAX_NB_LITS) + 1);
        // ensure sorted clause ids
        clauses[i] = create_flat_clause(i + 1, nb_lits, NULL);
        do_assert(clauses[i]);
        for (int j = 0; j < nb_lits; j++)
            get_clause_lits(clauses[i])[j] = random();
        clauses_size += get_clause_size(clauses[i]);
    }
    do_assert(clauses_size > BUFFER_CAPACITY);

    printf("   * generated %u clauses\n", NUM_CLAUSES);
}

static void create_file() {
    printf("   * create file containing generated clauses\n");

    FILE* file = fopen(FILE_PATH, "wb+");
    do_assert(file);
    trusted_utils_write_int(NUM_CLAUSES, file);
    for (size_t i = 0; i < NUM_CLAUSES; i++)
        write_flat_clause_to_file(clauses[i], file);
    fclose(file);
}

static void init_tests() {
    srandom(time(NULL));
    generate_clauses();
    create_file();
}

static void wrap_up_tests() {
    for (size_t i = 0; i < NUM_CLAUSES; i++)
        delete_flat_clause(clauses[i]);
    free(clauses);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("** no arguments given, default to BUFFER_CAPACITY = %uB, MAX_NB_LITS=%u, NUM_CLAUSES=%u\n", DEFAULT_BUFFER_CAPACITY, DEFAULT_MAX_NB_LITS, DEFAULT_NUM_CLAUSES);
        BUFFER_CAPACITY = DEFAULT_BUFFER_CAPACITY;
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
        NUM_CLAUSES = DEFAULT_NUM_CLAUSES;
    } else if (argc == 2) {
        printf("** set BUFFER_CAPACITY = %sB, MAX_NB_LITS=%u, NUM_CLAUSES=%u\n", argv[1], DEFAULT_MAX_NB_LITS, DEFAULT_NUM_CLAUSES);
        BUFFER_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
        NUM_CLAUSES = DEFAULT_NUM_CLAUSES;
    } else if (argc == 3) {
        printf("** set BUFFER_CAPACITY = %sB, MAX_NB_LITS=%s, NUM_CLAUSES=%u\n", argv[1], argv[2], DEFAULT_NUM_CLAUSES);
        BUFFER_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = atoi(argv[2]);
        NUM_CLAUSES = DEFAULT_NUM_CLAUSES;
    } else if (argc == 4) {
        printf("** set BUFFER_CAPACITY = %sB, MAX_NB_LITS=%s, NUM_CLAUSES=%s\n", argv[1], argv[2], argv[3]);
        BUFFER_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = atoi(argv[2]);
        NUM_CLAUSES = atoi(argv[3]);
    } else {
        printf("** can take at most two arguments\n");
        printf("** ABORT\n");
        abort();
    }

    printf("** initialize tests\n");
    init_tests();

    printf("** test merge_buffer_fill\n");
    test_merge_buffer_fill();

    printf("** test merge_buffer_next_clause\n");
    test_merge_buffer_next_clause();

    printf("** test merge_buffer_set_file_pointer\n");
    test_merge_buffer_set_file_pointer();

    printf("** wrap tests up\n");
    wrap_up_tests();

    printf("** DONE\n");
    return 0;
}
