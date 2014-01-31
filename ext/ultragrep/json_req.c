#include <stdio.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"
#include "json_req.h"
#include "../../lib/jansson/include/jansson.h"

/*
todo:
  Make it work for key filtering
*/

static int lineCount=0;
static int indentValue = 4; //JSON print uses this

typedef struct {
    req_matcher_t base;
    on_req on_request;
    on_err on_error;
    void *arg;
    int stop_requested;
    int blank_lines;

} json_req_matcher_t;

typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    pcre **regexps;
    req_matcher_t *m;
} context_t;

static request_t request;

//
static void json_on_request(json_req_matcher_t * m, request_t * r)
{
    if (r && m->on_request) {
        if (r->lines > 0) {
            m->on_request(r, m->arg);
        }
        clear_request(r);
    }
}


void json_stop(req_matcher_t * base)
{
    json_req_matcher_t *m = (json_req_matcher_t *) base;
    m->stop_requested = 1;
}

//prints the JSON in a user readiable format
static int pretty_print_json(char * line , char** json_pretty_text)
{
    json_t *j_object;
    char * json_text;
    json_error_t j_error;

    j_object = json_loads(line, 0, &j_error);

    if(!json_is_object(j_object))
    {
        fprintf(stderr,"error: commit data -- is not an object [%.50s]\n", line);
        *json_pretty_text = "Corrupted Json";
        json_decref(j_object);
        return 0;
    }
    json_text = json_dumps(j_object,JSON_INDENT(indentValue)|JSON_PRESERVE_ORDER);

    if(json_text)
    {
        *json_pretty_text = json_text;
    }
    else
    {
        *json_pretty_text = "Corrupted Json";
        json_decref(j_object);
        return 0;
    }
    json_decref(j_object);
    return 1;
}

void print_json_request(int request_lines, char **request, int * matchedLines)
{
    int i, j;
    char * json_pretty_text;
    putchar('\n');

    for (i = 0; i < request_lines ; i++) {
        if (matchedLines[i]>0) {
            if (pretty_print_json(request[i], &json_pretty_text)>0) {
                printf("\n%s\n",json_pretty_text);
            }
        }
    }

    for (j = 0; j < strlen(request[request_lines - 1]) && j < 80; j++)
     putchar('-');

    putchar('\n');
    fflush(stdout);
}
//Parse and get the time
static int parse_req_json_time(char *line, ssize_t line_size, time_t * time)
{
    int matched = 0;
    int ovector[30];
    char *date_buf;
    struct tm request_tm;
    time_t tv;
    const char *error, * message_text;
    int erroffset;
    static pcre *regex = NULL;
    json_t *j_object, *j_time;
    json_error_t j_error;
    char * json_pretty_text;

    j_object = json_loads(line, 0, &j_error);
    if(!json_is_object(j_object)){
        fprintf(stderr,"error: commit data -- is not a valid JSON object [%.50s]\n", line);
        json_decref(j_object);
        return -1;
    }

    j_time = json_object_get(j_object, "time");
    message_text = json_string_value(j_time);
    strptime(message_text, "%Y-%m-%d %H:%M:%S", &request_tm);
    *time = mktime(&request_tm);

    json_decref(j_object); //dereference the mem for JANSSON

    return (1);
}

static int json_process_line(req_matcher_t * base, char *line, ssize_t line_size, off_t offset)
{
    json_req_matcher_t *m = (json_req_matcher_t *) base;

    // stop the processing if file is empty or end signal is received.
    if ((m->stop_requested) || (line_size == -1)){
        json_on_request(m, &request);
        return ((m->stop_requested) ? STOP_SIGNAL : EOF_REACHED);
    }

    add_to_request(&request, line, offset);

    if (request.time == 0) {
        parse_req_json_time(line, line_size, &(request.time));
    }
    return (0);
}

//check and returns matched
int check_json_request(int lines, char **request, time_t request_time, pcre ** regexps, int num_regexps, int* matchedLines)
{
    int i, j, matched;

    for (i = 0; i < lines; i++) {
        for (j = 0; j < num_regexps; j++) {
            int ovector[30];
            if (matchedLines[i] > 0)
                continue;

            matchedLines[i] = pcre_exec(regexps[j], NULL, request[i], strlen(request[i]), 0, 0, ovector, 30);
            if (matchedLines[i] > 0){
                matched = 1;
            }
        }
    }
    return matched;
}

void handle_json_request(request_t * req, void *cxt_arg)
{
    static int time = 0;
    context_t *cxt = (context_t *) cxt_arg;
    req_matcher_t * req_matcher = (req_matcher_t *)req;

    int* matchedLines = malloc(sizeof(int) * req->lines);
    memset(matchedLines, 0, (sizeof(int) * req->lines)); //initialize

    if ((req->time > cxt->start_time &&
         check_json_request(req->lines, req->buf, req->time, cxt->regexps, cxt->num_regexps, matchedLines))) {
        if (req->time != 0) {
            printf("@@%lu\n", req->time);
        }

        print_json_request(req->lines, req->buf, matchedLines);
    }
    if (req->time > time) {
        time = req->time;
        printf("@@%lu\n", time);
    }
    if (req->time > cxt->end_time) {
        cxt->m->stop(cxt->m);
    }
}

//
req_matcher_t *json_req_matcher(on_req fn1, on_err fn2, void *arg)
{
    json_req_matcher_t *m = (json_req_matcher_t *) malloc(sizeof(json_req_matcher_t));
    req_matcher_t *base = (req_matcher_t *) m;

    m->on_request = fn1;
    m->on_error = fn2;
    m->arg = arg;

    m->stop_requested = 0;
    m->blank_lines = 0;

    base->process_line = &json_process_line;
    base->stop = &json_stop;
    clear_request(&request);
    return base;
}
