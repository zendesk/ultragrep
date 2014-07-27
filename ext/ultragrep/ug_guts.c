#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pcre.h"
#include "req_matcher.h"
#include "rails_req.h"
#include "work_req.h"
#include "json_req.h"
#include <unistd.h>


typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    pcre **regexps;
    req_matcher_t *m;
} context_t;

static const char* commandparams="l:s:e:k:";
static const char* usage ="Usage: %s  ug_guts -l (work|app|json) -s start_time -e end_time regexps [... regexps]\n\n";

int check_request(int lines, char **request, time_t request_time, pcre ** regexps, int num_regexps)
{
    int *matches, i, j, matched;

    matches = malloc(sizeof(int) * num_regexps);
    memset(matches, 0, (sizeof(int) * num_regexps));

    for (i = 0; i < lines; i++) {
        for (j = 0; j < num_regexps; j++) {
            int ovector[30];
            if (matches[j])
                continue;

            matched = pcre_exec(regexps[j], NULL, request[i], strlen(request[i]), 0, 0, ovector, 30);
            if (matched > 0)
                matches[j] = 1;
        }
    }

    matched = 1;
    for (j = 0; j < num_regexps; j++) {
        matched &= matches[j];
    }

    free(matches);
    return (matched);
}

void print_request(int request_lines, char **request)
{
    int i, j;
    putchar('\n');

    for (i = 0; i < request_lines; i++)
        printf("%s", request[i]);

    for (j = 0; j < strlen(request[request_lines - 1]) && j < 80; j++)
        putchar('-');

    putchar('\n');
    fflush(stdout);
}

void handle_request(request_t * req, void *cxt_arg)
{
    static int time = 0;
    context_t *cxt = (context_t *) cxt_arg;
    if ((req->time > cxt->start_time 
          && req->time <= cxt->end_time 
          && check_request(req->lines, req->buf, req->time, cxt->regexps, cxt->num_regexps))) {
        if (req->time != 0) {
            printf("@@%lu\n", req->time);
        }
        print_request(req->lines, req->buf);
    }
    /* print a time-marker every second -- allows collections of logs with one sparse 
       log to proceed */
    if (req->time > time) {
        time = req->time;
        printf("@@%lu\n", time);
    }
    if (req->time > cxt->end_time) {
        cxt->m->stop(cxt->m);
    }
}


int parse_args(int argc,char** argv, context_t *cxt)
{
    extern char *optarg;
    extern int optind;
    const char *error;
    int erroffset, opt = 0,  optValue=0, j=0, retValue=1, i;

    //getOpt(): command line parsing
    while ((optValue = getopt(argc, argv, commandparams))!= -1) {
        switch (optValue) {
            case 'l':
                if (strcmp(optarg, "work") == 0) {
                    cxt->m = work_req_matcher(&handle_request, NULL, cxt);
                } else if (strcmp(optarg, "app") == 0) {
                    cxt->m = rails_req_matcher(&handle_request, NULL, cxt);
                } else if (strcmp(optarg, "json") == 0 ){
                    cxt->m = json_req_matcher(&handle_json_request, NULL, cxt);
                    }
                else {
                    return(-1);
                }
                break;
            case 's':
                cxt->start_time = atol(optarg);
                break;
            case 'e':
                cxt->end_time = atol(optarg);
                break;
            case 'k':
                add_key_value(optarg, cxt->m);
                break;
            case '?':
                return(-1);
                break;
            case -1:    //Options exhausted
                break;
            default:
                return(-1);
            }
    }
    if ( cxt->m < 1 ||  cxt->start_time < 1 || cxt->end_time < 1 ) {	// mandatory fields
        return(-1);
    }
    else if ((optind + 1 ) > argc) { //Need at least one argument after options
        return(-1);
        }

    if (optind < argc) {	//these are the arguments after the command-line options
        cxt->num_regexps = argc - optind;
        cxt->regexps = malloc(sizeof(pcre *) * cxt->num_regexps);
        for (i=0; optind < argc; ++optind, i++){
            cxt->regexps[i] = pcre_compile(argv[optind], 0, &error, &erroffset, NULL);
            if (error) {
                fprintf(stderr, "Error compiling regexp \"%s\": %s\n", argv[optind], error);
                exit(1);
            }
         }
    }
    return retValue;
}

int main(int argc, char **argv)
{
    context_t *cxt;
    char *line = NULL;
    ssize_t line_size, allocated;
    if (argc < 5) {
        fprintf(stderr, "%s", usage);
        exit(1);
    }
    cxt = calloc(1, sizeof(context_t));

    if (parse_args(argc, argv, cxt) > 0 ) {
        while (1) {
            int ret;
            line_size = getline(&line, &allocated, stdin);
            ret = cxt->m->process_line(cxt->m, line, line_size, 0);
            if (ret == EOF_REACHED || ret == STOP_SIGNAL) {
                break;
            }
            line = NULL;
        }
    }
    else {
        fprintf(stderr, "%s",usage);
    }
    // free allocated momory
    if (cxt->m->cleanup)
        cxt->m->cleanup(cxt->m);
    free(cxt->m);
    free(cxt);

}
