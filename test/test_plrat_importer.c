
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#include "test.h"
#include "../src/trusted/heap.h"
#include "../src/trusted/clause_flat.h"
#include "../src/trusted/commutative_sig.h"
#include "../src/trusted/int_vec.h"

#include "../src/trusted/plrat_importer.h"

#ifndef UNIT_TEST
#define UNIT_TEST
#endif

#define DEFAULT_HEAP_CAPACITY 1024 * 1024
#define DEFAULT_MAX_NB_LITS 100
#define TEST_DIR "plrat_importer_test_dir"

// Maximum heap capacity in Byte
unsigned int HEAP_CAPACITY;
unsigned int MAX_NB_LITS;
unsigned int NUM_CLAUSES;

clause_ptr* clauses;
struct clause_heap* heap;
struct int_vec* lits_buffer;

// ----- UTIL -----

// insert clauses with duplicates until heap is full
static size_t fill_heap_with_duplicates(unsigned int offset) {
    printf("   * insert duplicate clauses into heap\n");
    size_t unique_clause_cnt = 0;
    bool exit_loop = false;
    while (true) {
        for (size_t i = 0; i < unique_clause_cnt; i++) {
            size_t idx = (offset + unique_clause_cnt) % NUM_CLAUSES;
            clause_ptr c = clauses[idx];
            clause_ptr c_copy = create_flat_clause(get_clause_id(c),
                                                   get_clause_nb_lits(c),
                                                   get_clause_lits(c));
            if (heap_insert(heap, c_copy)) {
                exit_loop = true;
                delete_flat_clause(c_copy);
                break;
            }
        }
        if (exit_loop) {
            break;
        }
        unique_clause_cnt++;
    }
    printf("   * inserted %lu unique clauses %lu times\n", unique_clause_cnt, heap->element_count);
    return unique_clause_cnt;
}

static void check_simple_flush() {
    printf("   * init new importer: ");
    plrat_importer_init(TEST_DIR, 0, 1, 1, 1);
    size_t unique_clause_cnt = fill_heap_with_duplicates(0);

    struct comm_sig* expected_sig = comm_sig_init(SECRET_KEY_2);
    u8 read_sig[16];
    // offset clauses by 1, to accomodate quirks in fill_heap_with_duplicates()
    for (size_t i = 1; i <= unique_clause_cnt; i++)
        comm_sig_update_clause(expected_sig,
                               get_clause_id(clauses[i]),
                               get_clause_lits(clauses[i]),
                               (u64)get_clause_nb_lits(clauses[i]));
    u8* expected_sig_dig = comm_sig_digest(expected_sig);

    printf("   * flush 1/10 of heap to empty file\n");
    u64 old_size = heap->size;
    flush_heap_to_file(heap, 0, .9);
    do_assert(heap->size < old_size);
    do_assert(heap->size <= .9 * heap->capacity);

    printf("   * flush rest of heap to file\n");
    flush_heap_to_file(heap, 0, 0);
    do_assert(heap->size == 0);

    printf("   * check written clauses\n");
    char filename[512];
    snprintf(filename, 512, "%s", get_plrat_importer_out_file_name());
    plrat_importer_end();
    FILE* file = fopen(filename, "rb");
    unsigned int clauses_in_file = trusted_utils_read_int(file);
    do_assert(clauses_in_file == unique_clause_cnt);

    for (size_t i = 0; i < clauses_in_file; i++) {
        trusted_utils_read_ul(file);
        int nb_lits = trusted_utils_read_int(file);
        int_vec_resize(lits_buffer, nb_lits);
        trusted_utils_read_ints(lits_buffer->data, nb_lits, file);
    }
    trusted_utils_read_sig(read_sig, file);

    do_assert(fgetc(file) == EOF);
    for (size_t i = 0; i < 16; i++)
        do_assert(read_sig[i] == expected_sig_dig[i]);

    comm_sig_free(expected_sig);
    free(expected_sig_dig);
    fclose(file);
}

