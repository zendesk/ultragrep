// ex: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "pcre.h"
#include "rails_req.h"
#include "work_req.h"
#include "work_req.h"
#include "ug_index.h"

#define USAGE "Usage: ug_build_index (work|app) file\n"


// index file format
// [64bit,64bit] -- timestamp, file offset 
// [32bit, Nbytes] -- extra data

void handle_request(request_t* req, void* _cxt) {
    build_idx_context_t *cxt = _cxt;
    time_t floored_time;
    floored_time = req->time - (req->time % INDEX_EVERY);
    if ( !cxt->last_index_time || floored_time > cxt->last_index_time ) {
        ug_write_index(cxt->findex, floored_time, req->offset);
        cxt->last_index_time = floored_time;
    }
}

void open_indexes(char *log_fname, build_idx_context_t *cxt)
{
    char *index_fname, *gz_index_fname;

    index_fname = ug_get_index_fname(log_fname, "idx");

    if ( strcmp(log_fname + (strlen(log_fname) - 3), ".gz") == 0 ) {
        gz_index_fname = ug_get_index_fname(log_fname, "gzidx");
        /* we don't do incremental index building in gzipped files -- we just truncate and 
         * build over*/
        cxt->findex = fopen(index_fname, "w+");
        cxt->fgzindex = fopen(gz_index_fname, "w+");

        if ( !cxt->findex || !cxt->fgzindex ) { 
            perror("Couldn't open index file");
            exit(1);
        }
    } else {
        cxt->findex = fopen(index_fname, "r+");
        if ( cxt->findex ) { 
            /* seek in the log, (and the index, with get_offset_for_timestamp()) to the 
             * last timestamp we indexed */
            fseeko(cxt->flog, ug_get_offset_for_timestamp(cxt->findex, -1), SEEK_SET);
        } else {
            cxt->findex = fopen(index_fname, "w+");
        }
        if ( !cxt->findex ) { 
            perror("Couldn't open index file");
            exit(1);
        }
    }
}

int main(int argc, char **argv)
{
    build_idx_context_t *cxt;
    char *line = NULL, *index_fname = NULL, *dir, *type, *log_fname;
    ssize_t line_size, allocated;

    if ( argc < 3 ) {
        fprintf(stderr, USAGE);
        exit(1);
    }

    type = argv[1];
    log_fname = argv[2];

    cxt = malloc(sizeof(build_idx_context_t));
    bzero(cxt, sizeof(build_idx_context_t));

    if (strcmp(type, "work") == 0) {
        cxt->m = work_req_matcher(&handle_request, NULL, cxt);
    } else if(strcmp(type, "app") == 0) {
        cxt->m = rails_req_matcher(&handle_request, NULL, cxt);
    } else {
        fprintf(stderr, USAGE);
        exit(1);
    }

    cxt->flog = fopen(log_fname, "r");
    if ( !cxt->flog ) { 
        perror("Couldn't open log file");
        exit(1);
    }

    open_indexes(log_fname, cxt);

    if ( strcmp(log_fname + (strlen(log_fname) - 3), ".gz") == 0 ) {
        build_gz_index(cxt);
    } else {
        while(1) {
            int ret;
            off_t offset;
            offset = ftello(cxt->flog);
            line_size = getline(&line, &allocated, cxt->flog);
            ret = cxt->m->process_line(cxt->m, line, line_size, offset);

            if(ret == EOF_REACHED || ret == STOP_SIGNAL)
                break;
          
            line = NULL;
      }
    }
    exit(0);
}


