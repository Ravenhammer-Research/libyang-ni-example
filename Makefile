CC = gcc
CFLAGS = -I/usr/local/include -I/usr/local/include/libyang -g -O2
# Add -rdynamic so backtrace_symbols can resolve symbol names
LDFLAGS = -rdynamic -L/usr/local/lib -lyang -lexecinfo -ldl -lstdc++

all: example

example: example_libyang.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f example
