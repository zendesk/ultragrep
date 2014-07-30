LUA_CFLAGS   = $(shell pkg-config --cflags --silence-errors lua)
LUA_CFLAGS  += $(shell pkg-config --cflags --silence-errors lua-5.2)
LUA_LDFLAGS  = $(shell pkg-config --libs --silence-errors lua)
LUA_LDFLAGS += $(shell pkg-config --libs --silence-errors lua-5.2)
CFLAGS=-O3 -g  $(LUA_CFLAGS)
LDFLAGS=$(LUA_LDFLAGS) -lpcre
#all: ug_guts ug_build_index ug_cat 
all: ug_guts ug_cat 
install: all

#request.o: request.c request.h
#	gcc ${CFLAGS} -c request.c

#rails_req.o: rails_req.c rails_req.h req_matcher.h
#	gcc ${CFLAGS} -c rails_req.c

#json_req.o: json_req.c json_req.h req_matcher.h
#	gcc ${CFLAGS} -c json_req.c

#work_req.o: work_req.c work_req.h req_matcher.h
#	gcc ${CFLAGS} -c work_req.c

#ug_guts.o: ug_guts.c 
#	gcc ${CFLAGS} -c ug_guts.c

ug_guts: ug_guts.o lua.o Makefile
	gcc ${LDFLAGS} -o ug_guts ug_guts.o lua.o 

#ug_index.o: ug_index.h ug_index.c
#	gcc ${CFLAGS} -c ug_index.c

#ug_build_index.o: ug_build_index.c req_matcher.h ug_index.h
#	gcc ${CFLAGS} -c ug_build_index.c

#zran.o: zran.c
#  gcc -g -c zran.c


#ug_build_index: ug_build_index.o ug_index.o rails_req.o json_req.o work_req.o request.o Makefile zran.o
#	gcc ${CFLAGS} -o ug_build_index request.o rails_req.o json_req.o work_req.o ug_index.o ug_build_index.o zran.o -lpcre -lz -ljansson

#ug_cat.o: ug_cat.c ug_index.h
#	gcc ${CFLAGS} -c ug_cat.c

ug_cat: ug_cat.o ug_index.o Makefile zran.o
	gcc ${LDFLAGS} -o ug_cat ug_cat.o ug_index.o zran.o -lz

#lua: lua.o
#	gcc ${CFLAGS} -o lua lua.o -llua

#ug_guts_lua: lua.o ug_guts_lua.o
#	gcc ${CFLAGS} -o ug_guts_lua ug_guts_lua.o lua.o -llua -lpcre 

clean:
	rm -rf *.o ug_guts
