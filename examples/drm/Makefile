# examples/drm/Makefile
# Сборка: make       Нужны gcc, make и libdrm-dev (на Debian: sudo apt install build-essential libdrm-dev)
CFLAGS ?= -O2 -g -Wall -Wextra
CPPFLAGS += -std=gnu11 -D_FILE_OFFSET_BITS=64 $(shell pkg-config --cflags libdrm)
LDLIBS += $(shell pkg-config --libs libdrm)
PROGS = drm-info drm-line drm-nv12 drm-atomic

all: $(PROGS)

%: %.c drm-common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@

clean:
	rm -f $(PROGS)

.PHONY: all clean
