#include <stdio.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"
#include "json_req.h"
#include <string.h>
#include <stdlib.h>
#include "../../lib/jansson/include/jansson.h"

/*
This file handles the JSON logs, searches then and pritty prints the matching logs.

Example of normal search:
cat ../../storage/test.log-20140202.json | ~/Code/zendesk/ultragrep/ext/ultragrep/ug_guts json 0 139059835  b48f080a0d9c576703a

Example of using key based filtereing
cat ../../storage/test.log-20140202.json | ~/Code/zendesk/ultragrep/ext/ultragrep/ug_guts json 0 139059835 -k request_id b48f080a0d9c576703a
*/

static int indentValue = 4; //JSON print uses this

typedef struct {
    req_matcher_t base;
    on_req on_request;
    on_err on_error;
    void *arg;
    int stop_requested;
    int blank_lines;
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
    //free(json_text); //fre

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
    int ovector[30];
    char *date_buf;
    struct tm request_tm;
    time_t tv;
    const char *error;
    int erroffset;
    static pcre *regex = NULL;

    *time = 0;

    if (regex == NULL) {
        regex = pcre_compile("^(?:Processing|Started).*(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})", 0, &error, &erroffset, NULL);
    }
    matched = pcre_exec(regex, NULL, line, line_size, 0, 0, ovector, 30);
    if (matched > 0) {
        pcre_get_substring(line, ovector, matched, 1, (const char **) &date_buf);
        strptime(date_buf, "%Y-%m-%d %H:%M:%S", &request_tm);
        free(date_buf);

        *time = timegm(&request_tm);
        return (1);
    }
    return (-1);
}

static int json_process_line(req_matcher_t * base, char *line, ssize_t line_size, off_t offset)
{
    json_req_matcher_t *m = (json_req_matcher_t *) base;

    // stop the processing if file is empty or end signal is received.
    if ((m->stop_requested) || (line_size == -1)) {

        if (m->stop_requested)
            printf("\n\nSTOP REQUESTED[%s]", line);
        else
            printf("\n\nline_size == -1[%s]", line);

        json_on_request(m, &request);
        return ((m->stop_requested) ? STOP_SIGNAL : EOF_REACHED);
    }
    //printf("Adding line [%s]\n", line);
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
    int i, matched=1;

    for (i = 0; i < num_regexps; i++) {
        int ovector[30];
        if (pcre_exec(regexps[i], NULL, request[0], strlen(request[0]), 0, 0, ovector, 30)<=0){
            matched = 0;
            break;
        }
    }
    return matched;
}


//int check_json_keys(int lines, char **request, char **keys, pcre **regexps, int num_keys) {
//    int j, matched = 1;
//    char *value;
//    int* matchedLines = calloc(sizeof(int) *request);
//
//    for (j = 0; j < num_regexps; j++) {
//        int ovector[30];
//
//         // if it is a non key search, match the entire line for the regex
//        value = json_get_key(keys[j])
//        matchedLines[j] = pcre_exec(regexps[j], NULL, value, strlen(request[i]), 0, 0, ovector, 30);
//        matched = matched && matchedLines[j];
//}
//    return matched;
//}

// if -k parameter is passed on command line then search the value for that key rather then entire line
// ug_guts/ultragrep -l work -s start -e end -k key=regexp -k key=regexp r1 r2 r3
int json_get_key( char *request, char *key, char **value_buffer, int buffer_size)
{
    json_t *j_object, *json_message_text;
    const char *json_text;
    json_error_t j_error;
    int is_str=0, is_number=0, number;

    if(key) {
        j_object = json_loads(request, 0, &j_error);
        if (check_json_validity(j_object, request) < 0) {
            return -1;
        }
        //Get the message
        json_message_text = json_object_get(j_object, key);
        is_str = json_is_string(json_message_text);
        if(!is_str)
            is_number = json_is_number(json_message_text);

        if(is_number) {
            //if the key is number then conver it to string and assign to a buffer
            snprintf(*value_buffer, buffer_size, "%lld",json_integer_value(json_message_text));
        }
        else if(is_str) {
            strcpy(*value_buffer, json_string_value(json_message_text));
        }
        else {

            fprintf(stderr, "error: JSON%s: message is neither a string nor a number\n", request);
            json_decref(j_object);
            return -1;
        }
        json_decref(j_object); //deallocate the json objects allocated by jansson
        return 1;
    }
    else {
        return 0;
    }
}

void handle_json_request(request_t *req, void *cxt_arg)
{
    static int time = 0;
    context_t *cxt = (context_t *) cxt_arg;
    req_matcher_t * req_matcher = (req_matcher_t *)req;


    // TODO - Fix the time check
    //if ((req->time > cxt->start_time) && check_json_request(req->buf, cxt->regexps, cxt->num_regexps)) {
         if (check_json_request(req->buf, cxt->regexps, cxt->num_regexps)) {

            if (req->time != 0) {
                printf("@@%ld\n", req->time);
            }

            print_json_request(req->buf); //print JSON
         }
    //}

    if (req->time > time) {
        time = req->time;
        printf("@@%d\n", time); //print time
    }
    if (req->time > cxt->end_time) {
        cxt->m->stop(cxt->m);
    }
}

//
req_matcher_t *json_req_matcher(on_req fn1, on_err fn2, void *arg, char * key)
{
    json_req_matcher_t *m = (json_req_matcher_t *) malloc(sizeof(json_req_matcher_t));
    req_matcher_t *base = (req_matcher_t *) m;

    m->on_request = fn1;
    m->on_error = fn2;
    m->arg = arg;

    m->stop_requested = 0;
    m->blank_lines = 0;
    m->key=key; //set the key value in the context

    base->process_line = &json_process_line;
    base->stop = &json_stop;
    clear_request(&request);
    return base;
}
