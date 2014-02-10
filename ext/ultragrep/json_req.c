#include <stdio.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"
#include "json_req.h"
#include "jansson.h"
#include <string.h>
#include <stdlib.h>

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

//pretty print Json
int print_json_request(char **request, char **json_print_text)
{
    int i, j, is_message_only = 0 ;
    json_t *j_object;
    json_error_t j_error;

    j_object = json_loads(request[0], 0, &j_error); //load the JSON object

    if(!j_object) {
        fprintf(stderr,"Error: Corrupt JSON object, error message: [%s], line: [%s]\n", j_error.text,  request[0]);
        printf("Error: Corrupt JSON object, error message: [%.500s], line: [%.500s]\n", j_error.text,  request[0]);

        return -1;
    }

    if(!json_is_object(j_object)) {
        fprintf(stderr,"Error: JSON data -- is not a valid JSON object, error message: [%s], line: [%s]\n", j_error.text,  request[0]);
        printf("Error: JSON data -- is not a valid JSON object, error message: [%.500s], line: [%.500s]\n", j_error.text,  request[0]);

        json_decref(j_object);
        return -1;
    }

    //get formatted data
    *json_print_text = json_dumps(j_object,JSON_INDENT(indentValue)| JSON_PRESERVE_ORDER);

    if(!json_print_text) {
        fprintf(stderr,"Error:  Corrupt JSON data, error message: [%s], line: [%s]\n", j_error.text,  request[0]);
        printf("Error: Corrupt JSON data, error message: [%.500s], line: [%.500s]\n", j_error.text,  request[0]);

        json_decref(j_object);
        return -1;
    }
    else {
            json_decref(j_object); //clear before return
            return 1;
    }
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

    j_object = json_loads(line, 0, &j_error);
    if(!j_object) {
        fprintf(stderr,"Error: Corrupt JSON object, error message: [%s], line: [%s]\n", j_error.text, line);
        printf("Error: Corrupt JSON object, error message: [%.500s], line: [%.500s]\n", j_error.text, line);

        return -1;
    }
    if(!json_is_object(j_object)) {
        fprintf(stderr,"Error: JSON data -- is not a valid JSON object, error message: [%s], line: [%s]\n", j_error.text, line);
        printf("Error: JSON data -- is not a valid JSON object, error message: [%.500s], line: [%.500s]\n", j_error.text, line);

        json_decref(j_object);
        return -1;
     }

    j_time = json_object_get(j_object, "time");
    if(!j_time) {
        fprintf(stderr,"Error: Corrupt JSON object, error message: [%s], line: [%s]\n", j_error.text, line);
        printf("Error: Corrupt JSON object, error message: [%.500s], line: [%.500s]\n", j_error.text, line);

        json_decref(j_object); //dereference JANSSON objects

        return -1;
    }

    if (j_time) {
        message_text = json_string_value(j_time);
        strptime(message_text, "%Y-%m-%d %H:%M:%S", &request_tm);
        *time = timegm(&request_tm);
        matched = 1;
    }
    else {
        matched = -1;
    }

    json_decref(j_object); //dereference JANSSON objects
    return matched;
}
//process request
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
    char *json_print_text;
    context_t *cxt = (context_t *) cxt_arg;
    req_matcher_t * req_matcher = (req_matcher_t *)req;

    if ((req->time > cxt->start_time) && check_json_request(req->buf, cxt->regexps, cxt->num_regexps)) {
        if (req->time != 0) {
            printf("@@%ld\n", req->time);
        }
       //print JSON
       if(print_json_request(req->buf, &json_print_text) > 0) {
         printf("\n%s\n", json_print_text);

        //seperate request by ---
        for (int j=0 ; j < 80; j++)
            putchar('-');
        putchar('\n');
        free(json_print_text);
        fflush(stdout);
       }
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
