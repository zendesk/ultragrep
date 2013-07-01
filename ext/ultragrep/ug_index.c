#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "ug_index.h"

int ug_write_index(FILE *file, uint64_t time, uint64_t offset, char *data, uint32_t data_size)
{
  fwrite(&time, 8, 1, file);
  fwrite(&offset, 8, 1, file);
  fwrite(&data_size, 4, 1, file);

  if ( data_size ) 
    fwrite(data, 1, data_size, file);
}

int ug_read_index_entry(FILE *file, struct ug_index *idx, int read_data)
{
  int nread;
  nread = fread(&(idx->time), 8, 1, file);
  if ( !nread ) 
    return 0;

  nread = fread(&(idx->offset), 8, 1, file);
  nread = fread(&(idx->data_size), 4, 1, file);
  if ( idx->data_size ) {
    if ( read_data ) {
      idx->data = malloc(idx->data_size);
      nread = fread(idx->data, 1, idx->data_size, file);
    } else {
      fseek(file, idx->data_size, SEEK_CUR);
    }
  }

  return 1;
}

int ug_get_last_index_entry(FILE *file, struct ug_index *idx) {
  while (ug_read_index_entry(file, idx, 0));
}

void ug_seek_to_timestamp(FILE *flog, FILE *findex, uint64_t time, struct ug_index *param_idx) 
{
  struct ug_index idx, prev;
  off_t last_offset = 0;

  memset(&prev, 0, sizeof(struct ug_index));

  for(;;) { 
    if ( !ug_read_index_entry(findex, &idx, 1) ) {
      if ( prev.data )
        free(prev.data);

      memcpy(&prev, &idx, sizeof(struct ug_index));
      break;
    }

    if ( idx.time > time )
      break;

    if ( prev.data )
      free(prev.data);
    memcpy(&prev, &idx, sizeof(struct ug_index));
  } 

  if ( prev.offset ) {
      fseek(flog, prev.offset, SEEK_SET);
      if ( param_idx ) {
        memcpy(param_idx, &prev, sizeof(struct ug_index));
      }
  }
}

/* returns malloc'ed memory. */
char *ug_get_index_fname(char *log_fname) 
{
  char *dir, *index_fname;

  dir = strdup(log_fname);
  dir = dirname(dir);

  index_fname = malloc(strlen(dir) + strlen(basename(log_fname)) + strlen("/..idx") + 1);

  sprintf(index_fname, "%s/.%s.idx", dir, basename(log_fname));
  free(dir);
  return index_fname;
}


