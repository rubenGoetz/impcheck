
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/trusted/trusted_utils.h"
#include "../src/trusted/clause_flat.h"

char* PATH_IN;
char* PATH_OUT;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("* Need arguments <path_in> and <path_out>. ABORT.\n");
        abort();
    }
    
    printf("* init\n");
    // INIT
    PATH_IN = argv[1];
    PATH_OUT = argv[2];

    clause_ptr* clauses = trusted_utils_calloc(10000, sizeof(clause_ptr));
    u8* sig = trusted_utils_calloc(16, sizeof(u8));

    FILE* in_file = fopen(PATH_IN, "rb");
    FILE* out_file = fopen(PATH_OUT, "w");
    if (!in_file) {
        printf("* Could not open file at %s", PATH_IN);
        abort();
    }
    if (!out_file) {
        printf("* Could not open file at %s", PATH_OUT);
        abort();
    }

    printf("* read in-file\n");
    unsigned int clause_count = parse_lrat_import(in_file, clauses, sig);
    printf("* found %u clauses\n", clause_count);

    printf("* write out-file\n");
    // write header to increase readability
    fprintf(out_file, "File contained %u Clauses. The signature was read as:\n", clause_count);
    for (size_t i = 0; i < 16; i++) {
        fprintf(out_file, "%c", (char)sig[i]);
    }
    fprintf(out_file, "\n--------------------\n\n");

    // write clauses as text
    for (size_t i = 0; i < clause_count; i++) {
        clause_ptr clause = clauses[i];
        fprintf(out_file, "[CLAUSE %lu]\n", i);
        fprintf(out_file, "  ID: %lu\n", get_clause_id(clause));
        fprintf(out_file, "  NB_LITS: %u\n", get_clause_nb_lits(clause));
        fprintf(out_file, "  LITS:");
        int* lits = get_clause_lits(clause);
        for (size_t j = 0; j < get_clause_nb_lits(clause); j++) {
            fprintf(out_file, " %i", lits[j]);
        }
        fprintf(out_file, "\n");
    }
    
    // CLEANUP
    fsync(fileno(in_file));
    fsync(fileno(out_file));
    fclose(in_file);
    fclose(out_file);

    for (size_t i = 0; i < clause_count; i++)
        delete_flat_clause(clauses[i]);
    free(clauses);
    free(sig);

    
    return 0;
}
