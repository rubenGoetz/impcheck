
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test.h"
#include "../src/trusted/heap.h"
#include "../src/trusted/clause_flat.h"

#define DEFAULT_HEAP_CAPACITY 1024 * 1024
#define DEFAULT_MAX_NB_LITS 100

// Maximum heap capacity in Byte
unsigned int HEAP_CAPACITY;
unsigned int MAX_NB_LITS;
unsigned int NUM_CLAUSES;

clause_ptr* clauses;
clause_ptr bonus_clause;
struct clause_heap* heap;

// ----- TEST -----

static void build_heap() {
    printf("   * insert clauses\n");
    u64 old_size = 0;
    u64 old_count = 0;
    u64 initial_data_size = heap->data_size;
    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        int res = heap_insert(heap, clauses[i]);
        do_assert(!res);
        do_assert(old_size + get_clause_size(clauses[i]) == heap->size);
        do_assert(old_count + 1 == heap->element_count);
        old_size = heap->size;
        old_count = heap->element_count;
    }
    do_assert(heap_insert(heap, bonus_clause));
    do_assert(heap->size == old_size);
    do_assert(heap->element_count == old_count);

    // assert that heap->data has grown succesfully
    do_assert(initial_data_size < heap->data_size);
}

static void check_size() {
    printf("   * check heap size\n");
    do_assert(heap->element_count == NUM_CLAUSES);
    do_assert(heap->size <= HEAP_CAPACITY);
}

static void check_element_count() {
    printf("   * check element count\n");
    do_assert(heap->element_count == NUM_CLAUSES);
    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        do_assert(heap->data[i] != NULL);
    }
}

static void check_heap_invariant() {
    printf("   * check heap invariant\n");
    for (u64 i = 0; i < heap->element_count; i++)
    {
        u64 left_child_idx = (i * 2) + 1;
        u64 right_child_idx = (i * 2) + 2;

        clause_ptr clause = heap->data[i];

        if (left_child_idx < heap->element_count) {
            clause_ptr left_child = heap->data[left_child_idx];
            do_assert(get_clause_id(clause) <= get_clause_id(left_child));
        }

        if (right_child_idx < heap->element_count) {
            clause_ptr right_child = heap->data[right_child_idx];
            do_assert(get_clause_id(clause) <= get_clause_id(right_child));
        }
    }
}

static void check_heap_min() {
    printf("   * check heap min\n");
    do_assert(compare_flat_clause(heap->data[0], heap_get_min(heap)));
}

static void check_flush() {
    printf("   * check flush heap\n");
    clause_ptr* sorted_clauses = trusted_utils_calloc(NUM_CLAUSES, sizeof(clause_ptr));

    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        u64 old_count = heap->element_count;
        clause_ptr clause = heap_pop_min(heap);
        
        do_assert(heap->element_count == old_count - 1);
        if (heap->size > 0)
            do_assert(!compare_flat_clause(clause, heap_get_min(heap)));

        sorted_clauses[i] = clause;
    }
    do_assert(heap->size == 0);
    do_assert(heap->element_count == 0);

    for (u64 i = 1; i < NUM_CLAUSES; i++)
        do_assert(get_clause_id(sorted_clauses[i]) >= get_clause_id(sorted_clauses[i - 1]));

    free(sorted_clauses);
}

// ----- INIT -----

static void generate_random_clauses() {
    printf("   * generate clauses\n");

    // Unit clause is 8 Byte
    // => at most HEAP_CAPACITY / 8 clauses will be generated
    clauses = (clause_ptr*)trusted_utils_calloc(HEAP_CAPACITY / 8, sizeof(clause_ptr));
    NUM_CLAUSES = 0;
    u64 clause_mem = 0;
    while (true) {
        u64 id = (u64)random();
        unsigned int nb_lits = (unsigned int)((random() % MAX_NB_LITS) + 1);
        int* lits = trusted_utils_calloc(nb_lits, sizeof(int));
        lits[0] = (int)NUM_CLAUSES;    // esures differing clauses
        for (size_t j = 1; j < nb_lits; j++)
            lits[j] = random();
        clause_ptr c = create_flat_clause(id, nb_lits, lits);
        free(lits);
        // enough clauses generated
        if (clause_mem + get_clause_size(c) > HEAP_CAPACITY) {
            bonus_clause = c;
            break;
        }
        clauses[NUM_CLAUSES++] = c;
        clause_mem += get_clause_size(c);
    }

    printf("   * generated %u clauses, allocating %luB\n", NUM_CLAUSES, clause_mem);
}

static void init_tests() {
    srandom(time(NULL));
    printf("   * init heap\n");
    heap = heap_init(HEAP_CAPACITY);
    NUM_CLAUSES = 0;
    generate_random_clauses();
}

static void wrap_up_tests() {
    for (size_t i = 0; i < NUM_CLAUSES; i++)
        delete_flat_clause(clauses[i]);
    delete_flat_clause(bonus_clause);
    free(clauses);
    heap_free(heap);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("** no arguments given, default to HEAP_CAPACITY = %uB, MAX_NB_LITS=%u\n", DEFAULT_HEAP_CAPACITY, DEFAULT_MAX_NB_LITS);
        HEAP_CAPACITY = DEFAULT_HEAP_CAPACITY;
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 2) {
        printf("** set HEAP_CAPACITY = %sB, MAX_NB_LITS=%u\n", argv[1], DEFAULT_MAX_NB_LITS);
        HEAP_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 3) {
        printf("** set HEAP_CAPACITY = %sB, MAX_NB_LITS=%s\n", argv[1], argv[2]);
        HEAP_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = atoi(argv[2]);
    } else {
        printf("** can take at most two arguments\n");
        printf("** ABORT\n");
        abort();
    }

    printf("** initialize tests\n");
    init_tests();

    printf("** build heap\n");
    build_heap();
    
    printf("** check initial heap\n");
    check_size();
    check_element_count();
    check_heap_invariant();

    printf("** check heap behaviour\n");
    check_heap_min();
    check_flush();

    printf("** wrap tests up\n");
    wrap_up_tests();

    printf("** DONE\n");
    return 0;
}
