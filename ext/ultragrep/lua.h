#ifndef _UG_LUA_H
#define _UG_LUA_H

#include <lua.h>

void ug_process_line(lua_State *lua, char *line, int line_len, off_t offset);
lua_State *ug_lua_init(char *fname);

#endif
