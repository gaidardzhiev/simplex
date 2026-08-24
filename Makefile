CC:=$(shell command -v musl-gcc 2>/dev/null || command -v tcc 2>/dev/null || command -v gcc 2>/dev/null)
XFLAGS=-g -Wall -Wextra
ifeq ($(findstring tcc,$(CC)),tcc)
FLAGS=$(XFLAGS)
else
FLAGS=$(XFLAGS) -static -no-pie
endif
ifeq ($(strip $(CC)),)
CC=cc
endif

BIN=simplex2elf

all: $(BIN)

$(BIN): simplex2elf.c simplex2elf.h
	$(CC) -o $@ simplex2elf.c $(FLAGS)

install:
	cp $(BIN) /usr/local/bin/$(BIN)

clean:
	rm -f $(BIN) src/*.out stage1/*.out
