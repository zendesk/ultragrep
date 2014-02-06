#include <stdio.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"
#include "json_req.h"
#include <string.h>
#include <stdlib.h>
#include "jansson.h"

/*
 * This file handles the JSON logs. It uses Jansson to facilitate the parsing of JSON
 * Searches them, and prints the matching logs.
 */

static int indentValue = 4; //JSON print uses this

typedef struct {
    req_matcher_t base;
    on_req on_request;
    on_err on_error;
    void *arg;
    int stop_requested;
    char *key;

} json_req_matcher_t;

typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    pcre **regexps;
    req_matcher_t *m;
} context_t;

static request_t request;

static void json_on_request(json_req_matcher_t *m, request_t *r)
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

//Check if the object is valid JSON String
int check_json_validity(json_t *j_object, char *line)
{
  if(!json_is_object(j_object)) {
     fprintf(stderr,"error: JSON - is not a valid JSON object [%.50s]\n", line);
     json_decref(j_object);
     return -1;
    }
   else return 1;
}

//prints the JSON in a user readiable format
static int pretty_print_json(char *line , char** json_pretty_text, int print_message_only)
{
    json_t *j_object, *json_message_text;
    const char * json_text;
    json_error_t j_error;

    j_object = json_loads(line, 0, &j_error); //load the JSON object

    if(!json_is_object(j_object)) {
            fprintf(stderr,"error: JSON data -- is not a valid JSON object [%.50s]\n", line);
            *json_pretty_text = "Corrupted Json";
            json_decref(j_object);
            return -1;
    }

    if(print_message_only) {
    json_message_text = json_object_get(j_object, "message");

    if(!json_is_string(json_message_text)) {
       fprintf(stderr, "\nerror: JSON%s: message is not a string\n",line);
       json_decref(j_object);
       return -1;
      }
      json_text = json_string_value(json_message_text);
    }
    //print the entire JSON string
    else {
        json_text = json_dumps(j_object,JSON_INDENT(indentValue)|JSON_PRESERVE_ORDER);
    }

    if(json_text) {
        *json_pretty_text = json_text;
    }
    else {
        *json_pretty_text = "Corrupted Json";
        json_decref(j_object);
        return -1;
    }

    json_decref(j_object);
    return 1;
}

void print_json_request(char **request)
{
    int i, j, is_message_only = 0 ;
    char *json_pretty_text;
    putchar('\n');

    if (pretty_print_json(request[0], &json_pretty_text, is_message_only) > 0) {
        printf("\n%s\n",json_pretty_text);
    }
    //seperate request by ---
    for (j = 0;  j < 100; j++)
        putchar('-');
    putchar('\n');
    fflush(stdout);
}

//Parse and get the time
static int parse_req_json_time(char *line, ssize_t line_size, time_t *time)
{
    int matched = 0;
    struct tm request_tm;
    *time = 0;

    const char * message_text;
    //Jansson parameters
    json_t *j_object, *j_time;
    json_error_t j_error;
    char * json_pretty_text;

    j_object = json_loads(line, 0, &j_error);
    if (check_json_validity(j_object, line) < 0) {
        json_decref(j_object);
        return -1;
    }

    j_time = json_object_get(j_object, "time");
    if (j_time) {
        message_text = json_string_value(j_time);
        strptime(message_text, "%Y-%m-%d %H:%M:%S", &request_tm);
        *time = timegm(&request_tm);
        matched=1;
    }
    else {
        matched=-1;
    }

    json_decref(j_object); //dereference JANSSON objects
    return matched;
}

static int json_process_line(req_matcher_t * base, char *line, ssize_t line_size, off_t offset)
{
    json_req_matcher_t *m = (json_req_matcher_t *) base;

    // stop the processing if file is empty or end signal is received.
    if ((m->stop_requested) || (line_size == -1)) {
        json_on_request(m, &request);
        return ((m->stop_requested) ? STOP_SIGNAL : EOF_REACHED);
    }

    add_to_request(&request, line, offset);

    if (request.time == 0) {
        parse_req_json_time(line, line_size, &(request.time));
    }
    json_on_request(m, &request); //remove the old request and queue the new one

    return (0);
}

//check and returns matched
int check_json_request(char **request, pcre **regexps, int num_regexps)
{
    int i, matched = 1;

    for (i = 0; i < num_regexps; i++) {
        int ovector[30];
        if (pcre_exec(regexps[i], NULL, request[0], strlen(request[0]), 0, 0, ovector, 30)<=0){
            matched = 0;
            break;
        }
    }
    return matched;
}

void handle_json_request(request_t *req, void *cxt_arg)
{
    static int time = 0;
    context_t *cxt = (context_t *) cxt_arg;
    req_matcher_t * req_matcher = (req_matcher_t *)req;

    if ((req->time > cxt->start_time) && check_json_request(req->buf, cxt->regexps, cxt->num_regexps)) {
        if (req->time != 0) {
            printf("@@%ld\n", req->time);
        }
        print_json_request(req->buf); //print JSON
     }

    if (req->time > time) {
        time = req->time;
        printf("@@%d\n", time); //print time
    }

    if (req->time > cxt->end_time) {
        cxt->m->stop(cxt->m);
    }
}

req_matcher_t *json_req_matcher(on_req fn1, on_err fn2, void *arg)
{
    json_req_matcher_t *m = (json_req_matcher_t *) malloc(sizeof(json_req_matcher_t));
    req_matcher_t *base = (req_matcher_t *) m;

    m->on_request = fn1;
    m->on_error = fn2;
    m->arg = arg;
    m->stop_requested = 0;

    base->process_line = &json_process_line;
    base->stop = &json_stop;
    clear_request(&request);
    return base;
}
