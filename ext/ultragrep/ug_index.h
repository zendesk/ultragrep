#include <stdint.h>
#include <stdio.h>
#include "req_matcher.h"

#define INDEX_EVERY 10

struct ug_index {
  uint64_t time;
  uint64_t offset;
  uint32_t data_size;
  char *data;
};

typedef struct {
    time_t last_index_time;
    FILE *log;
    FILE *index;
    req_matcher_t* m;

    struct ug_index *index;
} build_idx_context_t;

int ug_write_index(FILE *file, uint64_t time, uint64_t offset, char *data, uint32_t data_size);
int ug_get_last_index_entry(FILE *file, struct ug_index *idx); 
void ug_seek_to_timestamp(FILE *log, FILE *idx, uint64_t time, struct ug_index *param_idx);
char *ug_get_index_fname(char *log_fname);

