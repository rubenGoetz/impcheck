

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/trusted/trusted_utils.h"
#include "../src/trusted/clause_flat.h"
#include "../src/trusted/plrat_file_reader.h"

// Instantiate int_vec
#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/trusted/vec.h"
#undef TYPED
#undef TYPE

// Instantiate u64_vec
#define TYPE u64
#define TYPED(THING) u64_##THING
#include "../src/trusted/vec.h"
#undef TYPED
#undef TYPE

char* PATH_IN;
char* PATH_OUT;

struct plrat_reader* reader;
struct int_vec* lits_buffer;
struct u64_vec* hints_buffer;

static inline void read_literals(int nb_lits) {
    int_vec_reserve(lits_buffer, nb_lits);
    plrat_reader_read_vbl_ints(lits_buffer->data, nb_lits, reader);
}

static inline void read_hints(int nb_hints) {
    u64_vec_reserve(hints_buffer, nb_hints);
    plrat_reader_read_vbl_uls(hints_buffer->data, nb_hints, reader);
}

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
    hints_buffer = u64_vec_init(1);

    reader = plrat_reader_init(1024 * 1024, input, 0);
    while (true) {
        char c = plrat_reader_read_vbl_char(reader);
        if (plrat_reader_eof_reached(reader))
            break;
        
        if (c == 'a') {
            // log
            // printf("a PRODUCE\n");

            // parse clause
            u64 id = (u64)plrat_reader_read_vbl_int(reader);
            fprintf(output, "%c %lu", c, id);
            while (true) {
                int lit = plrat_reader_read_vbl_int(reader);
                fprintf(output, " %i", lit);
                if (!lit) break;
            }

            // parse hints
            while (true) {
                int hint = plrat_reader_read_vbl_int(reader);
                fprintf(output, " %i", hint);
                if (!hint) break;
            }

            fprintf(output, "\n");

        } else if (c == 'i') {
            // log
            // printf("i IMPORT\n");

            // parse clause
            u64 id = (u64)plrat_reader_read_vbl_int(reader);
            fprintf(output, "%c %lu", c, id);
            while (true) {
                int lit = plrat_reader_read_vbl_int(reader);
                fprintf(output, " %i", lit);
                if (!lit) break;
            }

            fprintf(output, "\n");

        } else if (c == 'd') {
            // log
            // printf("d DELETE\n");

            // parse clause
            fprintf(output, "%c", c);
            while (true) {
                u64 hint = plrat_reader_read_vbl_int(reader);
                //nb_lits++;
                fprintf(output, " %lu", hint);
                if (!hint) break;
            }

            fprintf(output, "\n");

        } else if (c == 'T') {
            // log
            // printf("T TERMINATE\n");

            // TODO: copy hash into file header
            break;

        } else {
            char errlog[512];
            snprintf(errlog, 512, "Invalid directive! c: %d filesize:%lu", c, reader->total_bytes);
            trusted_utils_log_err(errlog);
            exit(1);
        }
    }

    plrat_reader_end(reader);
    fsync(fileno(output));
    fclose(output);
    exit(0);
}
