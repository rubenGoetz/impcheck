
#include "plrat_checker.h"  // for plrat_reader_read_int, trusted_utils_log...

#include <assert.h>
#include <math.h>
#include <stdbool.h>  // for bool, true, false
#include <stdio.h>    // for fclose, fflush_unlocked, fopen, snprintf
#include <stdlib.h>   // for free
#include <time.h>     // for clock, CLOCKS_PER_SEC, clock_t
#include <unistd.h>    // for access

#include "checker_interface.h"
#include "hash.h"
#include "plrat_file_reader.h"
#include "plrat_importer.h"
#include "plrat_utils.h"
#include "secret.h"
#include "siphash_cls.h"
#include "top_check.h"  // for top_check_commit_formula_sig, top_check_d...

#define UNUSED(x) (void)(x)

// Instantiate int_vec
#define TYPE int
#define TYPED(THING) int_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

// Instantiate u64_vec
#define TYPE u64
#define TYPED(THING) u64_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct plrat_reader* proof;  // named pipe
struct siphash* clause_hash;

int nb_vars;               // # variables in formula
long nb_clauses;           // # clauses in formula
signature formula_sig;     // formula signature
u64 nb_solvers;            // number of solvers
u64 solver_rank;           // solver id
u64 redist;                // redistribution_strategy
u64 pc_nb_loaded_clauses;  // number of loaded clauses
char proof_path_in[512];
char redestribute_path_out[512];

bool do_logging = true;
bool palrup_binary;

// Buffering.
signature buf_sig;
struct int_vec* buf_lits;
struct u64_vec* buf_hints;

void read_literals(int nb_lits) {
    int_vec_reserve(buf_lits, nb_lits);
    plrat_reader_read_ints(buf_lits->data, nb_lits, proof);
}

void read_hints(int nb_hints) {
    u64_vec_reserve(buf_hints, nb_hints);
    plrat_reader_read_uls(buf_hints->data, nb_hints, proof);
}

void skip_proof_header() {
    if (palrup_binary)
        return;

    char c = '\0';

    c = plrat_reader_read_char(proof);
    if (c == TRUSTED_CHK_INIT) {
        plrat_reader_skip_bytes(sizeof(int), proof);
    }

    c = plrat_reader_read_char(proof);
    while (c == TRUSTED_CHK_LOAD) {
        const int nb_lits = plrat_reader_read_int(proof);
        plrat_reader_skip_bytes(nb_lits * sizeof(int), proof);
        c = plrat_reader_read_char(proof);
    }

    if (c == TRUSTED_CHK_END_LOAD && solver_rank == 0) {
        plrat_utils_log("Header Skipped");
    }
    if (c == TRUSTED_CHK_TERMINATE) {
        plrat_utils_log("empty file");
        long loaded_end = proof->total_bytes - proof->remaining_bytes;
        long loaded_start = loaded_end - proof->actual_buffer_size;
        long current_pos = (proof->pos - proof->read_buffer) + loaded_start;
        plrat_reader_seek(current_pos - 1, proof);
    }
}

bool pc_load_from_file(FILE* formular) {
    int nb_vars;
    long nClauses;

    char buffer[1024];
    bool foundPcnf = false;
    int tmp = 0;

    while (fgets(buffer, sizeof(buffer), formular)) {
        if (buffer[0] == 'c') continue;  // Skip comments

        // Check for the line starting with "p cnf"
        tmp = sscanf(buffer, "p cnf %i %li \n", &nb_vars, &nClauses);
        if (tmp == 2) {
            foundPcnf = true;
            break;
        }
    }
    nb_clauses = nClauses;

    if (!foundPcnf) {
        plrat_utils_log_err("Error: 'p cnf' line not found in the formula file");
        return false;
    }

    if (solver_rank == 0) {
        char msg[512];
        snprintf(msg, 512, "Start reading the formula file: cnf %i %li:", nb_vars, nClauses);
        plrat_utils_log(msg);
    }

    top_check_init(nb_vars, false, false);
    bool no_error = true;
    while (true) {
        int lit;
        tmp = fscanf(formular, " %i ", &lit);

        if (tmp == EOF) break;

        top_check_load(lit);
        // printf("lit: %i\n", lit);
    }

    top_check_end_load();
    pc_nb_loaded_clauses = top_check_get_nb_loaded_clauses();

    if (solver_rank == 0) {
        char log_str[512];
        snprintf(log_str, 512, "Formular Loaded nb_clauses:%lu", pc_nb_loaded_clauses);
        plrat_utils_log(log_str);
    }

    return no_error;
}

