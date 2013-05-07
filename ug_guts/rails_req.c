#include <stdio.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"

typedef struct {
    req_matcher_t base;
    on_req on_request;
    on_err on_error;
    void* arg;
    request_t* curr_req;
    int stop_requested;
    int blank_lines;

}rails_req_matcher_t;
static request_t request;


static void rails_on_request(rails_req_matcher_t* m) {
    if(m->curr_req && m->on_request) {
        if(m->curr_req->lines > 0) {
            m->on_request(m->curr_req, m->arg);
        }
        clear_request(m->curr_req);
    }
}

void rails_stop(req_matcher_t* base) {
    rails_req_matcher_t* m = (rails_req_matcher_t*)base;
    m->stop_requested = 1;
}

static int parse_req_time(char* line, ssize_t line_size, time_t* time) {
    int matched = 0;
    int ovector[30];
    char *date_buf;
    struct tm request_tm;
    time_t tv;
    const char* error;
    int erroffset;
    static pcre* regex = NULL;

    if(regex == NULL) {
        regex = pcre_compile("^Processing.*(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})", 0, &error, &erroffset, NULL);
    }
    matched = pcre_exec(regex, NULL, line, line_size,0,0,ovector, 30);
    if(matched > 0) {
        pcre_get_substring(line, ovector, matched, 1, (const char **)&date_buf);
        strptime(date_buf, "%Y-%m-%d %H:%M:%S", &request_tm);
        free(date_buf);

        *time = mktime(&request_tm);
        return(1);
    }
    return(-1);
}

static int rails_process_line(req_matcher_t* base, char *line, ssize_t line_size) {
    rails_req_matcher_t* m = (rails_req_matcher_t*)base;

    if((m->stop_requested) || (line_size == -1)) {
        rails_on_request(m);
        return((m->stop_requested)?STOP_SIGNAL:EOF_REACHED);
    }

    if(line_size == 1) { //blank line
        m->blank_lines += 1;
        return(0);
    }

    if(m->blank_lines >= 2) {
        m->blank_lines = 0;
        rails_on_request(m);
    }

    add_to_request(m->curr_req, line);

    if(m->curr_req->time == 0) {
        parse_req_time(line, line_size, &(m->curr_req->time));
    }

    return(0);
}

req_matcher_t* rails_req_matcher(on_req fn1, on_err fn2, void* arg) {
    rails_req_matcher_t* m = (rails_req_matcher_t*)malloc(sizeof(rails_req_matcher_t));
    req_matcher_t* base = (req_matcher_t*)m;

    m->on_request = fn1;
    m->on_error = fn2;
    m->arg = arg;

    m->stop_requested = 0;
    m->blank_lines = 0;
    m->curr_req = &request;

    base->process_line = &rails_process_line;
    base->stop = &rails_stop;
    return base;
}
