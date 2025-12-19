
#include <stdlib.h>
#include <string.h>

#include "trusted_utils.h"
#include "clause_flat.h"

// ----- SET -----

static inline void set_id(clause_ptr ptr, id_type id) {
    *((id_type*)ptr) = id;
}

static inline void set_nb_lits(clause_ptr ptr, nb_lits_type nb_lits) {
    *((nb_lits_type*)((char*)ptr + sizeof(id_type))) = nb_lits;
}

static inline void set_lits(clause_ptr ptr, nb_lits_type nb_lits, const lit_type* lits) {
    lit_type* write_ptr = (lit_type*)((char*)ptr + sizeof(id_type) + sizeof(nb_lits_type));
    for (size_t i = 0; i < nb_lits; i++) {
        *write_ptr++ = lits[i];
    }   
}

// ----- GET -----

inline id_type get_clause_id(clause_ptr ptr) {
    return *((id_type*)ptr);
}

inline nb_lits_type get_clause_nb_lits(clause_ptr ptr) {
    return *((nb_lits_type*)((char*)ptr + sizeof(id_type)));
}

inline lit_type* get_clause_lits(clause_ptr ptr) {
    return (lit_type*)((char*)ptr + sizeof(id_type) + sizeof(nb_lits_type));
}

inline size_t get_clause_size(clause_ptr ptr) {
    return sizeof(id_type) + sizeof(nb_lits_type) + (get_clause_nb_lits(ptr) * sizeof(lit_type));
}

// ----- OBJECT CREATION / DELETION -----

clause_ptr create_flat_clause(id_type id, nb_lits_type nb_lits, const lit_type* lits) {
    clause_ptr ptr = trusted_utils_malloc(sizeof(id_type)
                                            + sizeof(nb_lits_type)
                                            + (nb_lits * sizeof(lit_type)));
    if (ptr == NULL) {
        trusted_utils_log_err("could not allocate enough memory for flat clause");
        return NULL;
    }

    set_id(ptr, id);
    set_nb_lits(ptr, nb_lits);

    if (lits)
        set_lits(ptr, nb_lits, lits);
    
    return ptr;
}

void delete_flat_clause(clause_ptr ptr) {
    free(ptr);
}

// ----- UTILS -----

/*
Reads the next flat clause from a file, copies it and returns a pointer to the copied clause in memory.
Returns NULL if points to EOF. The file signature is returned as a clause_ptr and can be detected by 
checking for EOF. (i.e. by recieving NULL whenn reading the next clause)
*/
clause_ptr read_next_flat_clause_from_file(FILE* file) {
    int c = fgetc(file);
    if (c == EOF)
        return NULL;
    ungetc(c, file);
    
    id_type id = trusted_utils_read_ul(file);
    nb_lits_type nb_lits = (nb_lits_type)trusted_utils_read_int(file);

    int first_lit = trusted_utils_read_int(file);
    c = fgetc(file);
    if (c == EOF) {   // signature detected
        clause_ptr clause = create_flat_clause(id, 1, &first_lit);
        set_nb_lits(clause, nb_lits);
        return clause;
    }
    ungetc(c, file);

    clause_ptr clause = create_flat_clause(id, nb_lits, NULL);
    
    *(get_clause_lits(clause)) = first_lit;
    trusted_utils_read_ints(get_clause_lits(clause) + 1, nb_lits - 1, file);
    
    return clause;
}

inline void write_flat_clause_to_file(clause_ptr clause, FILE* file) {
    trusted_utils_write_flat_clause(clause, get_clause_size(clause), file);
}

bool compare_flat_clause(clause_ptr c1, clause_ptr c2) {
    if (c1 == c2)
        return true;
    if (get_clause_id(c1) != get_clause_id(c2)
        || get_clause_nb_lits(c1) != get_clause_nb_lits(c2))
        return false;
    
    return !memcmp(c1, c2, sizeof(id_type) + sizeof(nb_lits_type) + (get_clause_nb_lits(c1) * sizeof(lit_type)));
}

unsigned int parse_lrat_import(FILE* file, clause_ptr* clauses, u8* sig) {
    if (feof(file))
        return 0;

    unsigned int clause_count = 0;
    clause_ptr clause = read_next_flat_clause_from_file(file);

    while (clause) {
        clauses[clause_count++] = clause;
        clause = read_next_flat_clause_from_file(file);
    }

    // Last clause contains the files signature. Copy and clean up clause
    clause = clauses[--clause_count];
    memcpy(sig, clause, 16);
    delete_flat_clause(clause);

    return clause_count;
}
