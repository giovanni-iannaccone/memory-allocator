CC = gcc
CFLAGS = -g -Wall

SRC := src
INCLUDE := include
BIN := bin

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

.PHONY: clean create-build-folder

all: create-build-folder $(BIN)



create-build-folder:
	@mkdir -p $(BUILD) $(OBJ)

clean:
	@rm -rf $(BUILD)