static void check_merge_flush() {
    printf("   * init new importer with some clauses alredy in file: ");

    // init plrat importer with some clauses in file
    struct comm_sig* expected_sig = comm_sig_init(SECRET_KEY_2);
    u8 read_sig[16];
    do_assert(heap->size == 0);
    plrat_importer_init(TEST_DIR, 0, 1, 1, HEAP_CAPACITY);
    size_t unique_clause_cnt = NUM_CLAUSES / 2;
    for (size_t i = 0; i < NUM_CLAUSES / 2; i++) {
        clause_ptr clause_cpy = create_flat_clause(get_clause_id(clauses[i]),
                                                    get_clause_nb_lits(clauses[i]),
                                                    get_clause_lits(clauses[i]));
        heap_insert(heap, clause_cpy);
        comm_sig_update_clause(expected_sig,
                               get_clause_id(clauses[i]),
                               get_clause_lits(clauses[i]),
                               get_clause_nb_lits(clauses[i]));
    }
    flush_heap_to_file(heap, 0, 0);
    
    // ensure a smaller id than whatever clauses already in the file
    int lit = 1;
    clause_ptr small_id_clause = create_flat_clause(0,1,&lit);
    heap_insert(heap, small_id_clause);
    unique_clause_cnt += 1;
    comm_sig_update_clause(expected_sig,
                           get_clause_id(small_id_clause),
                           get_clause_lits(small_id_clause),
                           get_clause_nb_lits(small_id_clause));
    size_t tmp = fill_heap_with_duplicates(NUM_CLAUSES / 2);
    unique_clause_cnt += tmp;
    for (size_t i = (NUM_CLAUSES / 2) + 1; i <= (NUM_CLAUSES / 2) + tmp; i++) {
        comm_sig_update_clause(expected_sig,
                               get_clause_id(clauses[i]),
                               get_clause_lits(clauses[i]),
                               (u64)get_clause_nb_lits(clauses[i]));
    }
    u8* expected_sig_dig = comm_sig_digest(expected_sig);
    
    printf("   * flush heap to already existing file\n");
    flush_heap_to_file(heap, 0, 0);

    printf("   * check written clauses\n");
    char filename[512];
    snprintf(filename, 512, "%s", get_plrat_importer_out_file_name());
    plrat_importer_end();
    FILE* file = fopen(filename, "rb");
    
    // read clauses
    unsigned int clauses_in_file = trusted_utils_read_int(file);
    for (size_t i = 0; i < clauses_in_file; i++) {
        trusted_utils_read_ul(file);
        int nb_lits = trusted_utils_read_int(file);
        int_vec_resize(lits_buffer, nb_lits);
        trusted_utils_read_ints(lits_buffer->data, nb_lits, file);
    }
    trusted_utils_read_sig(read_sig, file);

    do_assert(clauses_in_file == unique_clause_cnt);
    do_assert(heap->size == 0);

    do_assert(fgetc(file) == EOF);
    for (size_t i = 0; i < 16; i++)
        do_assert(read_sig[i] == expected_sig_dig[i]);

    comm_sig_free(expected_sig);
    free(expected_sig_dig);
    fclose(file);
}

static void check_doubling_id() {
    printf("   * check detection of differing clauses with same id\n");
    int pid = fork();
    int status;
    do_assert(pid >= 0);
    if (pid == 0) {
        // Mute child process
        if (!freopen("/dev/null", "w", stdout))
            printf("   * could not mute child process\n");

        plrat_importer_init(TEST_DIR, 0, 1, 1, HEAP_CAPACITY);

        int lits1[3] = {1,2,3};
        int lits2[3] = {4,5,6};
        clause_ptr c1 = create_flat_clause(1,3,lits1);
        clause_ptr c2 = create_flat_clause(1,3,lits2);

        char filename[512];
        snprintf(filename, 512, "%s/0/error_test_file", TEST_DIR);
        FILE* file = fopen(filename, "wb+");
        trusted_utils_write_int(0, file);
        if (file == NULL)
            exit(0);    // we expect process to abort, thus exit(0) for issues

        write_flat_clause_to_file(c2, file);
        set_plrat_importer_out_file(file);
        set_plrat_importer_max_id(1);
        
        if (heap->size > 0)
            exit(0);
        heap_insert(heap, c1);

        flush_heap_to_file(heap, 0, 0);     // expected to abort

        exit(0);
    } else
        wait(&status);

    do_assert(status != 0);
}

// ----- TEST -----

