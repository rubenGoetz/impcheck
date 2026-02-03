
#include <stdlib.h>

#include "merge_buffer.h"

static void resize_data(struct merge_buffer* buffer) {
    // Unit clause has size sizeof(id + nb_lits + lit) = 4B + 2B + 2B = 8B
    // => buffer can contain at most capacity / 8 Byte clauses
    u64 max_size = buffer->capacity / 8;
    u64 new_data_size = buffer->data_size * 1.5;
    if (new_data_size == buffer->data_size)
        new_data_size = new_data_size + 1;
    buffer->data_size = MIN(new_data_size, max_size);
    buffer->data = (clause_ptr*)trusted_utils_realloc(buffer->data, sizeof(clause_ptr) * buffer->data_size);
}

struct merge_buffer* merge_buffer_init(u64 capacity, char* file_name) {
    struct merge_buffer* buffer = (struct merge_buffer*)trusted_utils_malloc(sizeof(struct merge_buffer));
    if (file_name)
        buffer->file = fopen(file_name, "rb");
    else
        buffer->file = NULL;
    buffer->capacity = capacity;
    buffer->size = 0;
    // start with 1/100 of possible clause count in buffer
    buffer->data_size = MAX(capacity / 800, 1);
    buffer->data = (clause_ptr*)trusted_utils_calloc(buffer->data_size, sizeof(clause_ptr));
    buffer->start_idx = 0;
    buffer->end_idx = 0;
    buffer->eof = false;

    return buffer;
}

void merge_buffer_free(struct merge_buffer* buffer) {
    for (u64 i = 0; i < buffer->end_idx - buffer->start_idx; i++)
        delete_flat_clause(buffer->data[buffer->start_idx + i]);
    if (buffer->file)
        fclose(buffer->file);
    free(buffer->data);
    free(buffer);
}

void merge_buffer_open_file(struct merge_buffer* buffer, char* file_name) {
    if (buffer->file)
        fclose(buffer->file);
    
    buffer->file = fopen(file_name, "rb");
    if (!buffer->file) {
        char msg[512];
        snprintf(msg, 512, "Could not open file at %s", file_name);
        trusted_utils_log_err(msg);
    }
}

#define CLAUSE_SIZE(NB_LITS) (sizeof(id_type) + sizeof(nb_lits_type) + (nb_lits * sizeof(lit_type)))

int merge_buffer_fill(struct merge_buffer* buffer) {
    if (buffer->eof)
        return 1;
    
    // shift remaining data to start of buffer
    size_t elem_count = buffer->end_idx - buffer->start_idx;
    if (buffer->start_idx > 0) {
        for (size_t i = 0; i < elem_count; i++)
            buffer->data[i] = buffer->data[buffer->start_idx + i];
        buffer->start_idx = 0;
        buffer->end_idx = elem_count;
    }

    // read clauses while capacity allows it
    u64 id;
    u64 nb_read;
    int nb_lits;
    while (true) {
        // can't use trusted_utils_read_ul(file) since we expect EOF sometime
        nb_read = UNLOCKED_IO(fread)(&id, sizeof(u64), 1, buffer->file);
        if (nb_read == 0) {
            buffer->eof = true;
            break;
        }

        nb_lits = trusted_utils_read_int(buffer->file);
        if (buffer->size + CLAUSE_SIZE(nb_lits) > buffer->capacity) {
            fseek(buffer->file, -(sizeof(u64) + sizeof(int)), SEEK_CUR);
            break;
        }
        clause_ptr c = create_flat_clause(id, nb_lits, NULL);
        trusted_utils_read_ints(get_clause_lits(c), nb_lits, buffer->file);
        buffer->data[buffer->end_idx++] = c;
        buffer->size += CLAUSE_SIZE(nb_lits);

        if (buffer->end_idx >= buffer->data_size)
            resize_data(buffer);

    }

    return 0;
}

clause_ptr merge_buffer_next_clause(struct merge_buffer* buffer) {
    if (buffer->start_idx == buffer->end_idx && !buffer->eof)
        merge_buffer_fill(buffer);
    else if (buffer->start_idx == buffer->end_idx && buffer->eof)
        return NULL;

    buffer->size -= get_clause_size(buffer->data[buffer->start_idx]);
    return buffer->data[buffer->start_idx++];
}

void merge_buffer_set_file_pointer(struct merge_buffer* buffer, long pos) {
    fseek(buffer->file, pos, SEEK_SET);
}
