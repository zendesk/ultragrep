#include <stdint.h>
#include <stdio.h>
#include <lua.h>
#include <time.h>
#include <sqlite3.h>
#define INDEX_EVERY 10

typedef struct {
    time_t last_index_time;
    FILE *flog;
    lua_State *lua;
    sqlite3 *db;
} build_idx_context_t;

