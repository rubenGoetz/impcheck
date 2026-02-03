
#include <stdlib.h>
#include <assert.h>

//#include "trusted_utils.h"
//#include "clause_flat.h"
#include "heap.h"

// ----- PRIVATE ----- //

#define MIN(X,Y) (X < Y ? X : Y)
#define MAX(X,Y) (X > Y ? X : Y)

// returns index of left child only
static u64 get_child_idx(u64 idx) {
    u64 child_idx = (idx * 2) + 1;
    assert(child_idx > idx);
    return child_idx;
}

static u64 get_parent_idx(u64 idx) {
    u64 parent_idx = (idx - 1) / 2;
    assert(parent_idx < idx);
    return parent_idx;
}

static void resize_data(struct clause_heap* heap) {
    // Unit clause has size sizeof(id + nb_lits + lit) = 4B + 2B + 2B = 8B
    // => heap can contain at most capacity / 8 Byte clauses
    u64 max_size = heap->capacity / 8;
    u64 new_data_size = heap->data_size * 1.5;
    if (new_data_size == heap->data_size)
        new_data_size = new_data_size + 1;
    heap->data_size = MIN(new_data_size, max_size);
    heap->data = (clause_ptr*)trusted_utils_realloc(heap->data, sizeof(clause_ptr) * heap->data_size);
}

// ----- PUBLIC ----- //

struct clause_heap* heap_init(u64 capacity) {
    struct clause_heap* heap = trusted_utils_malloc(sizeof(struct clause_heap));
    // used Memory in bytes
    heap->size = 0;
    // maximum capacity in bytes
    heap->capacity = capacity;
    // count of elements in heap
    heap->element_count = 0;
    // start with 1/100 of possible clause count in heap
    heap->data_size = MAX(capacity / 800, 1);
    heap->data = (clause_ptr*)trusted_utils_calloc(heap->data_size, sizeof(clause_ptr));
    return heap;
}

void heap_free(struct clause_heap* heap) {
    while (heap->size > 0)
        delete_flat_clause(heap_pop_min(heap));
    free(heap->data);
    free(heap);
}

clause_ptr heap_get_min(struct clause_heap* heap) {
    if (heap->element_count <= 0)
        return NULL;
    return heap->data[0];
}

clause_ptr heap_pop_min(struct clause_heap* heap) {
    if (heap->size <= 0)
        return NULL;
    clause_ptr* data = heap->data;
    clause_ptr min_element = heap_get_min(heap);

    heap->element_count--;
    assert(heap->element_count < (u64)0 - 1);
    data[0] = data[heap->element_count];

    // sift down
    u64 idx = 0;
    clause_ptr elem = data[0];
    u64 child_idx;
    clause_ptr child;
    while(idx < heap->element_count) {
        // get index of child with lower id
        child_idx = get_child_idx(idx);
        
        if (child_idx >= heap->element_count)        // no children remaining
            break;

        clause_ptr left_child = data[child_idx];
        clause_ptr right_child = data[child_idx + 1];
        child = left_child;

        if (child_idx + 1 >= heap->element_count);               // only left child exists
        else if (get_clause_id(right_child) < get_clause_id(left_child)) {      // right child has smaller id
            child = right_child;
            child_idx++;
        }

        if (get_clause_id(elem) <= get_clause_id(child))       // heap invariant is restored
            break;

        // swap
        data[idx] = child;

        idx = child_idx;
    }
    data[idx] = elem;

    heap->size -= get_clause_size(min_element);
    return min_element;
}

/*
Inserts a clause into the heap if capacity allows it. 
Return 0 if successfull and 1 if clause could not be inserted.
*/
int heap_insert(struct clause_heap* heap, clause_ptr clause) {
    if (heap->size + get_clause_size(clause) > heap->capacity)
        return 1;
    if (heap->element_count + 1 >= heap->data_size)
        resize_data(heap);
    clause_ptr* data = heap->data;

    data[heap->element_count] = clause;
    u64 idx = heap->element_count;
    heap->element_count++;
    assert(heap->element_count > 0);

    // sift-up
    u64 parent_idx = 0;
    clause_ptr parent;
    while(idx > 0) {
        parent_idx = get_parent_idx(idx);
        parent = data[parent_idx];
        if (get_clause_id(parent) < get_clause_id(clause))        // heap invariant is restored
            break;
        
        // swap
        data[idx] = parent;

        idx = parent_idx;
    }
    data[idx] = clause;

    heap->size += get_clause_size(clause);
    return 0;
}
