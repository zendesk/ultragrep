#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "request.h"
#include "lua.h"
 
int ug_lua_request_add(lua_State *lua);

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
	return lua;
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
    strptime(timestring, "%Y-%m-%d %H:%M:%S", &request_tm);
    r.time = timegm(&request_tm);
  }

  r.offset = luaL_checknumber(lua, 3);
  handle_request(&r);

  return 1;
}

void ug_process_line(lua_State *lua, char *line, off_t offset) {
  lua_getglobal(lua, "process_line");
  lua_pushstring(lua, line);
  lua_pushnumber(lua, (lua_Number)offset);
  lua_call(lua, 2, 0);    
}

