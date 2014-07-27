#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <time.h>

typedef struct request_t {
    char *buf;
    off_t offset;
    time_t time;
} request_t;

request_t *alloc_request();
void init_request(request_t * r);
void clear_request(request_t * r);
void free_request(request_t * r);
void add_to_request(request_t *, char *, off_t);
#endif                          //__REQUEST_H__
