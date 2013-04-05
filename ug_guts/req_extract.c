#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "pcre.h"
#include "req_extract.h"


int req_extractor_init(request_t *req, int(*matcher)(char*, ssize_t, time_t*)) {
    req->state = WAITING_PATTERN;
    req->lines = 0;
    req->matcher = matcher;
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


int req_extract_each_line(char *line, ssize_t line_size, request_t* req, int (*on_req)(request_t*, void*), void *arg) {
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

    matched = req->matcher(line, line_size, &req->time);
    if(matched == MATCH_FOUND) {
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
    }
    if(matched != SKIP_LINE) {
        req->buf = realloc(req->buf, sizeof(char *) * (req->lines + 1));
        req->buf[req->lines] = line;
        req->lines++;
    }
    return(req->state);
}
