 # Makefile pro prekladac jazyka IFJ26*
# parser.c -- ...
# Autor: xsenkyp00 (Pavel Šenkýř)

# make        - prelozi projekt (vysledek je ./ifj26)
# make test   - spusti testy ze slozky tests/
# make clean  - smaze prelozene soubory

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g
TARGET = ifj26

SRC = scanner.c token.c main.c parser.c tree.c symtable.c
OBJ = $(SRC:.c=.o)
HDR = $(wildcard *.h)

.PHONY: all build check build-check test clean

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
