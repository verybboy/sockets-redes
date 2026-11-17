CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: servidor

servidor: src/servidor.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f servidor
