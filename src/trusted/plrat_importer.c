
#include "plrat_importer.h"

#include <assert.h>
#include <math.h>      // for sqrt
#include <stdbool.h>   // for bool, true, false
#include <stdio.h>     // for fclose, fflush_unlocked, fopen, snprintf
#include <stdlib.h>    // for free
#include <unistd.h>
#include <sys/stat.h>  // for mkdir
#include <time.h>      // for clock, CLOCKS_PER_SEC, clock_t

#include "checker_interface.h"
#include "hash.h"
#include "plrat_checker.h"  // for trusted_utils_read_int, trusted_utils_log...
#include "plrat_utils.h"
#include "secret.h"
#include "commutative_sig.h"
#include "top_check.h"  // for top_check_commit_formula_sig, top_check_d...
#include "heap.h"
#include "merge_buffer.h"

#ifdef UNIT_TEST
#define unit_static
#else
#define unit_static static
#endif

const char* out_path;  // named pipe
u64 n_solvers;         // number of solvers
double root_n;         // square root of number of solvers
struct comm_sig** signatures;
size_t comm_size;
u64 redist_strat;  // redistribution_strategy
u64 local_rank;    // solver id

// Buffering.
struct clause_heap** clause_heaps;
unsigned long* max_ids;
FILE** out_files;
char** file_names;  // to be removed. uses for file shenanigans
u64* clause_counts; // count clauses contained in each file
struct merge_buffer* merge_buffer;

// TODO: gather stats

#ifdef UNIT_TEST
FILE* get_plrat_importer_out_file() {
    return out_files[0];
}

char* get_plrat_importer_out_file_name() {
    return file_names[0];
}

void set_plrat_importer_out_file(FILE* file) {
    out_files[0] = file;
}

void set_plrat_importer_max_id(unsigned long id) {
    max_ids[0] = id;
}
#endif

// TODO: Add dedicated unit tests
static u64 get_merge_file_pos(u64 clause_id, FILE* file) {
    rewind(file);
    trusted_utils_read_int(file);

    while (true) {
        u64 id = trusted_utils_read_ul(file);
        if (clause_id <= id)
            break;
        int nb_lits = trusted_utils_read_int(file);
        trusted_utils_skip_bytes(nb_lits * sizeof(int), file);
    }

    fseek(file, -sizeof(u64), SEEK_CUR);
    return ftell(file);
}

// TODO: Add dedicated unit tests
static void write_clause_to_buffered_file(clause_ptr clause, FILE* write_ptr, struct merge_buffer* buffer) {
    FILE* read_ptr = buffer->file;

    if (!buffer->eof) {
        if ((long)get_clause_size(clause) + ftell(write_ptr) < ftell(read_ptr))
            merge_buffer_fill(buffer);

        // couldn't make enough space for new clause
        if ((long)get_clause_size(clause) + ftell(write_ptr) < ftell(read_ptr)) {
            trusted_utils_log_err("Could not create enough space while merging clauses into .palrup_proxy file");
            exit(1);
        }
    }

    write_flat_clause_to_file(clause, write_ptr);
}

static void plrat_importer_write_hash(u8* hash, FILE* current_out) {
    trusted_utils_write_sig(hash, current_out);
}

static u64 plrat_importer_get_proxy_rank(size_t id) {
    u64 x = plrat_utils_rank_to_x(id % n_solvers, comm_size);
    u64 y = plrat_utils_rank_to_y(local_rank, comm_size);
    return plrat_utils_2d_to_rank(x, y, comm_size);
}

