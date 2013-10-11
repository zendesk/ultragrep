#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request.h"

request_t *alloc_request()
{
    request_t *r = (request_t *) calloc(1, sizeof(request_t));
    return (r);
}

void init_request(request_t * r)
{
    memset(r, 0, sizeof(request_t));
    r->offset = -1;
}

void clear_request(request_t * r)
{
    int i = 0;

    for (i = 0; i < r->lines; i++)
        free(r->buf[i]);

    if (r->buf)
        free(r->buf);
    if (r->session)
        free(r->session);

    init_request(r);
}

void free_request(request_t * r)
{
    clear_request(r);
    free(r);
}

void add_to_request(request_t * req, char *line, off_t offset)
{
    if (req->offset == -1)
        req->offset = offset;

    req->buf = realloc(req->buf, sizeof(char *) * (req->lines + 1));
    req->buf[req->lines] = line;
    req->lines++;
}
