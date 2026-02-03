
#ifndef MERGE_BUFFER_H
#define MERGE_BUFFER_H

#include "vec.h"
#include "clause_flat.h"

struct merge_buffer {
    FILE* file;
    u64 capacity;
    u64 size;
    u64 start_idx;
    u64 end_idx;
    u64 data_size;
    clause_ptr* data;
    bool eof;
};

struct merge_buffer* merge_buffer_init(u64 capacity, char* file_name);
void merge_buffer_free(struct merge_buffer* buffer);

void merge_buffer_open_file(struct merge_buffer* buffer, char* file_name);
int merge_buffer_fill(struct merge_buffer* buffer);
clause_ptr merge_buffer_next_clause(struct merge_buffer* buffer);
void merge_buffer_set_file_pointer(struct merge_buffer* buffer, long pos);

#endif