void plrat_importer_init(const char* main_path, unsigned long solver_id, unsigned long num_solvers, unsigned long redistribution_strategy, unsigned long write_buffer_size) {
    redist_strat = redistribution_strategy;
    n_solvers = num_solvers;
    root_n = sqrt((double)num_solvers);
    comm_size = (size_t)ceil(root_n);  // round to nearest integer
    if (redist_strat == 1) {
        comm_size = n_solvers;
    }
    out_path = main_path;
    local_rank = solver_id;
    clause_heaps = trusted_utils_malloc(sizeof(struct clause_heap*) * comm_size);
    max_ids = trusted_utils_calloc(comm_size, sizeof(unsigned long));
    out_files = trusted_utils_malloc(sizeof(FILE*) * comm_size);
    file_names = trusted_utils_malloc(sizeof(char*) * comm_size);
    clause_counts = trusted_utils_calloc(comm_size, sizeof(u64));
    signatures = trusted_utils_malloc(sizeof(struct comm_sig*) * comm_size);
    merge_buffer = merge_buffer_init(write_buffer_size, NULL);

    if (local_rank == 0) {
        char msg[512];
        snprintf(msg, 512, "comm_size: %ld", comm_size);
        plrat_utils_log(msg);
    }

    for (size_t i = 0; i < comm_size; i++) {
        char proof_folder[512];
        char ids_path[1024];
        u64 proxy_rank = plrat_importer_get_proxy_rank(i);
        if (redist_strat == 2) {
            snprintf(proof_folder, 512, "%s/%lu", out_path, proxy_rank);
        } else {
            snprintf(proof_folder, 512, "%s/%lu", out_path, i);
        }
        if (proxy_rank > num_solvers - 1) {
            mkdir(proof_folder, 0755);
        }

        if (redist_strat == 2) {
            snprintf(ids_path, 1024, "%s/%lu.palrup_proxy", proof_folder, plrat_utils_rank_to_x(local_rank, comm_size));
        } else {
            snprintf(ids_path, 1024, "%s/%lu.palrup_import", proof_folder, local_rank);
        }

        // plrat_utils_log(ids_path);
        out_files[i] = fopen(ids_path, "wb+");
        if (!(out_files[i])) {
            char msg[1048];
            snprintf(msg, 1048, "out_files not created: %s\n", ids_path);
            plrat_utils_log_err(msg);
        }
        trusted_utils_write_int(0, out_files[i]);   // placeholder to insert number of clauses in file
        file_names[i] = trusted_utils_calloc(1024, sizeof(char));
        memcpy(file_names[i], ids_path, 1024);

        clause_heaps[i] = heap_init(write_buffer_size);        
        signatures[i] = comm_sig_init(SECRET_KEY_2);
    }
}

unit_static void skip_heap_duplicates(clause_ptr c, struct clause_heap* heap) {
    if (heap->size <= 0)
        return;
    clause_ptr next_c = heap_get_min(heap);
    while (next_c != NULL && get_clause_id(c) == get_clause_id(next_c)) {
        if (!compare_flat_clause(c, next_c)) {
            trusted_utils_log_err("differing clauses with same id detected");
            // TODO: correctly mark proof as faulty
            exit(1);
        }
        
        delete_flat_clause(heap_pop_min(heap));
        if (heap->size <= 0)
            break;
        next_c = heap_get_min(heap);
    }
}

