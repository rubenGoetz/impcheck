
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "test.h"
#include "../src/trusted/trusted_utils.h"
#include "../src/trusted/clause_flat.h"

#define DEFAULT_N 1000
#define DEFAULT_MAX_NB_LITS 100
#define FILENAME "flat_clause_test_file"

// Number of clauses and max number of lits per clause used in tests
unsigned int N;
unsigned int MAX_NB_LITS;

u64* ids;
unsigned int* nb_lits;
int** lits;
clause_ptr* flat_clauses;
FILE* file;
char* sig;

// ----- UTIL -----

void print_flat_clause(clause_ptr clause_ptr) {
    printf("----- BEGIN CLAUSE -----\n");
    printf("ID: %lu\n", get_clause_id(clause_ptr));
    printf("NB_LITS: %u\n", get_clause_nb_lits(clause_ptr));
    printf("LITS:");
    for (size_t i = 0; i < get_clause_nb_lits(clause_ptr); i++)
        printf(" %i", get_clause_lits(clause_ptr)[i]);
    printf("\n");
    
    printf("----- END CLAUSE -----\n");
}

// ----- CHECKS -----

static void check_id(clause_ptr clause_ptr, u64 id) {
    do_assert(get_clause_id(clause_ptr) == id);
}

static void check_nb_lits(clause_ptr clause_ptr, unsigned int nb_lits) {
    do_assert(get_clause_nb_lits(clause_ptr) == nb_lits);
}

static void check_lits(clause_ptr clause_ptr, unsigned int nb_lits, int* lits) {
    int* clause_lits = get_clause_lits(clause_ptr);
    for (size_t i = 0; i < nb_lits; i++) {
        do_assert(clause_lits[i] == lits[i]);
    }
}

// ----- TEST -----

static void create_flat_clauses() {
    for (size_t i = 0; i < N; i++) {
        flat_clauses[i] = create_flat_clause(ids[i], nb_lits[i], lits[i]);
    }
}

static void check_flat_clauses() {
    printf("   * check ids\n");
    for (size_t i = 0; i < N; i++)
        check_id(flat_clauses[i], ids[i]);

    printf("   * check nb_lits\n");
    for (size_t i = 0; i < N; i++)
        check_nb_lits(flat_clauses[i], nb_lits[i]);

    printf("   * check lits\n");
    for (size_t i = 0; i < N; i++)
        check_lits(flat_clauses[i], nb_lits[i], lits[i]);
}

static void check_file_io() {
    printf("   * write clauses to file\n");
    for (size_t i = 0; i < N; i++) 
        write_flat_clause_to_file(flat_clauses[i], file);
    
    printf("   * write signature to file\n");
    fwrite(sig, sizeof(char), 16, file);
    
    printf("   * read clauses from file and check for validity\n");
    rewind(file);
    for (size_t i = 0; i < N; i++) {
        clause_ptr clause = read_next_flat_clause_from_file(file);
        check_id(clause, ids[i]);
        check_nb_lits(clause, nb_lits[i]);
        check_lits(clause, nb_lits[i], lits[i]);
        delete_flat_clause(clause);
    }
    
    printf("   * check signature handling\n");
    char* sig_clause = read_next_flat_clause_from_file(file);
    do_assert(fgetc(file) == EOF);
    do_assert(read_next_flat_clause_from_file(file) == NULL);
    do_assert(!memcmp(sig_clause, sig, 16));
}

static void check_parse_lrat_import() {
    printf("   * parse previously written file\n");
    clause_ptr parsed_clauses[N];
    u8 file_sig[16];
    rewind(file);
    parse_lrat_import(file, parsed_clauses, file_sig);

    printf("   * check parsed clauses and signature\n");
    for (size_t i = 0; i < N; i++) {
        clause_ptr clause = parsed_clauses[i];
        check_id(clause, ids[i]);
        check_nb_lits(clause, nb_lits[i]);
        check_lits(clause, nb_lits[i], lits[i]);
        delete_flat_clause(clause);
    }
    do_assert(!memcmp(file_sig, sig, 16));
}

// ----- INIT -----

static void generate_random_clauses(u64 n, unsigned int max_nb_lits) {
    printf("   * generate clauses\n");
    ids = (u64*)trusted_utils_calloc(n, sizeof(u64));
    nb_lits = (unsigned int*)trusted_utils_calloc(n, sizeof(unsigned int));
    lits = (int**)trusted_utils_calloc(n, sizeof(int*));

    for (u64 i = 0; i < n; i++) {
        ids[i] = (u64)random();
        nb_lits[i] = (unsigned int)((random() % max_nb_lits) + 1);
        lits[i] = trusted_utils_calloc(nb_lits[i], sizeof(int));
        for (size_t j = 0; j < nb_lits[i]; j++) {
            lits[i][j] = random();
        }
    }
}

static void init_tests() {
    srandom(time(NULL));
    generate_random_clauses(N, MAX_NB_LITS);
    flat_clauses = (clause_ptr*)trusted_utils_calloc(N, sizeof(clause_ptr));
    sig = "0123456789ABCDEF";
    printf("   * remove previous test file\n");
    remove(FILENAME);
    printf("   * create new test file\n");
    file = fopen(FILENAME, "wb+");
}

static void wrap_up_tests() {
    free(ids);
    free(nb_lits);
    for (size_t i = 0; i < N; i++) {
        free(lits[i]);
        delete_flat_clause(flat_clauses[i]);
    }
    free(lits);
    free(flat_clauses);

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("** no arguments given, default to N = %u, MAX_NB_LITS=%u\n", DEFAULT_N, DEFAULT_MAX_NB_LITS);
        N = DEFAULT_N;
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 2) {
        printf("** set N = %s, MAX_NB_LITS=%u\n", argv[1], DEFAULT_MAX_NB_LITS);
        N = atoi(argv[1]);
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 3) {
        printf("** set N = %s, MAX_NB_LITS=%s\n", argv[1], argv[2]);
        N = atoi(argv[1]);
        MAX_NB_LITS = atoi(argv[2]);
    } else {
        printf("** can only take two arguments\n");
        printf("** ABORT\n");
        abort();
    }

    printf("** initialize tests\n");
    init_tests();

    printf("** create flat_clauses\n");
    create_flat_clauses();

    printf("** check flat_clauses\n");
    check_flat_clauses();

    // print_flat_clause(flat_clauses[0]);

    printf("** check file io\n");
    check_file_io();

    printf("** check parse_lrat_import\n");
    check_parse_lrat_import();

    printf("** wrap tests up\n");
    wrap_up_tests();

    printf("** DONE\n");
    return 0;
}
