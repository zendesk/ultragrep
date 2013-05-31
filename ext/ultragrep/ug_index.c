#include "ug_index.h"

int ug_write_index(FILE *file, uint64_t time, uint64_t offset, char *data, uint32_t data_size)
{
  fwrite(&time, 8, 1, file);
  fwrite(&offset, 8, 1, file);
  fwrite(&data_size, 4, 1, file);

  if ( data_size ) 
    fwrite(data, 1, data_size, file);
}

int ug_read_index_entry(FILE *file, uint64_t *time, uint64_t *offset, char **data, uint32_t *data_size)
{
  int nread;
  nread = fread(time, 8, 1, file);
  if ( !nread ) 
    return 0;

  nread = fread(offset, 8, 1, file);
  nread = fread(data_size, 4, 1, file);
  if ( *data_size && data ) {
    *data = malloc(*data_size);
    nread = fread(*data, 1, data_size, file);
  } else if ( data ) {
    *data = NULL;
  }
  return 1;
}

int ug_get_last_index_entry(FILE *file, uint64_t *time, uint64_t *offset) {
  uint64_t junk;
  while (ug_read_index_entry(file, time, offset, NULL, &junk));
}
