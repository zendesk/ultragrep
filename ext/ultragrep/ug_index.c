// ex: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "ug_index.h"

int ug_write_index(FILE *file, uint64_t time, uint64_t offset)
{
  fwrite(&time, 8, 1, file);
  fwrite(&offset, 8, 1, file);
}

int ug_read_index_entry(FILE *file, struct ug_index *idx)
{
  int nread;
  nread = fread(&(idx->time), 8, 1, file);
  if ( !nread ) 
    return 0;

  nread = fread(&(idx->offset), 8, 1, file);
  return 1;
}

int ug_get_last_index_entry(FILE *file, struct ug_index *idx) {
  while (ug_read_index_entry(file, idx));
}

void ug_seek_to_timestamp(FILE *flog, FILE *findex, uint64_t time, struct ug_index *param_idx) 
{
  struct ug_index idx;
  off_t last_offset = 0;

  for(;;) { 
    if ( !ug_read_index_entry(findex, &idx) ) 
      break;

    if ( idx.time > time )
      break;
  
    last_offset = idx.offset;
  } 

  if ( last_offset ) {
      fseek(flog, offset, SEEK_SET);
  }
}

/* returns malloc'ed memory. */
char *ug_get_index_fname(char *log_fname, char *ext) 
{
  char *tmp, *dir, *index_fname;

  tmp = strdup(log_fname);
  dir = dirname(tmp);

  index_fname = malloc(strlen(dir) + strlen(basename(log_fname)) + strlen("/..") + strlen(ext) + 1);

  sprintf(index_fname, "%s/.%s.%s", dir, basename(log_fname), ext);
  free(tmp);
  return index_fname;
}


