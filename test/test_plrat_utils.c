
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test.h"
#include "../src/trusted/trusted_utils.h"
#include "../src/trusted/plrat_utils.h"

#define DEFAULT_ELEM_COUNT 1000

// ----- TEST -----

static void test_bin_search() {
    printf("   * init array\n");
    size_t elem_count = DEFAULT_ELEM_COUNT;
    //elem_count = (elem_count / 2) * 2;  // erase possible int div problems
    int* a = trusted_utils_calloc(elem_count, sizeof(int));
    for (size_t i = 0; i < elem_count; i++)
        a[i] = (i - (elem_count / 2)) * 3; // i+1 to leave some positive integers before sart

    printf("   * try to find every element in array\n");
    for (size_t i = 0; i < elem_count; i++)
        do_assert(bin_search(a, a[i], 0, elem_count));
    
    printf("   * try to find missing elements\n");
    do_assert(!bin_search(a, 2, 0, elem_count));
    do_assert(!bin_search(a, 7, 0, elem_count));
    do_assert(!bin_search(a, -2, 0, elem_count));
    do_assert(!bin_search(a, -7, 0, elem_count));
    do_assert(!bin_search(a, a[0] - 1, 0, elem_count));
    do_assert(!bin_search(a, a[elem_count - 1] + 1, 0, elem_count));

    printf("   * check variable start / end\n");
    int start = elem_count / 4, end = 3* (elem_count / 4);
    do_assert(bin_search(a, a[elem_count / 2], start, end));
    do_assert(!bin_search(a, a[0], start, end));
    do_assert(!bin_search(a, a[elem_count - 1], start, end));

    printf("   * free array\n");
    free(a);
}

static void test_plrat_utils_compare_semi_sorted_lits() {
    printf("   * create array of sorted lits\n");
    size_t elem_count = DEFAULT_ELEM_COUNT;
    int* a = trusted_utils_calloc(elem_count, sizeof(int));
    for (size_t i = 0; i < elem_count; i++)
        a[i] = i - (elem_count / 2);
    
    printf("   * creae shuffled copy of array\n");
    int* b = trusted_utils_calloc(elem_count, sizeof(int));
    memcpy(b, a, elem_count * sizeof(int));
    for (size_t i = elem_count - 1; i > 0; i--) {
        size_t j = (size_t) (drand48()*(i+1));
        int t = b[j];
        b[j] = b[i];
        b[i] = t;
    }

    printf("   * check comparison between arrays\n");
    do_assert(plrat_utils_compare_semi_sorted_lits(a, b, elem_count, elem_count));
    do_assert(!plrat_utils_compare_semi_sorted_lits(a, b, elem_count, elem_count - 1));
    b[(size_t)drand48()*elem_count] = elem_count;   // alter b
    do_assert(!plrat_utils_compare_semi_sorted_lits(a, b, elem_count, elem_count));

    printf("   * free arrays\n");
    free(a);
    free(b);
}

// ----- INIT -----

static void init_tests() {
    srand48(time(NULL));
}

static void wrap_up_tests() {

}

int main(int argc, char const *argv[])
{
    UNUSED(argc); UNUSED(argv);

    printf("** initialize tests\n");
    init_tests();
    
    printf("** test bin_search\n");
    test_bin_search();
    
    printf("** test plrat_utils_compare_semi_sorted_lits\n");
    test_plrat_utils_compare_semi_sorted_lits();

    printf("** wrap tests up\n");
    wrap_up_tests();

    printf("** DONE\n");
    return 0;
}
