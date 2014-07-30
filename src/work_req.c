#include <stdio.h>
#include <string.h>
#include <time.h>
#include "pcre.h"
#include "request.h"
#include "req_matcher.h"

typedef struct {
    req_matcher_t base;

    on_req on_request;
    on_err on_error;
    void *arg;

    request_t *curr_req;
    request_t *top;

    int depth;                  //debug

    int stop_requested;
} work_req_matcher_t;

static void on_request(work_req_matcher_t * m, request_t * r)
{
    if (r) {
        if (r->lines > 0 && m->on_request) {
            m->on_request(r, m->arg);
        }
        //disconnect
        if (r->next) {
            r->next->prev = r->prev;
        }
        if (r->prev) {
            r->prev->next = r->next;
        } else {
            m->top = r->next;
        }

        free_request(r);
        m->depth--;
    }
}

static void on_all_requests(work_req_matcher_t * m)
{
    request_t *r = m->top;
    while (r) {
        on_request(m, r);
        r = m->top;
    }
}

static void work_stop(req_matcher_t * base)
{
    work_req_matcher_t *m = (work_req_matcher_t *) base;
    m->stop_requested = 1;
}

static char *extract_session(char *line, ssize_t line_size)
{
    int matched = 0;
    int ovector[30];
    char *session_buf;
    const char *error;
    int erroffset;
    static pcre *regex = NULL;
    if (regex == NULL) {
        regex = pcre_compile("\"(\\w{6}:\\w{6})\"", 0, &error, &erroffset, NULL);
    }
    matched = pcre_exec(regex, NULL, line, line_size, 0, 0, ovector, 30);
    if (matched > 0) {
        pcre_get_substring(line, ovector, matched, 1, (const char **) &session_buf);
        return (session_buf);
    }
    return NULL;
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

    if (regex == NULL) {
        regex = pcre_compile("\"(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})\"", 0, &error, &erroffset, NULL);
    }
    matched = pcre_exec(regex, NULL, line, line_size, 0, 0, ovector, 30);
    if (matched > 0) {
        pcre_get_substring(line, ovector, matched, 1, (const char **) &date_buf);
        strptime(date_buf, "%Y-%m-%d %H:%M:%S", &request_tm);
        free(date_buf);

        *time = mktime(&request_tm);
        return (1);
    }
    return (-1);
}

static int detect_end(char *line, ssize_t line_size)
{
    int matched = 0;
    int ovector[30];
    char *session_buf;
    const char *error;
    int erroffset;
    static pcre *regex = NULL;
    if (regex == NULL) {
        regex = pcre_compile("\"Finished this session\"", 0, &error, &erroffset, NULL);
    }
    matched = pcre_exec(regex, NULL, line, line_size, 0, 0, ovector, 30);
    return matched;
}

static int session_match(request_t * r, char *s)
{
    if (strcmp(r->session, s) == 0) {
        return 1;
    }
    return 0;
}

static int work_process_line(req_matcher_t * base, char *line, ssize_t line_size, off_t offset)
{
    work_req_matcher_t *m = (work_req_matcher_t *) base;
    char *session_str;
    int matched = 0;
    request_t *r;

    if ((m->stop_requested) || (line_size == -1)) {
        on_all_requests(m);
        return ((m->stop_requested) ? STOP_SIGNAL : EOF_REACHED);
    }

    session_str = extract_session(line, line_size);

    r = m->top;
    if (session_str != NULL) {
        if (r && r->next == NULL && r->session == NULL) {
            //The only req we have is sessionless
            on_request(m, r);
            r = NULL;
            //Finish and start afresh
        }
        //Find the correct req
        while (r && !session_match(r, session_str)) {
            r = r->next;
        }
    }                           //else it goes on to the top

    if (!r) {
        r = alloc_request();
        //This is now new top request
        if (m->top) {
            r->next = m->top;
            m->top->prev = r;
        }
        m->top = r;
        r->session = session_str;

        m->depth++;
    } else {
        free(session_str);
    }

    add_to_request(r, line, offset);

    if (r->time == 0) {
        parse_req_time(line, line_size, &(r->time));
    }

    if (r->session != NULL) {
        matched = detect_end(line, line_size);
        if (matched > 0) {
            on_request(m, r);
        }
    }

    return (0);
}

req_matcher_t *work_req_matcher(on_req fn1, on_err fn2, void *arg)
{
    work_req_matcher_t *m = (work_req_matcher_t *) malloc(sizeof(work_req_matcher_t));
    req_matcher_t *base = (req_matcher_t *) m;

    m->on_request = fn1;
    m->on_error = fn2;
    m->arg = arg;

    m->stop_requested = 0;
    m->curr_req = NULL;

    base->process_line = &work_process_line;
    base->stop = &work_stop;

    return base;
}
