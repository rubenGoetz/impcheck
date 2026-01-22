
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test.h"
#include "../src/trusted/plrat_file_reader.h"

#define TEST_DIR "plrat_file_reader_test_dir"

// ----- UTIL -----

#define CONCAT(A,B) A##B

#define B(S) binary_to_char(#S)

static inline char binary_to_char(const char *s) {
        char c = 0;
        while (*s) {
                c <<= 1;
                c += *s++ - '0';
        }
        return c;
}

// creates a file at path containing len bytes from data
static FILE* create_file(const char* name, size_t len, const void* data) {
    char path[512];
    snprintf(path, 512, "%s/%s", TEST_DIR, name);

    printf("   * remove file %s\n", path);
    remove(path);
    printf("   * create new file %s\n", path);
    FILE* file = fopen(path, "wb+");

    printf("   * write data to file\n");
    size_t res = fwrite(data, 1, len, file);
    if (res != len)
        exit(1);

    printf("   * rewind file\n");
    rewind(file);

    return file;
}

// ----- TEST -----

// TODO: implement missing missing tests

static void test_vbl_int() {
    // {0, 1, -1, 2^31 - 1, -(2^31 - 1 )}
    int ints[] = {0, -0, 1, -1, 2147483647, -2147483647};
    
    // ints encoded into 14B vbl
    char data[] = {B(00000000),     // 0
                   B(00000001),     // -0?
                   B(00000010),     // 1
                   B(00000011),     // -1
                   B(11111110),     // 2^31 - 1 = 2147483647
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(00001111),
                   B(11111111),     // -(2^31 - 1) = -2147483647
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(00001111)};

    FILE* file = create_file("vbl_int", 14, data);
    
    printf("   * init plrat reader\n");
    struct plrat_reader* reader = plrat_reader_init(4, file, 0);

    printf("   * read data from file\n");
    int read_ints[6];
    for (size_t i = 0; i < 6; i++)
        read_ints[i] = plrat_reader_read_vbl_int(reader);
    
    printf("   * check read data\n");
    for (size_t i = 0; i < 6; i++)
        do_assert(ints[i] == read_ints[i]);

    plrat_reader_end(reader);
}

static void test_vbl_ul() {
    // {0, 1, 2^64 - 1}
    u64 uls[] = {0, 1, (u64)-1};
    
    // ints encoded into 12B vbl
    char data[] = {B(00000000),     // 0
                   B(00000001),     // 1
                   B(11111111),     // 2^64 - 1 = 2147483647
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(00000001)};

    FILE* file = create_file("vbl_ul", 12, data);
    
    printf("   * init plrat reader\n");
    struct plrat_reader* reader = plrat_reader_init(2, file, 1);

    printf("   * read data from file\n");
    u64 read_uls[5];
    for (size_t i = 0; i < 3; i++)
        read_uls[i] = plrat_reader_read_vbl_ul(reader);
    
    printf("   * check read data\n");
    for (size_t i = 0; i < 3; i++)
        do_assert(uls[i] == read_uls[i]);

    plrat_reader_end(reader);
}

static void test_vbl_ints() {
    // {0, 1, -1, 2^31 - 1, -(2^31 - 1 )}
    int ints[] = {0, -0, 1, -1, 2147483647, -2147483647, 42};
    
    // ints encoded into 15B vbl
    char data[] = {B(00000000),     // 0
                   B(00000001),     // -0?
                   B(00000010),     // 1
                   B(00000011),     // -1
                   B(11111110),     // 2^31 - 1 = 2147483647
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(00001111),
                   B(11111111),     // -(2^31 - 1) = -2147483647
                   B(11111111),
                   B(11111111),
                   B(11111111),
                   B(00001111),
                   B(01010100)};    // 42

    FILE* file = create_file("vbl_ints", 15, data);
    
    printf("   * init plrat reader\n");
    struct plrat_reader* reader = plrat_reader_init(4, file, 2);

    printf("   * read data from file\n");
    int read_ints[7];
    plrat_reader_read_vbl_ints(&(read_ints[0]), 1, reader);
    plrat_reader_read_vbl_ints(&(read_ints[1]), 2, reader);
    plrat_reader_read_vbl_ints(&(read_ints[3]), 3, reader);
    plrat_reader_read_vbl_ints(&(read_ints[6]), 1, reader);
    
    printf("   * check read data\n");
    for (size_t i = 0; i < 7; i++)
        do_assert(ints[i] == read_ints[i]);

    plrat_reader_end(reader);
}

// ----- INIT -----

static void init_tests() {
    srandom(time(NULL));
    printf("   * ensure empty test file directory\n");
    char cmd[512];
    snprintf(cmd, 512, "if [ -d \"%s\" ]; then rm -r %s; fi; mkdir %s;", TEST_DIR, TEST_DIR, TEST_DIR);
    do_assert(!system(cmd));   
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    printf("** init tests\n");
    init_tests();

    printf("** test plrat_reader_read_vbl_int\n");
    test_vbl_int();

    printf("** test plrat_reader_read_vbl_ul\n");
    test_vbl_ul();

    printf("** test plrat_reader_read_vbl_ints\n");
    test_vbl_ints();

    printf("** DONE\n");
    return 0;
}