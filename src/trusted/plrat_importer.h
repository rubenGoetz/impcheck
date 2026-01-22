
#pragma once

#include <stdbool.h>

void plrat_importer_init(const char* main_path, unsigned long solver_id, unsigned long num_solvers, unsigned long redistribution_strategy, unsigned long write_buffer_size);
void plrat_importer_log(unsigned long id, const int* literals, int nb_literals);
void plrat_importer_end();

// make methods available to unit tests
#ifdef UNIT_TEST
#include "clause_flat.h"
#include "heap.h"
void skip_heap_duplicates(clause_ptr c, struct clause_heap* heap);
void flush_heap_to_file(struct clause_heap* clause_heap, int file_id, float flush_ratio);
FILE* get_plrat_importer_out_file();
char* get_plrat_importer_out_file_name();
void set_plrat_importer_out_file(FILE* file);
void set_plrat_importer_max_id(unsigned long id);
#endif