// flush_ratio \in [0,1] denotes the maximum fill level of the heap after the flush operation
unit_static void flush_heap_to_file(struct clause_heap* clause_heap, int file_id, float flush_ratio) {
    if (clause_heap->size <= 0)
        return;
    unsigned long* max_id = &(max_ids[file_id]);
    FILE* write_ptr = out_files[file_id];
    clause_ptr c = heap_get_min(clause_heap);

    if (*max_id < get_clause_id(c)) {   // simply appent heap to file
        while (clause_heap->size > clause_heap->capacity * flush_ratio) {
            if (clause_heap->size <= 0)
                break;
            c = heap_pop_min(clause_heap);
            skip_heap_duplicates(c, clause_heap);

            // write clause to file and remove from heap
            write_flat_clause_to_file(c, write_ptr);
            clause_counts[file_id]++;
            comm_sig_update_clause(signatures[file_id],
                                   get_clause_id(c),
                                   get_clause_lits(c),
                                   get_clause_nb_lits(c));
            *max_id = get_clause_id(c);
            delete_flat_clause(c);
        }
    } else {    // merge file with heap to assure sorted clauses in file 
        merge_buffer_open_file(merge_buffer, file_names[file_id]);
        merge_buffer_set_file_pointer(merge_buffer, get_merge_file_pos(get_clause_id(c), write_ptr));

        // merge file and heap
        clause_ptr min_clause, file_clause, heap_clause;
        file_clause = merge_buffer_next_clause(merge_buffer);
        if (clause_heap->size > (clause_heap->capacity * flush_ratio)) {
            heap_clause = heap_pop_min(clause_heap);
            skip_heap_duplicates(heap_clause, clause_heap);
        }
        bool add_sig;
        while (file_clause || clause_heap->size > (clause_heap->capacity * flush_ratio)) {
            add_sig = false;

            if (!file_clause || (heap_clause && get_clause_id(file_clause) > get_clause_id(heap_clause))) {
                min_clause = heap_clause;
                heap_clause = heap_pop_min(clause_heap);
                skip_heap_duplicates(heap_clause, clause_heap);
                add_sig = true;
            } else if (!heap_clause || get_clause_id(file_clause) < get_clause_id(heap_clause)) {
                min_clause = file_clause;
                file_clause = merge_buffer_next_clause(merge_buffer);
            } else if (compare_flat_clause(file_clause, heap_clause)) {
                min_clause = file_clause;
                file_clause = merge_buffer_next_clause(merge_buffer);
                delete_flat_clause(heap_clause);
                heap_clause = heap_pop_min(clause_heap);
                skip_heap_duplicates(heap_clause, clause_heap);
            } else {
                trusted_utils_log_err("differing clauses with same id detected");
                exit(1);
            }

            write_clause_to_buffered_file(min_clause, write_ptr, merge_buffer);
            if (add_sig) {
                clause_counts[file_id]++;
                comm_sig_update_clause(signatures[file_id],
                                       get_clause_id(min_clause),
                                       get_clause_lits(min_clause),
                                       get_clause_nb_lits(min_clause));
            }
            *max_id = get_clause_id(min_clause);
            delete_flat_clause(min_clause);
        }

        // write potentially remaining heap clause
        if (heap_clause) {
            write_clause_to_buffered_file(heap_clause, write_ptr, merge_buffer);
            clause_counts[file_id]++;
            comm_sig_update_clause(signatures[file_id],
                                   get_clause_id(heap_clause),
                                   get_clause_lits(heap_clause),
                                   get_clause_nb_lits(heap_clause));
            delete_flat_clause(heap_clause);
        }

        assert(merge_buffer->size > 0);
    }
}

void plrat_importer_end() {
    for (size_t i = 0; i < comm_size; i++) {
        flush_heap_to_file(clause_heaps[i], i, 0);
        assert(clause_heaps[i]->size == 0);
        u8* sig = comm_sig_digest(signatures[i]);
        plrat_importer_write_hash(sig, out_files[i]);
        // write clause count to beginning of file
        fseek(out_files[i], 0, SEEK_SET);
        trusted_utils_write_int(clause_counts[i], out_files[i]);
        comm_sig_free(signatures[i]);
        free(sig);
    }

    for (size_t i = 0; i < comm_size; i++) {
        heap_free(clause_heaps[i]);
        fsync(fileno(out_files[i]));
        fclose(out_files[i]);
        free(file_names[i]);
    }
    free(out_files);
    free(file_names);
    free(clause_counts);
    free(clause_heaps);
    free(max_ids);
    free(signatures);
    merge_buffer_free(merge_buffer);
}

void plrat_importer_log(unsigned long id, const int* literals, int nb_literals) {
    int file_id = plrat_utils_rank_to_x(id % n_solvers, comm_size);
    clause_ptr _clause = create_flat_clause(id, nb_literals, literals);
    struct clause_heap* clause_heap = clause_heaps[file_id];

    // write to file if capacity is reached
    if (heap_insert(clause_heap, _clause)) {
        flush_heap_to_file(clause_heap, file_id, 0.5);
        if (heap_insert(clause_heap, _clause)) {
            plrat_utils_log_err("Clause could not be inserted into heap.");
            exit(1);
        }
    }
}
