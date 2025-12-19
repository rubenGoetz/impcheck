
#ifndef CLAUSE_FLAT_H
#define CLAUSE_FLAT_H

#include "trusted_utils.h"

typedef void* clause_ptr;
typedef u64 id_type;
typedef unsigned int nb_lits_type;
typedef int lit_type;

/*
Creates a flat clause by allocating memory on the heap.
A flat clause consists of:
    - u64: id
    - u32: nb_lits
    - i32 * nb_lits: literals

If lits == NULL a flat clause will be created without explizit literals. 
In this case the necessary memory to include the literals later will still be allocated but NOT cleared.
*/
clause_ptr create_flat_clause(u64 id, unsigned int nb_lits, const int* lits);
void delete_flat_clause(clause_ptr ptr);

u64 get_clause_id(clause_ptr ptr);
unsigned int get_clause_nb_lits(clause_ptr ptr);
int* get_clause_lits(clause_ptr ptr);
size_t get_clause_size(clause_ptr ptr);

bool compare_flat_clause(clause_ptr c1, clause_ptr c2);

clause_ptr read_next_flat_clause_from_file(FILE* file);
void write_flat_clause_to_file(clause_ptr clause, FILE* file);
unsigned int parse_lrat_import(FILE* file, clause_ptr* clauses, u8* sig);

#endif