static void test_skip_heap_duplicates() {
    size_t unique_clause_cnt = fill_heap_with_duplicates(0);

    printf("   * check flush with duplicate skipping\n");
    for (size_t i = 0; i < unique_clause_cnt; i++) {
        do_assert(heap->element_count > 0);
        clause_ptr c = heap_pop_min(heap);
        skip_heap_duplicates(c, heap);
        if (heap_get_min(heap)) {
            do_assert(get_clause_id(c) != get_clause_id(heap_get_min(heap)));
        }
        delete_flat_clause(c);
    }
    do_assert(heap->size == 0);
    do_assert(heap->element_count == 0);

    printf("   * check detection of differing clauses with same id\n");
    int pid = fork();
    int status;
    do_assert(pid >= 0);
    if (pid == 0) {
        // Mute child process
        if (!freopen("/dev/null", "w", stdout))
            printf("   * could not mute child process\n");

        int lits1[3] = {1,2,3};
        int lits2[3] = {4,5,6};
        clause_ptr c1 = create_flat_clause(1,3,lits1);
        clause_ptr c2 = create_flat_clause(1,3,lits2);

        heap_insert(heap, c2);
        skip_heap_duplicates(c1, heap);     // expected to abort

        delete_flat_clause(c1);
        delete_flat_clause(c2);
        exit(0);
    } else
        wait(&status);
    do_assert(status != 0);
}

static void test_flush_heap_to_file() {
    check_simple_flush();
    check_merge_flush();
    check_doubling_id();
}

// ----- INIT -----

static void generate_random_clauses() {
    printf("   * generate clauses\n");

    // Unit clause is 8 Byte
    // => at most HEAP_CAPACITY / 8 clauses will be generated
    clauses = (clause_ptr*)trusted_utils_calloc(HEAP_CAPACITY / 8, sizeof(clause_ptr));
    NUM_CLAUSES = 0;
    u64 clause_mem = 0;
    while (true) {
        u64 id = (u64)random() + 1;
        unsigned int nb_lits = (unsigned int)((random() % MAX_NB_LITS) + 1);
        int* lits = trusted_utils_calloc(nb_lits, sizeof(int));
        lits[0] = (int)NUM_CLAUSES;    // esures differing clauses
        for (size_t j = 1; j < nb_lits; j++)
            lits[j] = random();
        clause_ptr c = create_flat_clause(id, nb_lits, lits);
        // enough clauses generated
        if (clause_mem + get_clause_size(c) > HEAP_CAPACITY) {
            //bonus_clause = c;
            break;
        }
        clauses[NUM_CLAUSES++] = c;
        clause_mem += get_clause_size(c);
    }

    printf("   * generated %u clauses, allocating %luB\n", NUM_CLAUSES, clause_mem);
}

static void init_tests() {
    srandom(time(NULL));
    printf("   * init heap\n");
    heap = heap_init(HEAP_CAPACITY);
    NUM_CLAUSES = 0;
    generate_random_clauses();
    printf("   * ensure empty test file directory\n");
    char cmd[512];
    snprintf(cmd, 512, "if [ -d \"%s\" ]; then rm -r %s; fi; mkdir %s; cd %s; mkdir 0;", TEST_DIR, TEST_DIR, TEST_DIR, TEST_DIR);
    do_assert(!system(cmd));
    lits_buffer = int_vec_init(1);
}

static void wrap_up_tests() {
    for (size_t i = 0; i < NUM_CLAUSES; i++)
        delete_flat_clause(clauses[i]);
    free(clauses);
    heap_free(heap);
    int_vec_free(lits_buffer);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("** no arguments given, default to HEAP_CAPACITY = %uB, MAX_NB_LITS=%u\n", DEFAULT_HEAP_CAPACITY, DEFAULT_MAX_NB_LITS);
        HEAP_CAPACITY = DEFAULT_HEAP_CAPACITY;
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 2) {
        printf("** set HEAP_CAPACITY = %sB, MAX_NB_LITS=%u\n", argv[1], DEFAULT_MAX_NB_LITS);
        HEAP_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 3) {
        printf("** set HEAP_CAPACITY = %sB, MAX_NB_LITS=%s\n", argv[1], argv[2]);
        HEAP_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = atoi(argv[2]);
    } else {
        printf("** can take at most two arguments\n");
        printf("** ABORT\n");
        abort();
    }

    printf("** initialize tests\n");
    init_tests();

    printf("** test skip_heap_duplication\n");
    test_skip_heap_duplicates();
    
    printf("** test flush_heap_to_file\n");
    test_flush_heap_to_file();

    printf("** wrap tests up\n");
    wrap_up_tests();

    printf("** DONE\n");
    return 0;
}
