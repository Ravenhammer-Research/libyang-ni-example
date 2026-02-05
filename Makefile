
CC = gcc

CFLAGS = -Wall -Wextra -g -I/usr/local/include
LDFLAGS = -L/usr/local/lib -Wl,-rpath,/usr/local/lib

SRC = example_ni.c
TARGET = example_ni

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "Building $@"
	@PKG_CFLAGS=$$(pkg-config --cflags libyang 2>/dev/null || echo); \
	PKG_LIBS=$$(pkg-config --libs libyang 2>/dev/null || echo -lyang); \
	$(CC) $(CFLAGS) $$PKG_CFLAGS -o $@ $< $(LDFLAGS) $$PKG_LIBS

clean:
	rm -f $(TARGET)

