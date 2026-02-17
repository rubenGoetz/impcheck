
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../src/trusted/trusted_utils.h"
#include "../src/trusted/clause_flat.h"
#include "../src/trusted/commutative_sig.h"
#include "../src/trusted/int_vec.h"

char* PATH_IN;
struct int_vec* lits_buffer;

void do_assert(bool cond) {
#ifndef NDEBUG
    assert(cond);
#else
    if (!cond) {
        printf("Assertion failed! Compile with DEBUG mode for details.\n");
        abort();
    }
#endif
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("* Need argument <path_in>. ABORT.\n");
        exit(1);
    }
    
    printf("* init\n");
    // INIT
    PATH_IN = argv[1];

    FILE* file = fopen(PATH_IN, "rb");
    if (!file) {
        printf("* Could not open file. ABORT.\n");
        exit(1);
    }

    struct comm_sig* expected_sig = comm_sig_init(SECRET_KEY_2);
    u8 read_sig[16];
    lits_buffer = int_vec_init(1);
    
    int clause_count = trusted_utils_read_int(file);
    printf("* expected clause count: %i\n", clause_count);
    for (int i = 0; i < clause_count; i++) {
        u64 id = trusted_utils_read_ul(file);
        int nb_lits = trusted_utils_read_int(file);
        int_vec_resize(lits_buffer, nb_lits);
        trusted_utils_read_ints(lits_buffer->data, nb_lits, file);
        comm_sig_update_clause(expected_sig, id, lits_buffer->data, nb_lits);
    }
    printf("* read signature..\n");
    trusted_utils_read_sig(read_sig, file);
    u8* expected_sig_dig = comm_sig_digest(expected_sig);

    printf("* assert EOF\n");
    do_assert(fgetc(file) == EOF); 

    printf("* assert signatures\n");
    for (size_t i = 0; i < 16; i++) {
        do_assert(read_sig[i] == expected_sig_dig[i]);
    }
    
    printf("* written signature matches recalculated one :)\n");
    printf("* sig: %i, %i, %i, %i\n", (unsigned int)read_sig[0], (unsigned int)read_sig[4], (unsigned int)read_sig[8], (unsigned int)read_sig[12]);

    free(expected_sig_dig);
    comm_sig_free(expected_sig);
    fclose(file);
    
    return 0;
}
