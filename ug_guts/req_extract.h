#ifndef __REQ_EXTRACT_H__
#define __REQ_EXTRACT_H__

#include <time.h>

//States for request
#define WAITING_PATTERN 0
#define IN_REQ 1
#define REQ_COMPLETE 2
#define EOF_REACHED -1
#define STOP_SIGNAL -2

// Matcher returns
#define MATCH_FOUND 1
#define NO_MATCH 0
#define SKIP_LINE -1

typedef struct {
    char **buf;
    int lines;
    time_t time;
    int state;
    int(*matcher)(char*, ssize_t, time_t*);
}request_t;

#endif
