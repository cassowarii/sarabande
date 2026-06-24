CC=gcc
CFLAGS=-Wall -g -DDEBUG -fsanitize=undefined
#CFLAGS=-Wall -O3 -flto -DGLEW_STATIC -DDEBUG -fsanitize=undefined
LIBS=

SRC := src
OBJ := obj
BUILDDIR := build

SOURCES := $(shell find $(SRC) -type f -name '*.c')
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

build/a.out: $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRC) $(OBJECTS) $(LIBS) -o $@

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LIBS) -I$(SRC) -c $< -o $@

clean:
	rm -rf $(OBJ) $(BUILDDIR)