void parse(u64* nb_produced, u64* nb_imported, u64* nb_deleted) {
    u64 max_derived_id = 0;
    while (true) {
        char c = plrat_reader_read_vbl_char(proof);
        if (plrat_reader_eof_reached(proof)) {
            u8* sig = siphash_cls_digest(clause_hash);

            // write in new file for stage 2
            char finger_print_path[517];
            snprintf(finger_print_path, 517, "%s.hash", proof_path_in);
            FILE* finger_print = fopen(finger_print_path, "wb");
            if (!finger_print) {
                char msg[1024];
                snprintf(msg, 1024, "Can't open file %s", finger_print_path);
                trusted_utils_log_err(msg);
            }
                
            trusted_utils_write_sig(sig, finger_print);
            //fsync(fileno(finger_print));
            fclose(finger_print);

            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            int_vec_resize(buf_lits, 0);
            u64_vec_resize(buf_hints, 0);
            
            u64 id = (u64)plrat_reader_read_vbl_sl(proof);
            siphash_cls_update(clause_hash, (u8*)&id, sizeof(u64));

            // Starting point of assigned ids
            if (id <= (u64)nb_clauses) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has ID lower that original formula. ID:%lu, solver_rank:%lu, nb_solvers:%lu", id, solver_rank, nb_solvers);
                plrat_utils_log_err(msg);
                exit(1);
            }

            // locality of assigned IDs
            if (id % nb_solvers != solver_rank) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has non local ID. ID:%lu, solver_rank:%lu, nb_solvers:%lu", id, solver_rank, nb_solvers);
                plrat_utils_log_err(msg);
                exit(1);
            }

            // Monotonicity of assigned IDs
            if (id < max_derived_id) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has lower ID that previously learned clause. newID:%lu, prevID:%lu", id, max_derived_id);
                plrat_utils_log_err(msg);
                exit(1);
            }
            max_derived_id = id;

            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = plrat_reader_read_vbl_int(proof);
                if (!lit) break;
                int_vec_push(buf_lits, lit);
                nb_lits++;
            }
            siphash_cls_update(clause_hash, (u8*)buf_lits->data, nb_lits * sizeof(int));

            // parse hints
            int nb_hints = 0;
            while (true) {
                u64 hint = (u64)plrat_reader_read_vbl_sl(proof);
                if (!hint) break;
                u64_vec_push(buf_hints, hint);
                nb_hints++;
            }

            //check IDs in hints
            if (!plrat_utils_check_hints(id, buf_hints->data, nb_hints)) {
                char msg[523];
                snprintf(msg, 512, "Discoverd hint >= id in produced clause. ID:%lu", id);
                plrat_utils_log_err(msg);
                exit(1);
            }

            // forward to checker
            top_check_produce(id, buf_lits->data, nb_lits,
                              buf_hints->data, nb_hints);
            *nb_produced += 1;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            int_vec_resize(buf_lits, 0);

            u64 id = (u64)plrat_reader_read_vbl_sl(proof);

            // Check ID against original formula
            if (id <= (u64)nb_clauses) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has ID lower that original formula. ID:%lu, solver_rank:%lu, nb_solvers:%lu", id, solver_rank, nb_solvers);
                plrat_utils_log_err(msg);
                exit(1);
            }

            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = plrat_reader_read_vbl_int(proof);
                if (!lit) break;
                int_vec_push(buf_lits, lit);
                nb_lits++;
            }

            // forward to checker
            plrat_utils_import_unchecked(id, buf_lits->data, nb_lits);
            *nb_imported += 1;

            // write in file for stage 2
            plrat_importer_log(id, buf_lits->data, nb_lits);

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            u64_vec_resize(buf_hints, 0);

            // parse hints
            int nb_hints = 0;
            while (true) {
                u64 hint = (u64)plrat_reader_read_vbl_sl(proof);
                if (!hint) break;
                u64_vec_push(buf_hints, hint);
                nb_hints++;
            }

            top_check_delete(buf_hints->data, nb_hints);
            *nb_deleted += nb_hints;

        } else {
            char errlog[512];
            snprintf(errlog, 512, "Invalid directive! c: %d filesize:%lu", c, proof->total_bytes);
            trusted_utils_log_err(errlog);
            exit(1);
        }

        if (MALLOB_UNLIKELY(!top_check_valid())) {    
            trusted_utils_log_err(trusted_utils_msgstr);
            exit(1);
        }
    }
}

