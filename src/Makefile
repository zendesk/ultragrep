LUA_CFLAGS   = $(shell pkg-config --cflags --silence-errors lua)
LUA_CFLAGS  += $(shell pkg-config --cflags --silence-errors lua5.2)
LUA_LDFLAGS  = $(shell pkg-config --libs --silence-errors lua)
LUA_LDFLAGS += $(shell pkg-config --libs --silence-errors lua5.2)
CFLAGS=-Wall -O3 -g $(LUA_CFLAGS)
LDFLAGS=$(LUA_LDFLAGS) -lpcre
all: ug_guts ug_cat
install: all

ug_guts.o: ug_guts.c
ug_index.o: ug_index.h ug_index.c
ug_build_index.o: ug_build_index.c ug_index.h

ug_guts: ug_guts.o ug_lua.o Makefile
	gcc -o ug_guts ug_guts.o ug_lua.o ${LDFLAGS}

ug_cat: ug_cat.o ug_index.o ug_build_index.o ug_gzip.o ug_lua.o Makefile
	gcc -o ug_cat ug_cat.o ug_index.o ug_build_index.o ug_gzip.o ug_lua.o -lz ${LDFLAGS}

clean:
	rm -rf *.o ug_guts ug_build_index ug_cat
