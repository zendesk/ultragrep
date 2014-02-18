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
 * Searches based on key=value, and prints the matching logs.
 */

static int indentValue = 4; //JSON print uses this

typedef struct {
    req_matcher_t base;
    on_req on_request;
    on_err on_error;
    void *arg;
    int stop_requested;
    char *key;
    KVpair * kv_list;
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
        *json_print_text = json_dumps(j_object,JSON_INDENT(indentValue)| JSON_PRESERVE_ORDER);
        fprintf(stderr,"\n\nErrored json\n[%s]\n\n", *json_print_text);
        return 0;
    }

    if(!json_is_object(j_object)) {
        fprintf(stderr,"Error: JSON data -- is not a valid JSON object, error message: [%s], line: [%s]\n", j_error.text,  request[0]);
        printf("Error: JSON data -- is not a valid JSON object, error message: [%.500s], line: [%.500s]\n", j_error.text,  request[0]);
        fprintf(stderr,"\n\nErrored json\n[%s]\n\n", *json_print_text);
        json_decref(j_object);
        return 0;
    }

    //get formatted data
    *json_print_text = json_dumps(j_object,JSON_INDENT(indentValue)| JSON_PRESERVE_ORDER);

    if(!json_print_text) {
        fprintf(stderr,"Error:  Corrupt JSON data, error message: [%s], line: [%s]\n", j_error.text,  request[0]);
        printf("Error: Corrupt JSON data, error message: [%.500s], line: [%.500s]\n", j_error.text,  request[0]);

        fprintf(stderr,"\n\nErrored3 json\n[%s]\n\n", *json_print_text);
        json_decref(j_object);
        return 0;
    }
    else {
        json_decref(j_object); //clear before return
        return 1;
    }
}


// deallocaing memory of attr_val is of the caller.
int get_json_attr_value(char *line, ssize_t line_size, char* attr, char** attr_val)
{
    int matched = 0, retValue = 1;
    char c[20];
    const char* temp_str;
    //Jansson parameters
    json_t *j_object, *j_attr;
    json_error_t j_error;

    j_object = json_loads(line, 0, &j_error);
    if(!j_object) {
        fprintf(stderr,"Error: Corrupt JSON object, error message:1 [%s], on line: [%s]\n", j_error.text, line);
        printf("Error: Corrupt JSON object, error message:2 [%.500s], on line: [%.500s]\n", j_error.text, line);
        return -1;
    }
    if(!json_is_object(j_object)) {
        fprintf(stderr,"Error: JSON data -- is not a valid JSON object, error message:3 [%s], line: [%s]\n", j_error.text, line);
        printf("Error: JSON data -- is not a valid JSON object, error message:4 [%.500s], line: [%.500s]\n", j_error.text, line);
        json_decref(j_object);
        return -1;
     }

    j_attr = json_object_get(j_object, attr);
    if (j_attr) {
        switch (json_typeof(j_attr)) {
            case JSON_STRING:
                temp_str = json_string_value(j_attr);
                break;
            case JSON_INTEGER:
                sprintf(c, "%d", (int)json_integer_value(j_attr));
                temp_str = c;
                break;
            case JSON_TRUE:
                temp_str = "true";
                break;
            case JSON_FALSE:
                temp_str = "false";
                break;
            case JSON_NULL:
            default:
                json_decref(j_object);
                return -1;

        }
        if (temp_str) {
            *attr_val = (char*)malloc(strlen(temp_str)+1); // this needs to be freed by calling function.
            strcpy(*attr_val, temp_str);
        }
    }
    else {
        retValue = 0;
    }
    json_decref(j_object);
    return retValue;
}


//Parse and get the time
static int parse_req_json_time(char *line, ssize_t line_size, time_t *time)
{
    int matched = 0;
    struct tm request_tm;
    char* message_text;
    *time = 0;

    if (get_json_attr_value(line, line_size, "time", &message_text)>0) {
        strptime(message_text, "%Y-%m-%d %H:%M:%S", &request_tm);
        *time = timegm(&request_tm);
        matched = 1;
        free(message_text);
    }
    else {
        matched = -1;
    }

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
int check_json_request(char **request, pcre **regexps, int num_regexps, req_matcher_t *m)
{
    int i, matched = 1;
    KVpair* current;
    json_req_matcher_t * req_matcher = (json_req_matcher_t*)m;
    char * attr_value;

    // check key value pairs
    if (req_matcher->kv_list) {
        //fprintf(stderr, "\nThere are some Key-Value pairs to search\n");
        current = req_matcher->kv_list;

        do {
            int ovector[30];
            if (get_json_attr_value(request[0], strlen(request[0]), current->key, &attr_value)>0) { // if key exists in this line
                if (pcre_exec((const pcre*)current->value, NULL, attr_value, strlen(attr_value), 0, 0, ovector, 30)>0){
                    free(attr_value);
                } else { // this key did not match in json - return not matched
                    matched = 0;
                    return matched;
                }
            } else { // this key did not match in json - return not matched
                 matched = 0;
                 return matched;
             }
            current = current->next;
        } while(current);
    }

    // check all remaining regex
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

    if ((req->time > cxt->start_time) && check_json_request(req->buf, cxt->regexps, cxt->num_regexps, cxt->m)) {
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

void add_key_value_pair(char* key, char* value, void* ctx)
{
    json_req_matcher_t * m;
    KVpair* current;

    m = (json_req_matcher_t*)((context_t*)ctx)->m;
    if (m->kv_list) { // if linked list exists, go to the last node
        current = m->kv_list;
        while (current->next) {
            current = current->next;
        }
        current->next = (KVpair*) malloc(sizeof(KVpair));
        current = current->next;
    } else { // add the first node
        m->kv_list = (KVpair*) malloc(sizeof(KVpair));
        current = m->kv_list;
    }

    if (current) {
        current->key = key;
        current->value = (pcre*)value;
        current->next = NULL;
    }
}

int add_key_value(char* key_value, void* ctx) {
    char * pch;
    int i=0;
    char * key, * value;
    const char *error;
    int erroffset;

    pch = strtok (key_value, "=");
    while (pch != NULL)
    {
        if (i==0) { // key
            key = (char*)malloc(strlen(pch)+1);
            strcpy(key, pch);//Found Key
        } else if(i==1) { // value
            value = (char*)pcre_compile(pch, 0, &error, &erroffset, NULL);// Found Value
        }
        pch = strtok (NULL, "=");
        i++;
    }

    if (i != 2) {
        fprintf (stderr, "Incorrect key value pair %d\n", i);
        return -1;
    } else {
        add_key_value_pair(key, value, ctx);
    }
    return 1;
}

//Clean up linklist
void cleanup_keyValue_list(void* cxt) {
    json_req_matcher_t* m;
    KVpair* current, * next;

    m = (json_req_matcher_t*)((context_t*)cxt)->m;

    if (m) {
        if (m->kv_list) {
            current = m->kv_list;
            do {
                next = current->next;
                free(current->key);
                free(current);
                current = next;
            } while(current);

        }
    }
}

//clean up the linklist
void json_cleanup(void* cxt) {
    json_req_matcher_t* m;
    if (cxt) {
        cleanup_keyValue_list(cxt);

        m = (json_req_matcher_t*)((context_t*)cxt)->m;
        free(m);
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
    m->kv_list = NULL;

    base->process_line = &json_process_line;
    //base->cleanup =&json_cleanup
    base->stop = &json_stop;
    clear_request(&request);
    return base;
}
