#include <stdint.h>
#include <stdio.h>
#include <sqlite3.h>
#include <lua.h>
#include <time.h>
#define INDEX_EVERY 10

struct ug_index {
    uint64_t time;
    uint64_t offset;
};

typedef struct {
    time_t last_index_time;
    FILE *flog;
    FILE *findex;
    FILE *fgzindex;
    lua_State *lua;
    sqlite3 *db;
} build_idx_context_t;

void ug_write_index(FILE * file, uint64_t time, uint64_t offset);
int ug_get_last_index_entry(FILE * file, struct ug_index *idx);
off_t ug_get_offset_for_timestamp(FILE * findex, uint64_t time);
char *ug_get_index_fname(char *log_fname, char *ext);
