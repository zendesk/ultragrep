#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include "pcre.h"

#define WAITING_PATTERN 0
#define IN_REQ 1
#define REQ_COMPLETE 2
#define EOF_REACHED 3
#define STOP_SIGNAL 4

typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    pcre **regexps;

}context;

typedef struct {
    char **buf;
    int lines;
    time_t time;
    int state;
    pcre* boundary_regex;
}request_t;

int req_extractor_init(request_t *req) {
    const char* error;
    int erroffset;
    req->boundary_regex =  pcre_compile("^Processing.*(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})", 0, &error, &erroffset, NULL);
    req->state = WAITING_PATTERN;
    req->lines = 0;
}

int req_found(request_t* req, int method, int (*on_req)(request_t*, void*), void *arg) {
    int i, res;
    req->state = method;
    res = on_req(req, arg);

    for(i = 0; i < req->lines; i++) {
        free(req->buf[i]);
    }
    free(req->buf);
    req->buf = NULL;
    req->lines = 0;
    return(res);
}

int req_extract_each_line(char *line, int line_size, request_t* req, int (*on_req)(request_t*, void*), void *arg) {
    int ovector[30];
    char *date_buf;
    struct tm request_tm;
    int matched = 0;
    int i;

    if(line_size == 1)
        return req->state; //skip empty lines

    if(line_size == -1) {
        if(req->buf) {
            int res = req_found(req, EOF_REACHED, on_req, arg);
            if(res == -1)
                return(STOP_SIGNAL);
        }
        return(req->state);
    }

    matched = pcre_exec(req->boundary_regex, NULL, line, line_size,0,0,ovector, 30);
    if(matched > 0) {
        if(req->buf) {
            int method = WAITING_PATTERN;
            int res;
            if(req->state == IN_REQ) {
                method = REQ_COMPLETE;
            }
            res = req_found(req, method, on_req, arg);
            if(res == -1)
                return(STOP_SIGNAL);
        }
        req->state = IN_REQ;
        pcre_get_substring(line, ovector, matched, 1, (const char **)&date_buf);
        strptime(date_buf, "%Y-%m-%d %H:%M:%S", &request_tm);
        req->time = mktime(&request_tm);
        free(date_buf);
    }
    req->buf = realloc(req->buf, sizeof(char *) * (req->lines + 1));
    req->buf[req->lines] = line;
    req->lines++;
    return(req->state);
}

int check_request(int lines, char **request, time_t request_time, pcre **regexps, int num_regexps)
{
	int *matches, i, j, matched;

	matches = malloc(sizeof(int) * num_regexps);
	memset(matches, 0, (sizeof(int) * num_regexps));

	for(i=0; i < lines; i++) {
		for(j=0; j < num_regexps; j++) {
			int ovector[30];
			if ( matches[j] ) continue;

			matched = pcre_exec(regexps[j], NULL, request[i], strlen(request[i]), 0, 0, ovector, 30);
			if ( matched > 0 )
				matches[j] = 1;
		}
	}

	matched = 1;
	for (j=0; j < num_regexps; j++) {
		matched &= matches[j];
	}

	free(matches);
	return(matched);
}

void print_request(int request_lines, char **request)
{
	int i, j;
	putchar('\n');

	for(i=0; i < request_lines; i++)
		printf("%s", request[i]);

	for(j=0; j < strlen(request[request_lines - 1]) && j < 80; j++ )
		putchar('-');

	putchar('\n');
	fflush(stdout);
}

int handle_request(request_t* req, context* cxt) {
    static int tick = 0;
    if(req->time > cxt->start_time &&
            check_request(req->lines,  req->buf, req->time, cxt->regexps, cxt->num_regexps)) {
					printf("@@%lu\n", req->time);

                                        print_request(req->lines, req->buf);
    }

    if ( tick % 100 == 0 )
        printf("@@%lu\n", req->time);
    tick++;
    if(req->time > cxt->end_time)
        return -1;
    return 0;
}
int main(int argc, char **argv)
{
    int i;
    context *cxt;
    request_t *req;
    const char *error;
    int erroffset;
    char *line = NULL;
    ssize_t line_size, allocated;

    if ( argc < 4 ) {
        fprintf(stderr, "Usage: ug_guts start_time end_time regexps [... regexps]\n");
        exit(1);
    }

    cxt = malloc(sizeof(context));
    cxt->start_time = atol(argv[1]);
    cxt->end_time = atol(argv[2]);

    cxt->num_regexps = argc - 3;
    cxt->regexps = malloc(sizeof(pcre *) * cxt->num_regexps);

    for ( i = 3; i < argc; i++) {
        cxt->regexps[i-3] = pcre_compile(argv[i], 0, &error, &erroffset, NULL);
        if ( error ) {
            fprintf(stderr, "Error compiling regexp \"%s\": %s\n", argv[i], error);
            exit;
        }
    }

    req = malloc(sizeof(request_t));

    req_extractor_init(req);

    while(1) {
        int ret;
        line_size = getline(&line, &allocated, stdin);
        ret = req_extract_each_line(line, line_size, req, &handle_request, cxt);
        if(ret == EOF_REACHED || ret == STOP_SIGNAL) {
            break;
        }
    }
}

