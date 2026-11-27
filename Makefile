# Makefile pro prekladac jazyka IFJ26*
# MAKEFILE
# Autor: xnemecm00 (Matyas Nemecek)

# make        - prelozi projekt (vysledek je ./ifj26)
# make test   - spusti testy ze slozky tests/
# make clean  - smaze prelozene soubory

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g
TARGET = ifj26

SRC = scanner.c token.c main.c parser.c tree.c symtable.c analyzer.c codegen.c
OBJ = $(SRC:.c=.o)
HDR = $(wildcard *.h)

.PHONY: all build test clean

all: build

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

test: build
	./tests/run_tests.sh ./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
