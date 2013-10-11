#include <stdio.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"

typedef struct {
    req_matcher_t base;
    on_req on_request;
    on_err on_error;
    void *arg;
    int stop_requested;
    int blank_lines;

} rails_req_matcher_t;
static request_t request;

static void rails_on_request(rails_req_matcher_t * m, request_t * r)
{
    if (r && m->on_request) {
        if (r->lines > 0) {
            m->on_request(r, m->arg);
        }
        clear_request(r);
    }
}

void rails_stop(req_matcher_t * base)
{
    rails_req_matcher_t *m = (rails_req_matcher_t *) base;
    m->stop_requested = 1;
}

static int parse_req_time(char *line, ssize_t line_size, time_t * time)
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

static int rails_process_line(req_matcher_t * base, char *line, ssize_t line_size, off_t offset)
{
    rails_req_matcher_t *m = (rails_req_matcher_t *) base;

    if ((m->stop_requested) || (line_size == -1)) {
        rails_on_request(m, &request);
        return ((m->stop_requested) ? STOP_SIGNAL : EOF_REACHED);
    }

    if (line_size == 1) {       //blank line
        m->blank_lines += 1;
        return (0);
    }

    if (m->blank_lines >= 2) {
        m->blank_lines = 0;
        rails_on_request(m, &request);
    }

    add_to_request(&request, line, offset);

    if (request.time == 0) {
        parse_req_time(line, line_size, &(request.time));
    }

    return (0);
}

req_matcher_t *rails_req_matcher(on_req fn1, on_err fn2, void *arg)
{
    rails_req_matcher_t *m = (rails_req_matcher_t *) malloc(sizeof(rails_req_matcher_t));
    req_matcher_t *base = (req_matcher_t *) m;

    m->on_request = fn1;
    m->on_error = fn2;
    m->arg = arg;

    m->stop_requested = 0;
    m->blank_lines = 0;

    base->process_line = &rails_process_line;
    base->stop = &rails_stop;
    clear_request(&request);
    return base;
}
