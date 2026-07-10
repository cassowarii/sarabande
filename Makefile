CC=gcc
CFLAGS=-Wall -g -Og -DDEBUG -fsanitize=undefined
#CFLAGS=-Wall -O3 -flto
LIBS=-lm

SRC := src
OBJ := obj
BUILDDIR := build

SOURCES := $(shell find $(SRC) -type f -name '*.c')
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

build/a.out: $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRC) $(OBJECTS) $(LIBS) -o $@

.PHONY: parsedebug optimized dev profile
parsedebug: CFLAGS += -DPARSEDEBUG
parsedebug: clean build/a.out

optimized: CFLAGS=-Wall -O3 -flto
optimized: clean build/a.out

profile: CFLAGS=-Wall -O3 -flto -pg
profile: clean build/a.out

dev: clean build/a.out

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRC) -c $< -o $@

clean:
	rm -rf $(OBJ) $(BUILDDIR)
