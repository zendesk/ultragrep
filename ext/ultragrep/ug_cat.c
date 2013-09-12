// ex: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "ug_index.h"

/* 
 * ug_cat -- given a log file and (possibly) a file + (timestamp -> offset) index, cat the file starting 
 *           from about that timestamp 
 */ 

#define USAGE "Usage: ug_cat file timestamp\n"

int main(int argc, char **argv)
{
    int nread;
    FILE *log;
    FILE *index;
    char *log_fname, *index_fname, buf[4096];

    if ( argc < 3 ) {
        fprintf(stderr, USAGE);
        exit(1);
    }

    log_fname = argv[1];

    log = fopen(log_fname, "r");
    if ( !log ) { 
      perror("Couldn't open log file");
      exit(1);
    }

    index_fname = ug_get_index_fname(log_fname, "idx");

    index = fopen(index_fname, "r");
    if ( strcmp(log_fname + (strlen(log_fname) - 3), ".gz") == 0 ) {
        char *gzidx_fname;
        FILE *gzidx;

        if ( index ) { 
            gzidx_fname = ug_get_index_fname(log_fname, "gzidx");
            gzidx = fopen(gzidx_fname, "r");
            if ( !gzidx ) {
                perror("error opening gzidx component");
                exit(1);
            }
            extract(log, atol(argv[2]), index, gzidx);

        } else {
            extract(log, atol(argv[2]), NULL, NULL);

        }
    } else {
        if ( index ) 
            fseeko(log, ug_get_offset_for_timestamp(index, atol(argv[2])), SEEK_SET);

        while ( nread = fread(buf, 1, 4096, log) )  
            fwrite(buf, 1, nread, stdout);
    }
}


