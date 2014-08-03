LUA_CFLAGS   = $(shell pkg-config --cflags --silence-errors lua)
LUA_CFLAGS  += $(shell pkg-config --cflags --silence-errors lua5.2)
LUA_LDFLAGS  = $(shell pkg-config --libs --silence-errors lua)
LUA_LDFLAGS += $(shell pkg-config --libs --silence-errors lua5.2)
CFLAGS=-Wall -g  $(LUA_CFLAGS)
LDFLAGS=$(LUA_LDFLAGS) -lpcre -lsqlite3
#all: ug_guts ug_build_index ug_cat 
all: ug_guts ug_cat ug_build_index
install: all

ug_guts.o: ug_guts.c 
ug_build_index.o: ug_build_index.c

ug_guts: ug_guts.o ug_lua.o Makefile
	gcc -o ug_guts ug_guts.o ug_lua.o ${LDFLAGS}

ug_build_index: ug_build_index.o Makefile ug_gzip.o ug_lua.o ug_sqlite.o
	gcc -o ug_build_index ug_lua.o ug_build_index.o ug_sqlite.o ug_gzip.o -lz ${LDFLAGS}

ug_cat: ug_cat.o ug_sqlite.o Makefile
	gcc -o ug_cat ug_cat.o ug_sqlite.o -lz ${LDFLAGS}

clean:
	rm -rf *.o ug_guts ug_build_index ug_cat
