
#ifndef HEAP_H
#define HEAP_H

#include "trusted_utils.h"
#include "clause_flat.h"

struct clause_heap {
    u64 capacity;
    u64 size;
    u64 element_count;
    clause_ptr* data;
};

struct clause_heap* heap_init(u64 capacity);
void heap_free(struct clause_heap* heap);
clause_ptr heap_get_min(struct clause_heap* heap);
clause_ptr heap_pop_min(struct clause_heap* heap);
int heap_insert(struct clause_heap* heap, clause_ptr ptr);

#endif