void parse_legacy(u64* nb_produced, u64* nb_imported, u64* nb_deleted) {
    while (true) {
        int c = plrat_reader_read_char(proof);
        if (c == TRUSTED_CHK_CLS_PRODUCE) {
            // parse
            u64 id = plrat_reader_read_ul(proof);
            siphash_cls_update(clause_hash, (u8*)&id, sizeof(u64));
            // printf("produce %lu\n", id);
            const int nb_lits = plrat_reader_read_int(proof);
            // printf("nb lits %d\n", nb_lits);
            read_literals(nb_lits);
            const int nb_hints = plrat_reader_read_int(proof);
            // printf("nb hints %d\n", nb_hints);
            read_hints(nb_hints);
            // check monotonous ID
            if (!plrat_utils_check_hints(id, buf_hints->data, nb_hints)) {
                char msg[523];
                snprintf(msg, 512, "Discoverd hint >= id in produced clause. ID:%lu", id);
                plrat_utils_log_err(msg);
                exit(1);
            }
            // forward to checker
            top_check_produce(id, buf_lits->data, nb_lits,
                              buf_hints->data, nb_hints);
            *nb_produced += 1;
            siphash_cls_update(clause_hash, (u8*)buf_lits->data, nb_lits * sizeof(int));

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            // parse
            const u64 id = plrat_reader_read_ul(proof);
            const int nb_lits = plrat_reader_read_int(proof);
            read_literals(nb_lits);
            // forward to checker
            plrat_utils_import_unchecked(id, buf_lits->data, nb_lits);
            *nb_imported += 1;

            // write in file for stage 2
            plrat_importer_log(id, buf_lits->data, nb_lits);

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            // parse
            const int nb_hints = plrat_reader_read_int(proof);
            read_hints(nb_hints);
            // forward to checker
            top_check_delete(buf_hints->data, nb_hints);
            *nb_deleted += nb_hints;

        } else if (c == TRUSTED_CHK_TERMINATE) {
            u8* sig = siphash_cls_digest(clause_hash);
            // write in file for stage 2
            long left_bytes = proof->end - proof->pos;
            if (left_bytes > 0) fseek(proof->buffered_file, -left_bytes, SEEK_CUR);
            
            trusted_utils_write_sig(sig, proof->buffered_file);
            break;

        } else {
            char errlog[512];
            snprintf(errlog, 512, "Invalid directive! rank: %lu c: %d filesize:%lu", solver_rank, c, proof->total_bytes);
            trusted_utils_log_err(errlog);
            exit(1);
        }

        if (MALLOB_UNLIKELY(!top_check_valid())) {    
            trusted_utils_log_err(trusted_utils_msgstr);
            exit(1);
        }
    }
}

void pc_init(const char* formula_path, const char* proofs_path_in, const char* proofs_path_out, unsigned long solver_id, unsigned long num_solvers, unsigned long redistribution_strategy, unsigned long read_buffer_size, bool use_palrup_binary) {
    FILE* formular;
    palrup_binary = use_palrup_binary;
    clause_hash = siphash_cls_init(SECRET_KEY);
    double root_n = sqrt((double)num_solvers);
    size_t comm_size = (size_t)ceil(root_n);  // round to nearest integer
    if (redistribution_strategy == 1) {
        comm_size = num_solvers;
    }
    unsigned int dir_hierarchy = solver_id / comm_size;
    snprintf(proof_path_in, 512, "%s/%u/%lu/out.palrup", proofs_path_in, dir_hierarchy, solver_id);
    snprintf(redestribute_path_out, 512, "%s", proofs_path_out);

    if (access(proof_path_in, F_OK) != 0) {
        char log_str[1024];
        snprintf(log_str, 1024, "proof_path_in does not exist. Creating it: %s", proof_path_in);
        plrat_utils_log(log_str);
        // file doesn't exist
        // create placeholder file containing only 0
        FILE* f = fopen(proof_path_in, "wb");
        //trusted_utils_write_char(1, f);
        //trusted_utils_write_char(2, f);
        //trusted_utils_write_char(TRUSTED_CHK_TERMINATE, f);  // write placeholder
        fclose(f);
    } 

    FILE* proof_stream = fopen(proof_path_in, "rb+");
    if (!proof_stream) trusted_utils_log_err("proof_path_in could not be opened");
    proof = plrat_reader_init(read_buffer_size, proof_stream, solver_id);


    formular = fopen(formula_path, "rb");
    if (!formular) trusted_utils_log_err("formula_path could not be opened");
    UNUSED(formula_path);
    buf_lits = int_vec_init(1 << 14);
    buf_hints = u64_vec_init(1 << 14);
    nb_solvers = num_solvers;
    solver_rank = solver_id;
    plrat_importer_init(proofs_path_out, solver_id, num_solvers, redistribution_strategy, read_buffer_size);
    if (!pc_load_from_file(formular)) {  //! pc_load() ||
        exit(0);
    }
    fclose(formular);
    skip_proof_header();
}

void pc_end() {
    plrat_importer_end();
    int_vec_free(buf_lits);
    u64_vec_free(buf_hints);
    plrat_reader_end(proof);
    top_check_end();
    siphash_cls_free(clause_hash);
}

int pc_run() {
    clock_t start = clock();
    u64 nb_produced = 0, nb_imported = 0, nb_deleted = 0;

    if (palrup_binary)
        parse(&nb_produced, &nb_imported, &nb_deleted);
    else
        parse_legacy(&nb_produced, &nb_imported, &nb_deleted);

    float elapsed = (float)(clock() - start) / CLOCKS_PER_SEC;

    if (top_check_validate_unsat(NULL)) {
        char unsat_folder[525];
        snprintf(unsat_folder, 525, "%s/.unsat_found", redestribute_path_out);
        if (mkdir(unsat_folder, 0777) == 0) {
            char unsat_folder_sub[545];
            snprintf(unsat_folder_sub, 1024, "%s/%lu", unsat_folder, solver_rank);
            mkdir(unsat_folder_sub, 0777);
        }
    }
    snprintf(trusted_utils_msgstr, 512, "rank: %lu cpu:%.3f prod:%lu imp:%lu del:%lu n_s:%lu", solver_rank, elapsed, nb_produced, nb_imported, nb_deleted, nb_solvers);
    trusted_utils_log(trusted_utils_msgstr);

    return 0;
}