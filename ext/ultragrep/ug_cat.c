#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "ug_index.h"

#define USAGE "Usage: ug_cat file timestamp\n"

int main(int argc, char **argv)
{
    int nread;
    FILE *log;
    FILE *index;
    char *index_fname, buf[4096];

    if ( argc < 3 ) {
        fprintf(stderr, USAGE);
        exit(1);
    }

    log = fopen(argv[1], "r");
    if ( !log ) { 
      perror("Couldn't open log file");
      exit(1);
    }

    index_fname = ug_get_index_fname(argv[1]);

    index = fopen(index_fname, "r");
    if ( index ) { 
        ug_seek_to_timestamp(log, index, atol(argv[2]), NULL);
    }

    while ( nread = fread(buf, 1, 4096, log) )  {
        fwrite(buf, 1, nread, stdout);
    }

    fclose(log);
}


