#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include "pcre.h"
#include "req_matcher.h"
#include "rails_req.h"
#include "work_req.h"
#include "json_req.h"

typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    pcre **regexps;
    req_matcher_t *m;
} context_t;

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
    if ((req->time > cxt->start_time && check_request(req->lines, req->buf, req->time, cxt->regexps, cxt->num_regexps))) {
        if (req->time != 0) {
            printf("@@%lu\n", req->time);
        }

        print_request(req->lines, req->buf);
    }
    if (req->time > time) {
        time = req->time;
        printf("@@%lu\n", time);
    }
    if (req->time > cxt->end_time) {
        cxt->m->stop(cxt->m);
    }
}

int main(int argc, char **argv)
{
    int i;
    context_t *cxt;
    const char *error;
    int erroffset;
    char *line = NULL;
    ssize_t line_size, allocated;

    if (argc < 5) {
        fprintf(stderr, "Usage: ug_guts (work|app|json) start_time end_time regexps [... regexps]\n");
        exit(1);
    }

    cxt = malloc(sizeof(context_t));

   if (strcmp(argv[1], "work") == 0) {
        cxt->m = work_req_matcher(&handle_request, NULL, cxt);
    }
    else if (strcmp(argv[1], "app") == 0) {
        cxt->m = rails_req_matcher(&handle_request, NULL, cxt);
    }
    else if (strcmp(argv[1], "json") == 0 ){
        cxt->m = json_req_matcher(&handle_json_request, NULL, cxt);
    }
    else {
        fprintf(stderr, "Usage: ug_guts (work|app|json) start_time end_time regexps [... regexps]\n");
        exit(1);
    }

    cxt->start_time = atol(argv[2]);
    cxt->end_time = atol(argv[3]);

    cxt->num_regexps = argc - 4;
    cxt->regexps = malloc(sizeof(pcre *) * cxt->num_regexps);

    for (i = 4; i < argc; i++) {
        cxt->regexps[i - 4] = pcre_compile(argv[i], 0, &error, &erroffset, NULL);
        if (error) {
            fprintf(stderr, "Error compiling regexp \"%s\": %s\n", argv[i], error);
            exit;
        }
    }

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
