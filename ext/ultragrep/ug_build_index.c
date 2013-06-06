#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "pcre.h"
#include "req_matcher.h"
#include "rails_req.h"
#include "work_req.h"
#include "ug_index.h"

#define USAGE "Usage: ug_build_index (work|app) file\n"

typedef struct {
    time_t last_index_time;
    FILE *log;
    FILE *index;
    req_matcher_t* m;
} context_t;

// index file format
// [64bit,64bit] -- timestamp, file offset 
// [32bit, Nbytes] -- extra data

void handle_request(request_t* req, context_t* cxt) {
    time_t floored_time;
    floored_time = req->time - (req->time % INDEX_EVERY);
    if ( !cxt->last_index_time || floored_time > cxt->last_index_time ) {
        ug_write_index(cxt->index, floored_time, req->offset, NULL, 0);
        cxt->last_index_time = floored_time;
    }
}

int main(int argc, char **argv)
{
    context_t *cxt;
    char *line = NULL, *index_fname = NULL, *dir;
    ssize_t line_size, allocated;


    if ( argc < 3 ) {
        fprintf(stderr, USAGE);
        exit(1);
    }

    cxt = malloc(sizeof(context_t));
    memset(cxt, 0, sizeof(context_t));

    if(strcmp(argv[1],"work") == 0)
    {
        cxt->m = work_req_matcher(&handle_request, NULL, cxt);
    }
    else if(strcmp(argv[1], "app") == 0)
    {
        cxt->m = rails_req_matcher(&handle_request, NULL, cxt);
    }
    else
    {
        fprintf(stderr, USAGE);
        exit(1);
    }

    cxt->log = fopen(argv[2], "r");
    if ( !cxt->log ) { 
      perror("Couldn't open log file");
      exit(1);
    }

    index_fname = ug_get_index_fname(argv[2]);

    cxt->index = fopen(index_fname, "r+");
    if ( cxt->index ) { 
        struct ug_index idx;
        ug_get_last_index_entry(cxt->index, &idx);
        fseeko(cxt->log, idx.offset, SEEK_SET);
    } else {
        cxt->index = fopen(index_fname, "w+");
    }

    if ( !cxt->index ) { 
      perror("Couldn't open index file");
      exit(1);
    }

    while(1) {
        int ret;
        line_size = getline(&line, &allocated, cxt->log);
        ret = cxt->m->process_line(cxt->m, line, line_size, ftello(cxt->log) - line_size);
        if(ret == EOF_REACHED || ret == STOP_SIGNAL) {
            break;
        }
        line = NULL;
    }

    fclose(cxt->index);
    fclose(cxt->log);
}


