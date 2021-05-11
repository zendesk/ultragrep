#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "request.h"
#include "lua.h"

int ug_lua_request_add(lua_State *lua);


void error (lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  vfprintf(stderr, fmt, argp);
  va_end(argp);
  lua_close(L);
  exit(EXIT_FAILURE);
}

static char *strptime_format = NULL;

lua_State *ug_lua_init(char *fname) {
	lua_State *lua = luaL_newstate();
	luaL_openlibs(lua);

	static const struct luaL_Reg ug_request_lib[] = {
		{"add", ug_lua_request_add},
		{NULL, NULL}};

  luaL_newlib(lua, ug_request_lib );
  lua_setglobal(lua, "ug_request");

  if ( luaL_loadfile(lua, fname) != LUA_OK ) {
    fprintf(stderr, "Couldn't load '%s'\n", fname);
    fprintf(stderr, "%s\n", lua_tostring(lua, -1));
    return NULL;
  }
  lua_call(lua, 0, 0);

  lua_getglobal(lua, "process_line");
  if ( lua_isnil(lua, -1) ) {
    fprintf(stderr, "expected %s to define 'process_line' function\n", fname);
    return NULL;
  }
  lua_pop(lua, 1);

  lua_getglobal(lua, "strptime_format");
  if ( lua_isnil(lua, -1) ) {
    fprintf(stderr, "expected %s to define 'strptime_format' string\n", fname);
    return NULL;
  }
  strptime_format = strdup(luaL_checkstring(lua, -1));
	return lua;
}

#define	TM_YEAR_BASE	1900
#define	EPOCH_YEAR	1970


static time_t
sub_mkgmt(struct tm *tm)
{
	int y, nleapdays;
	time_t t;
	/* days before the month */
	static const unsigned short moff[12] = {
		0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};

	/*
	 * XXX: This code assumes the given time to be normalized.
	 * Normalizing here is impossible in case the given time is a leap
	 * second but the local time library is ignorant of leap seconds.
	 */

	/* minimal sanity checking not to access outside of the array */
	if ((unsigned) tm->tm_mon >= 12)
		return (time_t) -1;
	if (tm->tm_year < EPOCH_YEAR - TM_YEAR_BASE)
		return (time_t) -1;

	y = tm->tm_year + TM_YEAR_BASE - (tm->tm_mon < 2);
	nleapdays = y / 4 - y / 100 + y / 400 -
	    ((EPOCH_YEAR-1) / 4 - (EPOCH_YEAR-1) / 100 + (EPOCH_YEAR-1) / 400);
	t = ((((time_t) (tm->tm_year - (EPOCH_YEAR - TM_YEAR_BASE)) * 365 +
			moff[tm->tm_mon] + tm->tm_mday - 1 + nleapdays) * 24 +
		tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;

	return (t < 0 ? (time_t) -1 : t);
}

int ug_lua_request_add(lua_State *lua) {
  struct tm request_tm;
  const char *timestring;
  request_t r;

  r.buf = (char *)luaL_checkstring(lua, 1);

  if ( lua_isnil(lua, 2) ) {
    r.time = 0;
  } else {
    timestring = luaL_checkstring(lua, 2);
    strptime(timestring, strptime_format, &request_tm);
    r.time = sub_mkgmt(&request_tm);
  }

  r.offset = luaL_checknumber(lua, 3);
  handle_request(&r);

  return 1;
}

void ug_process_line(lua_State *lua, char *line, int line_len, off_t offset) {
  lua_getglobal(lua, "process_line");
  lua_pushlstring(lua, line, line_len);
  lua_pushnumber(lua, (lua_Number)offset);

  if ( lua_pcall(lua, 2, 0, 0) != 0 ) {
    error(lua, "error running function `f': %s", lua_tostring(lua, -1));
  }
}

void ug_lua_on_eof(lua_State *lua) {
  lua_getglobal(lua, "on_eof");
  if ( !lua_isnil(lua, -1) ) {
    lua_call(lua, 0, 0);
  }
}


