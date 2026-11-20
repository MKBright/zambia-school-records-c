# Offline student register.
#   make        build the program
#   make test   compile with warnings as errors, then run the checks
#   make run    copy the sample file and open it
#   make clean  delete build output

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Werror -g

.PHONY: all test run clean

all: build/schoolreg

build:
	mkdir -p build

build/schoolreg: build/main.o build/student.o build/storage.o
	$(CC) $(CFLAGS) -o $@ build/main.o build/student.o build/storage.o

build/main.o: src/main.c src/student.h src/storage.h | build
	$(CC) $(CFLAGS) -c -o $@ src/main.c

build/student.o: src/student.c src/student.h | build
	$(CC) $(CFLAGS) -c -o $@ src/student.c

build/storage.o: src/storage.c src/storage.h src/student.h | build
	$(CC) $(CFLAGS) -c -o $@ src/storage.c

build/test_register: tests/test_register.c src/student.c src/storage.c \
		src/student.h src/storage.h | build
	$(CC) $(CFLAGS) -Isrc -o $@ tests/test_register.c src/student.c src/storage.c

test: build/schoolreg build/test_register
	./build/test_register
	./tests/e2e.sh

run: build/schoolreg
	cp data/sample_learners.csv data/learners.csv
	./build/schoolreg data/learners.csv

clean:
	rm -rf build
