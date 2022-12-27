CC := clang
TARGET := libsmusher-standalone

SRC := src
OBJ := build

SOURCES := $(wildcard $(SRC)/*.c $(SRC)/*/*.c) #$(SRC)/Cog/lex.yy.c $(SRC)/Cog/y.tab.c
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))
ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

# Flags
LDFLAGS := $(pkg-config --libs sdl2) $(pkg-config --cflags sdl2-mixer) -fshort-wchar #-lGL -lGLEW -lopenal -lalut
CFLAGS := -O2 -I$(ROOT_DIR)/$(SRC) $(pkg-config --cflags sdl2) -Wuninitialized -fshort-wchar -Wall -Wno-unused-variable -Wno-parentheses -Wno-missing-braces 

LIBSMUSHER_NO_ASAN ?= 0

ifneq ($(LIBSMUSHER_NO_ASAN), 1)
	CFLAGS += -fsanitize=address -fsanitize=float-divide-by-zero
	LDFLAGS += -fsanitize=address -fsanitize=float-divide-by-zero -static-libsan
endif

all: $(TARGET)

.PHONY: initial all

clean:
	rm -rf $(OBJ)
	rm -f $(TARGET)

$(OBJ):
	@mkdir -p $(OBJ)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ) initial
	@mkdir -p $(dir $@)
	$(CC) -c -g -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ -g $^ $(LDFLAGS)
