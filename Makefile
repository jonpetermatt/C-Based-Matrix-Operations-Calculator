CC = clang
CFLAGS = -O1 -Wall -std=gnu11 -march=native
LDFLAGS = -pthread

.PHONY: all clean

all: matrix

matrix: ./src/main.c ./src/matrix.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	-rm -f *.o
	-rm -f matrix
	-rm -rf *.dSYM
