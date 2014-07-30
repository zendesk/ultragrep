#ifndef _UG_LUA_H
#define _UG_LUA_H

#include <lua.h>

void ug_process_line(lua_State *lua, char *line, int line_len, off_t offset);
void ug_lua_on_eof(lua_State *lua);
lua_State *ug_lua_init(char *fname);

#endif
