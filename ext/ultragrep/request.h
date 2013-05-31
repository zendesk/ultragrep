#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <time.h>

typedef struct request_t{
    char **buf;
    int lines;
    time_t time;
    char* session;
    off_t offset;

    struct request_t* next; //for linking
    struct request_t* prev;
}request_t;

request_t* alloc_request();
void init_request(request_t* r);
void clear_request(request_t* r);
void free_request(request_t* r);
void add_to_request(request_t*, char*, off_t);
#endif //__REQUEST_H__
