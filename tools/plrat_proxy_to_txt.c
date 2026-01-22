
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/trusted/trusted_utils.h"
#include "../src/trusted/clause_flat.h"
#include "../src/trusted/int_vec.h"

char* PATH_IN;
char* PATH_OUT;

struct int_vec* lits_buffer;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("* Need arguments <path_in> and <path_out>. ABORT.\n");
        abort();
    }
    
    // INIT
    PATH_IN = argv[1];
    PATH_OUT = argv[2];

    // open files
    FILE* input = fopen(PATH_IN, "rb");
    FILE* output = fopen(PATH_OUT, "w");
    
    if (!input) {
        printf("* Could not open input file at %s. ABORT.\n", PATH_IN);
        abort();
    }

    if (!output) {
        printf("* Could not open output file at %s. ABORT.\n", PATH_OUT);
        abort();
    }

    lits_buffer = int_vec_init(1);
    u64 clause_count = trusted_utils_read_int(input);

    // write header (leave space for sig)
    fprintf(output, "File containes %lu Clauses. The signature was read as:\n", clause_count);
    long sig_offset = ftell(output);
    for (size_t i = 0; i < 16; i++) {
        fprintf(output, "%c", '0');
    }
    fprintf(output, "\n--------------------\n\n");

    // write clauses as they are read
    for (size_t i = 0; i < clause_count; i++) {
        // read clause
        u64 id = trusted_utils_read_ul(input);
        int nb_lits = trusted_utils_read_int(input);
        int_vec_resize(lits_buffer, nb_lits);
        trusted_utils_read_ints(lits_buffer->data, nb_lits, input);

        // write cluase
        fprintf(output, "[CLAUSE %lu]\n", i);
        fprintf(output, "  ID: %lu\n", id);
        fprintf(output, "  NB_LITS: %i\n", nb_lits);
        fprintf(output, "  LITS:");
        for (int j = 0; j < nb_lits; j++) {
            fprintf(output, " %i", lits_buffer->data[j]);
        }
        fprintf(output, "\n");
    }
    
    // write sig into spaceholder
    u8 sig[16];
    trusted_utils_read_sig(sig, input);
    fseek(output, sig_offset, SEEK_SET);
    trusted_utils_write_sig(sig, output);

    if (fgetc(input) != EOF) {
        printf("* Error while parsing, EOF was not reached. ABORT.");
        abort();
    }

    fsync(fileno(input));
    fsync(fileno(output));
    fclose(input);
    fclose(output);
    exit(0);
}
