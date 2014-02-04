#include <stdio.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"
#include "json_req.h"
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

    //if -m parameter is passed then only display the message in the JSON rather then then Json block.
    //TODO: pass parameter on command line
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

void print_json_request(int request_lines, char **request, int * matchedLines)
{
    int i, j, is_message_only=0;
    char * json_pretty_text;
    putchar('\n');

    for (i = 0; i < request_lines ; i++) {
        if (matchedLines[i] > 0) {
            if (pretty_print_json(request[i], &json_pretty_text, is_message_only) > 0) {
                printf("\n%s\n",json_pretty_text);
            }
            //seperate request by ---
             for (j = 0; j < strlen(request[request_lines - 1]) && j < 100; j++)
                  putchar('-');
        }
    }
    putchar('\n');
    fflush(stdout);
}
//Parse and get the time
static int parse_req_json_time(char *line, ssize_t line_size, time_t * time)
{
    struct tm request_tm;
    const char * message_text;
    //Jansson parameters
    json_t *j_object, *j_time;
    json_error_t j_error;
    char * json_pretty_text;

    j_object = json_loads(line, 0, &j_error);
    if (check_json_validity(j_object, line) < 0) {
        return -1;
    }

    j_time = json_object_get(j_object, "time");
    message_text = json_string_value(j_time);
    strptime(message_text, "%Y-%m-%d %H:%M:%S", &request_tm);
    *time = mktime(&request_tm);

    json_decref(j_object); //dereference JANSSON objects

    return (1);
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
    return (0);
}

//check and returns matched
int check_json_request(int lines, char **request, time_t request_time, pcre ** regexps, int num_regexps, int* matchedLines, char *key)
{
    int i, j, matched;
    char * value;

    for (i = 0; i < lines; i++) {
        for (j = 0; j < num_regexps; j++) {
            int ovector[30];
            if (matchedLines[i] > 0)
                continue;

            // Allocate a buffer that will be used to get the string Value of json Key (if specified)
            value = (char*)malloc(sizeof(char)*(strlen(request[i])+1));
            memset(value, 0, sizeof(char)*(strlen(request[i])+1));

            // If the -k parameter is present then, change the indexes for the parameter
            if (json_based_key(request[i], key, &value, strlen(request[i]))) {
                // if key if found then use its value for searching the regex
                matchedLines[i] = pcre_exec(regexps[j], NULL, value, strlen(request[i]), 0, 0, ovector, 30);
            }
            else {
                // if it is a non key search, match the entire line for the regex
                matchedLines[i] = pcre_exec(regexps[j], NULL, request[i], strlen(request[i]), 0, 0, ovector, 30);
            }

            // deallocate the previously allocated buffer
            free(value);

            if (matchedLines[i] > 0) {
                matched = 1;
            }
        }
    }
    return matched;
}

// if -k parameter is passed on command line then search the value for that key rather then entire line
int json_based_key( char *request, char *key, char ** value_buffer, int buffer_size)
{
    json_t *j_object, *json_message_text;
    const char * json_text;
    json_error_t j_error;
    int is_str=0, is_number=0, number;

    if(key) {
        j_object = json_loads(request, 0, &j_error);
        if (check_json_validity(j_object, request) < 0) {
            return -1;
        }
        // Get the message
        json_message_text = json_object_get(j_object, key);
        is_str = json_is_string(json_message_text);
        if(!is_str)
            is_number = json_is_number(json_message_text);

        if(is_number) {
            snprintf(*value_buffer, buffer_size, "%d",json_integer_value(json_message_text));
        } else if(is_str) {
            strcpy(*value_buffer, json_string_value(json_message_text));
        } else {
            fprintf(stderr, "error: JSON%s: message is neither a string nor a number\n", request);
            json_decref(j_object);
            return 0;
        }
        json_decref(j_object);
        return 1;
    } else {
        return 0;
    }
}

void handle_json_request(request_t * req, void *cxt_arg)
{
    static int time = 0;
    context_t *cxt = (context_t *) cxt_arg;
    req_matcher_t * req_matcher = (req_matcher_t *)req;

    int* matchedLines = malloc(sizeof(int) * req->lines);
    memset(matchedLines, 0, (sizeof(int) * req->lines)); //initialize

    if ((req->time > cxt->start_time &&
         check_json_request(req->lines, req->buf, req->time, cxt->regexps, cxt->num_regexps, matchedLines, ((json_req_matcher_t*)(cxt->m))->key))) {
        if (req->time != 0) {
            printf("@@%lu\n", req->time);
        }

        print_json_request(req->lines, req->buf, matchedLines); //print JSON
    }
    if (req->time > time) {
        time = req->time;
        printf("@@%lu\n", time); //print time
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
