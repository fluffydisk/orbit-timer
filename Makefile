CFLAGS ?= -O2 -Wall -Wextra
orbit-timer: orbit.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 $(shell pkg-config --cflags x11 cairo) $(LDFLAGS) -o $@ $< $(shell pkg-config --libs x11 cairo) -lm

check: orbit-timer
	./orbit-timer --self-test

clean:
	rm -f orbit-timer orbit-preview.png

.PHONY: check clean
