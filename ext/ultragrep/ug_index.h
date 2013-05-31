#include <stdint.h>
#include <stdio.h>

#define INDEX_EVERY 10
int ug_write_index(FILE *file, uint64_t time, uint64_t offset, char *data, uint32_t data_size);
