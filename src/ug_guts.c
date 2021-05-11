// ex: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <lua.h>
#include "pcre.h"
#include "request.h"
#include "ug_lua.h"

struct ug_regexp {
  int invert;
  pcre *re;
};

typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    struct ug_regexp *regexps;
    char *lua_file;
    char *in_file;
} context_t;

static context_t ctx;

static const char* commandparams="l:s:e:k:f:";
static const char* usage ="Usage: ug_guts [-f input] -l file.lua -s start_time -e end_time regexps [... regexps]\n\n";

int parse_args(int argc, char **argv)
{
    extern char *optarg;
    extern int optind;
    const char *error;
    int erroffset, optValue=0, retValue=1, i;
    ctx.start_time = -1;
    ctx.end_time = -1;
    ctx.lua_file = NULL;

    while ((optValue = getopt(argc, argv, commandparams))!= -1) {
        switch (optValue) {
            case 'f':
                ctx.in_file = strdup(optarg);
                break;
            case 'l':
                ctx.lua_file = strdup(optarg);
                break;
            case 's':
                ctx.start_time = atol(optarg);
                break;
            case 'e':
                ctx.end_time = atol(optarg);
                break;
            case '?':
                return(-1);
                break;
            case -1:    //Options exhausted
                break;
            default:
                return(-1);
            }
    }
    if ( ctx.lua_file == NULL ||  ctx.start_time < 0 || ctx.end_time < 0 ) {	// mandatory fields
        return(-1);
    }
    else if ((optind + 1 ) > argc) { // Need at least one argument after options
        return(-1);
    }

    if (optind < argc) {	// regexps follow after command-line options
        ctx.num_regexps = argc - optind;
        ctx.regexps = malloc(sizeof(struct ug_regexp) * ctx.num_regexps);
        bzero(ctx.regexps, sizeof(struct ug_regexp) * ctx.num_regexps);

        for (i=0; optind < argc; ++optind, i++) {
            char *p = argv[optind];
            if ( p[0] == '!' || p[0] == '+' ) {
                ctx.regexps[i].invert = p[0] == '!';
                p++;
            }

            ctx.regexps[i].re = pcre_compile(p, 0, &error, &erroffset, NULL);
            if (error) {
                fprintf(stderr, "Error compiling regexp \"%s\": %s\n", argv[optind], error);
                exit(1);
            }
         }
    }
    return retValue;
}

int check_request(char *request, struct ug_regexp *regexps, int num_regexps)
{
  int j, matched, ovector[30];

  for (j = 0; j < num_regexps; j++) {
    matched = pcre_exec(regexps[j].re, NULL, request, strlen(request), 0, 0, ovector, 30);
    if ( matched < 0 && !regexps[j].invert )
        return 0;
    else if ( matched >= 0 && regexps[j].invert )
        return 0;
  }

  return 1;
}

void print_request(char *request)
{
    int i, last_line_len = 0;
    char *p;

    printf("%s", request);
    p = request + (strlen(request) - 1);

    /* skip trailing newlines */
    while ( p > request && (*p == '\n') )
      p--;

    while ( p > request && (*p != '\n') ) {
      p--;
      last_line_len++;
    }

    for (i = 0; i < (last_line_len - 1) && i < 80; i++)
        putchar('-');

    putchar('\n');
    fflush(stdout);
}


time_t max_request_time = 0;

void handle_request(request_t * req)
{

    if (!req->time)
      req->time = max_request_time;

    if ((req->time >= ctx.start_time
          && req->time <= ctx.end_time
          && check_request(req->buf, ctx.regexps, ctx.num_regexps))) {
        if (req->time != 0) {
            printf("@@%lu\n", req->time);
        }
        print_request(req->buf);
    }
    /* print a time-marker every second -- allows collections of logs with one sparse
       log to proceed */
    if (req->time > max_request_time) {
        max_request_time = req->time;
        printf("@@%lu\n", max_request_time);
    }
}



int main(int argc, char **argv)
{
    lua_State *lua;
    ssize_t line_size;
    FILE *file = NULL;
    char *line = NULL;
    size_t allocated = 0, offset = 0;
    if (argc < 5) {
        fprintf(stderr, "%s", usage);
        exit(1);
    }

    bzero(&ctx, sizeof(context_t));
    if ( parse_args(argc, argv) == -1 ) {
      fprintf(stderr, "%s", usage);
      exit(1);
    }

    lua = ug_lua_init(ctx.lua_file);
    if ( !lua )
      exit(1);

    if ( ctx.in_file ) {
      file = fopen(ctx.in_file, "r");
      if ( !file ) {
        perror(ctx.in_file);
        exit(1);
      }
    } else {
      file = stdin;
    }

    while (1) {
        line_size = getline(&line, &allocated, file);

        if ( line_size < 0 )
          break;

        if ( strncmp(line, "@@FILE:", strlen("@@FILE:")) == 0 ) {
            printf("%s", line);
            offset += line_size;
            continue;
        }

        ug_process_line(lua, line, line_size, offset);
        offset += line_size;

        if ( max_request_time > ctx.end_time)
            break;
    }
    ug_lua_on_eof(lua);
}
